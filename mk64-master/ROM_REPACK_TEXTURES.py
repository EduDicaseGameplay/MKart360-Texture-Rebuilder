#!/usr/bin/env python3
"""Safe ROM-native texture repacker for the MK64 extraction manifests.

First stage: MIO0 (RGBA16/I4/IA4/I8/IA8) and TKMK00.
MIO0 uses a cost-aware parser with a bounded search and automatically retries
near-limit blocks with a wider match search; a legacy greedy encoder is kept
as a non-regression fallback.
It never relocates data or changes pointers. By default it only performs a
validation/dry-run. --patch writes a copy of the ROM only when every selected
replacement fits its original compressed allocation and round-trips correctly.
"""
from __future__ import annotations
import argparse, json, struct, shutil, hashlib, sys, tempfile, subprocess
from pathlib import Path
from collections import defaultdict
from PIL import Image

HERE=Path(__file__).resolve().parent
TKMOD=HERE/'tkmk00_rom_tool.py'


def fnv1a(b):
    h=2166136261
    for x in b: h=((h^x)*16777619)&0xffffffff
    return h

def mio0_decode(src):
    if src[:4]!=b'MIO0': raise ValueError('not MIO0')
    out_size=int.from_bytes(src[4:8],'big'); cp=int.from_bytes(src[8:12],'big'); rp=int.from_bytes(src[12:16],'big')
    ctrl_pos=16; mask=0; ctrl=0; out=bytearray()
    while len(out)<out_size:
        if mask==0:
            ctrl=src[ctrl_pos];ctrl_pos+=1;mask=0x80
        if ctrl&mask:
            out.append(src[rp]);rp+=1
        else:
            w=int.from_bytes(src[cp:cp+2],'big');cp+=2
            n=(w>>12)+3; d=(w&0xfff)+1; start=len(out)-d
            for i in range(n): out.append(out[start+i])
        mask>>=1
    return bytes(out[:out_size]), max(ctrl_pos,cp,rp)

def _mio0_matches(data, pos, max_candidates=96):
    """Yield useful MIO0 matches at *pos*.

    MIO0 permits a 12-bit distance (1..4096) and a 4-bit length field
    representing 3..18 bytes.  We index by the first three bytes and inspect
    only recent candidates.  The compressor is still deterministic, but the
    final token choice is made by dynamic programming below rather than by a
    greedy longest-match decision.
    """
    n = len(data)
    lim = min(18, n - pos)
    if lim < 3:
        return ()

    # Build lazily once per call from a cache attached to the function.
    index = getattr(_mio0_matches, "_index", None)
    if index is None or getattr(_mio0_matches, "_data_id", None) != id(data):
        index = {}
        for i in range(max(0, n - 2)):
            key = data[i:i+3]
            index.setdefault(key, []).append(i)
        _mio0_matches._index = index
        _mio0_matches._data_id = id(data)

    key = data[pos:pos+3]
    candidates = index.get(key, ())
    out = []
    seen = 0
    for j in reversed(candidates):
        if j >= pos:
            continue
        dist = pos - j
        if dist > 0x1000:
            break
        seen += 1
        k = 3
        # Overlapping/self-referential matches are legal in MIO0.
        while k < lim:
            if data[j + k if j + k < pos else pos + (k - dist)] != data[pos + k]:
                break
            k += 1
        if k >= 3:
            out.append((k, dist))
            if k == lim:
                break
        if seen >= max_candidates:
            break
    return out


def mio0_encode(data, max_candidates=128):
    """Encode bytes as MIO0 using a cost-aware dynamic-programming parser.

    Unlike the old greedy encoder, this accounts for the 8-token control-byte
    groups.  A slightly shorter match can be cheaper when it changes token
    alignment and enables a better match immediately afterwards.  The output
    always round-trips through ``mio0_decode`` before it is accepted by the
    repacker.
    """
    data = bytes(data)
    n = len(data)
    if n == 0:
        header = b'MIO0' + struct.pack('>I', 0) + struct.pack('>II', 16, 16)
        return header

    # For each position keep the best (lowest payload + control-byte) cost for
    # each token-group phase.  State is (position, phase), phase 0 means the
    # next token starts a new control byte.  At every token we pay one control
    # byte only when phase == 0, plus 1 byte for a literal or 2 for a match.
    INF = 10**18
    best = [[INF] * 8 for _ in range(n + 1)]
    prev = [[None] * 8 for _ in range(n + 1)]
    best[0][0] = 0

    # Rebuild the 3-byte index locally so separate textures do not retain a
    # reference to large previous buffers.
    index = {}
    for i in range(max(0, n - 2)):
        index.setdefault(data[i:i+3], []).append(i)

    def matches_at(pos):
        lim = min(18, n - pos)
        if lim < 3:
            return []
        candidates = index.get(data[pos:pos+3], ())
        out = []
        seen = 0
        for j in reversed(candidates):
            if j >= pos:
                continue
            dist = pos - j
            if dist > 0x1000:
                break
            k = 3
            while k < lim:
                # Overlap is intentional: decoder copies from its growing
                # output, so j+k >= pos maps to the corresponding earlier
                # output byte at pos+k-dist.
                src = j + k
                if src >= pos:
                    src = pos + (k - dist)
                if data[src] != data[pos + k]:
                    break
                k += 1
            if k >= 3:
                out.append((k, dist))
                if k == lim:
                    break
            seen += 1
            if seen >= max_candidates:
                break
        # Every prefix length of a valid match is itself a valid token, but
        # it must retain the distance of the candidate that actually matches
        # that many bytes.  Do not propagate a distance from a longer match:
        # that can be invalid for a shorter prefix when candidates differ.
        result = []
        for length, dist in out:
            result.extend((l, dist) for l in range(3, length + 1))
        # Same length/distance pairs are redundant.
        return sorted(set(result))

    match_cache = [None] * n
    for pos in range(n):
        if match_cache[pos] is None:
            match_cache[pos] = matches_at(pos)
        matches = match_cache[pos]
        for phase in range(8):
            cur = best[pos][phase]
            if cur == INF:
                continue
            control_cost = 1 if phase == 0 else 0
            next_phase = (phase + 1) & 7

            # Literal token.
            nc = cur + control_cost + 1
            if nc < best[pos + 1][next_phase]:
                best[pos + 1][next_phase] = nc
                prev[pos + 1][next_phase] = (pos, phase, 1, 0)

            # Back-reference tokens.
            for length, dist in matches:
                end = pos + length
                nc = cur + control_cost + 2
                if nc < best[end][next_phase]:
                    best[end][next_phase] = nc
                    prev[end][next_phase] = (pos, phase, length, dist)

    # Choose the cheapest final phase. Ties prefer the lower phase for stable
    # output; all phases already include their complete control-byte cost.
    end_phase = min(range(8), key=lambda ph: (best[n][ph], ph))
    if best[n][end_phase] == INF:
        raise ValueError('MIO0 encoder failed to find a parse')

    tokens = []
    pos, phase = n, end_phase
    while pos:
        item = prev[pos][phase]
        if item is None:
            raise ValueError('MIO0 encoder backtracking failure')
        ppos, pphase, length, dist = item
        tokens.append((length, dist))
        pos, phase = ppos, pphase
    tokens.reverse()

    ctrl = bytearray()
    comp = bytearray()
    raw = bytearray()
    ti = 0
    while ti < len(tokens):
        group = tokens[ti:ti+8]
        c = 0
        for length, dist in group:
            c <<= 1
            if length == 1:
                c |= 1
                # Literal byte position is reconstructed from the already
                # emitted token lengths.
            else:
                d = dist - 1
                ln = length - 3
                comp.extend(struct.pack('>H', (ln << 12) | d))
        # Emit raw bytes in token order. Recompute source position from the
        # tokens because the compressed and raw streams are separate.
        ctrl.append(c << (8 - len(group)))
        ti += len(group)

    # Rebuild raw stream in a second pass; this keeps the encoder simple and
    # avoids storing source positions in every token.
    raw.clear()
    pos = 0
    for length, dist in tokens:
        if length == 1:
            raw.append(data[pos])
            pos += 1
        else:
            pos += length

    comp_pos = 16 + len(ctrl)
    raw_pos = comp_pos + len(comp)
    candidate = (b'MIO0' + struct.pack('>I', n) + struct.pack('>II', comp_pos, raw_pos)
                + bytes(ctrl) + bytes(comp) + bytes(raw))
    # Safety net: the previous deterministic greedy parser is a known-valid
    # baseline. If the bounded DP search misses a distant candidate, never
    # regress compression quality.
    greedy = _mio0_encode_greedy(data)
    return candidate if len(candidate) < len(greedy) else greedy


def _mio0_encode_full(data):
    """Exhaustive MIO0 parse used only for near-limit/overflow blocks.

    For each source position, inspect every legal distance in the 4 KiB
    MIO0 window and keep the longest valid match.  Every prefix of that
    longest match is also valid, so the DP can choose the exact length needed
    for token/control-byte alignment.  This removes the candidate-count
    heuristic from the final retry and is intentionally not used for the
    normal fast path.
    """
    data = bytes(data)
    n = len(data)
    if n == 0:
        return b'MIO0' + struct.pack('>I', 0) + struct.pack('>II', 16, 16)

    # longest[pos] = (maximum match length, distance).  A single longest
    # match is enough because all shorter prefixes use the same 2-byte MIO0
    # token and remain valid.
    longest = [(0, 0)] * n
    for pos in range(n - 2):
        lim = min(18, n - pos)
        start = max(0, pos - 0x1000)
        best_len = 0
        best_dist = 0
        for j in range(pos - 1, start - 1, -1):
            if data[j] != data[pos]:
                continue
            dist = pos - j
            k = 1
            while k < lim:
                src = j + k
                if src >= pos:
                    src = pos + (k - dist)
                if data[src] != data[pos + k]:
                    break
                k += 1
            if k >= 3 and k > best_len:
                best_len = k
                best_dist = dist
                if best_len == lim:
                    break
        longest[pos] = (best_len, best_dist)

    INF = 10**18
    best = [[INF] * 8 for _ in range(n + 1)]
    prev = [[None] * 8 for _ in range(n + 1)]
    best[0][0] = 0

    for pos in range(n):
        max_len, dist = longest[pos]
        for phase in range(8):
            cur = best[pos][phase]
            if cur == INF:
                continue
            control_cost = 1 if phase == 0 else 0
            next_phase = (phase + 1) & 7

            nc = cur + control_cost + 1
            if nc < best[pos + 1][next_phase]:
                best[pos + 1][next_phase] = nc
                prev[pos + 1][next_phase] = (pos, phase, 1, 0)

            if max_len >= 3:
                for length in range(3, max_len + 1):
                    end = pos + length
                    nc = cur + control_cost + 2
                    if nc < best[end][next_phase]:
                        best[end][next_phase] = nc
                        prev[end][next_phase] = (pos, phase, length, dist)

    end_phase = min(range(8), key=lambda ph: (best[n][ph], ph))
    if best[n][end_phase] == INF:
        raise ValueError('MIO0 exhaustive encoder failed to find a parse')

    tokens = []
    pos, phase = n, end_phase
    while pos:
        item = prev[pos][phase]
        if item is None:
            raise ValueError('MIO0 exhaustive backtracking failure')
        ppos, pphase, length, dist = item
        tokens.append((length, dist))
        pos, phase = ppos, pphase
    tokens.reverse()

    ctrl = bytearray()
    comp = bytearray()
    raw = bytearray()
    source_pos = 0
    for i in range(0, len(tokens), 8):
        group = tokens[i:i + 8]
        c = 0
        for length, dist in group:
            c = (c << 1) | (1 if length == 1 else 0)
            if length == 1:
                raw.append(data[source_pos])
            else:
                comp.extend(struct.pack('>H', ((length - 3) << 12) | (dist - 1)))
            source_pos += length
        ctrl.append(c << (8 - len(group)))

    cp = 16 + len(ctrl)
    rp = cp + len(comp)
    blob = (b'MIO0' + struct.pack('>I', n) + struct.pack('>II', cp, rp)
            + bytes(ctrl) + bytes(comp) + bytes(raw))
    decoded, _ = mio0_decode(blob)
    if decoded != data:
        raise ValueError('MIO0 exhaustive round-trip mismatch')
    return blob

def _mio0_encode_greedy(data):
    n=len(data); ctrl=[]; comp=bytearray(); raw=bytearray(); pos=0
    while pos<n:
        group=[]
        for _ in range(8):
            if pos>=n: break
            best_len=0; best_dist=0
            start=max(0,pos-0x1000)
            for j in range(pos-1,start-1,-1):
                if data[j]!=data[pos]: continue
                lim=min(18,n-pos); k=1
                while k<lim and data[j+k if j+k<pos else j+k]==data[pos+k] and (j+k)<pos+0x1000:
                    k+=1
                    if j+k>=pos and j+k>=n: break
                if k>=3 and k>best_len:
                    best_len=k;best_dist=pos-j
                    if k==lim: break
            if best_len>=3:
                comp.extend(struct.pack('>H',((best_len-3)<<12)|(best_dist-1)));group.append(0);pos+=best_len
            else:
                raw.append(data[pos]);group.append(1);pos+=1
        c=0
        for bit in group:c=(c<<1)|bit
        ctrl.append(c<<(8-len(group)))
    cp=16+len(ctrl);rp=cp+len(comp)
    return b'MIO0'+struct.pack('>I',n)+struct.pack('>II',cp,rp)+bytes(ctrl)+bytes(comp)+bytes(raw)


def rgba16(r,g,b,a):
    # N64 RGBA16: 5/5/5/1, big endian.
    return ((r>>3)<<11)|((g>>3)<<6)|((b>>3)<<1)|(1 if a>=128 else 0)

def _x5to8(v):
    return (v << 3) | (v >> 2)

def decode_rgba16_texel(col16):
    return (_x5to8((col16>>11)&0x1f), _x5to8((col16>>6)&0x1f),
            _x5to8((col16>>1)&0x1f), 255 if (col16&1) else 0)

def decode_rgba16_palette(raw):
    if len(raw) % 2:
        raise ValueError('TLUT RGBA16 com tamanho impar')
    pal=[]
    for i in range(0,len(raw),2):
        c=(raw[i]<<8)|raw[i+1]
        pal.append((_x5to8((c>>11)&0x1f), _x5to8((c>>6)&0x1f),
                    _x5to8((c>>1)&0x1f), 255 if (c&1) else 0))
    return pal

def _parse_c_array_bytes(text):
    """Extract bytes from a C array body containing 0xNN/0xNNNN literals."""
    vals=[]
    for token in __import__('re').findall(r'0x([0-9A-Fa-f]{1,4})', text):
        v=int(token,16)
        vals.append(v)
    # Source TLUTs are normally written as u8 byte arrays. For 16-bit literals,
    # preserve big-endian order so RGBA16 decoding remains faithful.
    out=bytearray()
    for token in __import__('re').findall(r'0x([0-9A-Fa-f]{1,4})', text):
        v=int(token,16)
        if len(token)<=2: out.append(v)
        else: out.extend(v.to_bytes(2,'big'))
    return bytes(out)

def load_source_tluts(root):
    """Find TLUT byte arrays and texture->TLUT associations in project C/INC sources."""
    import re
    arrays={}
    sources=[]
    for base in (root/'assets',root/'courses',root/'src'):
        if not base.is_dir(): continue
        sources.extend(p for p in base.rglob('*') if p.suffix.lower() in ('.c','.h','.inc'))
    texts=[]
    for p in sources:
        try:texts.append((p,p.read_text(encoding='utf-8',errors='ignore')))
        except OSError:pass
    # Direct literal arrays and arrays whose body includes another .inc file.
    for p,text in texts:
        for m in re.finditer(r'\b(?:u8|unsigned\s+char)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\]\s*=\s*\{(.*?)\};',text,re.S):
            arrays[m.group(1)]=_parse_c_array_bytes(m.group(2))
        for m in re.finditer(r'\b(?:u8|unsigned\s+char)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\]\s*=\s*\{\s*#include\s+"([^"]+)"',text,re.S):
            inc=(root/m.group(2)).resolve()
            if not inc.is_file(): inc=(p.parent/m.group(2)).resolve()
            if inc.is_file():
                try: arrays[m.group(1)]=_parse_c_array_bytes(inc.read_text(encoding='utf-8',errors='ignore'))
                except OSError: pass
    tex_tlut={}
    init_re=re.compile(r'\binit_texture_object\s*\(\s*[^,]+,\s*(?P<tlut>[^,]+),\s*(?:\([^)]*\)\s*)?(?P<tex>[A-Za-z_][A-Za-z0-9_]*)\s*,',re.I)
    for p,text in texts:
        for m in init_re.finditer(text):
            tlut=m.group('tlut').strip()
            tex=m.group('tex')
            # Casts can leave a leading parenthesis; strip common source syntax.
            tlut=re.sub(r'^\([^)]*\)\s*','',tlut).strip()
            if tlut and tlut not in ('NULL','0'):
                tex_tlut[tex]=tlut
    return arrays,tex_tlut

def load_ci_palettes(root, rom):
    """Load CI palettes from assets JSON and, when needed, source TLUT arrays."""
    import re
    records={}
    for p in root.glob('assets/**/*.json'):
        try:d=json.loads(p.read_text(encoding='utf-8'))
        except Exception:continue
        if not isinstance(d,dict):continue
        for sym,rec in d.items():
            if isinstance(rec,dict) and str(rec.get('type','')).lower()=='rgba16':
                records[sym]=rec
    source_arrays,source_tex_tlut=load_source_tluts(root)
    palettes={}
    for sym,rec in records.items():
        try:
            off=int(str(rec['rom_offset']),0)
            block=int(str(rec.get('block_offset',0)),0)
            w=int(rec.get('width',16)); h=int(rec.get('height',16)); size=w*h*2
            raw_region=rom[off:]
            if raw_region[:4]==b'MIO0':
                raw,_=mio0_decode(raw_region)
                part=raw[block:block+size]
            else:
                part=raw_region[block:block+size]
            if len(part)==size: palettes[sym]=decode_rgba16_palette(part)
        except Exception:continue
    for sym,raw in source_arrays.items():
        if len(raw)>=32 and len(raw)%2==0 and sym not in palettes:
            palettes[sym]=decode_rgba16_palette(raw[:512])
    return palettes,source_tex_tlut

def load_texture_tlut_map(root):
    """Return symbol -> TLUT symbol from assets JSON plus source init_texture_object calls."""
    out={}
    for p in root.glob('assets/**/*.json'):
        try:d=json.loads(p.read_text(encoding='utf-8'))
        except Exception:continue
        if not isinstance(d,dict):continue
        for sym,rec in d.items():
            if isinstance(rec,dict) and isinstance(rec.get('tlut'),str): out[sym]=rec['tlut']
    _arrays,source_map=load_source_tluts(root)
    out.update(source_map)
    return out

def _ci_raw_matches_image(img, fmt, palette, original_raw):
    """Check whether a CI PNG is the same visual texture as original CI bytes."""
    if original_raw is None or not palette:
        return False
    px=list(img.convert('RGBA').getdata())
    if fmt=='ci8':
        if len(original_raw)!=len(px): return False
        for i,c in enumerate(px):
            pc=palette[original_raw[i]]
            if tuple(c)==tuple(pc): continue
            if c[3]==0 and tuple(c[:3])==tuple(pc[:3]): continue
            return False
        return True
    if fmt=='ci4':
        if len(original_raw)!=(len(px)+1)//2: return False
        for i,c in enumerate(px):
            b=original_raw[i//2]
            idx=(b>>4)&0xF if i%2==0 else b&0xF
            pc=palette[idx]
            if tuple(c)==tuple(pc): continue
            if c[3]==0 and tuple(c[:3])==tuple(pc[:3]): continue
            return False
        return True
    return False

def image_bytes(img,fmt,palette=None,original_raw=None):
    img=img.convert('RGBA'); px=list(img.getdata())
    if fmt=='rgba16':
        out=bytearray()
        for r,g,b,a in px: out+=struct.pack('>H',rgba16(r,g,b,a))
        return bytes(out)
    if fmt=='rgba32':
        return bytes(x for p in px for x in p)
    if fmt=='i8': return bytes((round((.299*r+.587*g+.114*b)) for r,g,b,a in px))
    if fmt=='ia16': return bytes(v for r,g,b,a in px for v in (round(.299*r+.587*g+.114*b), a))
    if fmt=='ia8':
        vals=[((round(.299*r+.587*g+.114*b)>>4)<<4)|(a>>4) for r,g,b,a in px]
        return bytes(vals)
    if fmt=='ia4':
        vals=[((round((.299*r+.587*g+.114*b))>>5)<<1)|(a>>7) for r,g,b,a in px]
        return bytes((vals[i]<<4)|vals[i+1] for i in range(0,len(vals),2))
    if fmt=='i4':
        # I4 stores intensity only; the extractor represents that intensity
        # in both grayscale RGB and PNG alpha. Fully transparent pixels often
        # have RGB=(0,0,0), so converting RGB alone destroys the extracted
        # texture and can make glyphs (such as font_letter_A) disappear.
        # If alpha is carrying intensity, quantize alpha to I4. For opaque
        # edited PNGs, keep the existing grayscale-RGB workflow.
        if any(a < 255 for r,g,b,a in px):
            vals=[a >> 4 for r,g,b,a in px]
        else:
            vals=[round(.299*r+.587*g+.114*b)>>4 for r,g,b,a in px]
        return bytes((vals[i]<<4)|vals[i+1] for i in range(0,len(vals),2))
    if fmt in ('ci8','ci4'):
        if not palette:
            raise ValueError(f'{fmt} sem TLUT original disponivel')
        by_color=defaultdict(list)
        for i,c in enumerate(palette): by_color[tuple(c)].append(i)
        # When several palette entries have the same RGBA value, prefer the
        # index most used by the original ROM texels. This keeps unmodified
        # extracted PNGs stable instead of arbitrarily changing duplicate indices.
        preferred={}
        if original_raw is not None:
            counts=defaultdict(lambda: defaultdict(int))
            if fmt=='ci8':
                for idx in original_raw: counts[tuple(palette[idx])][idx]+=1
            else:
                for b in original_raw:
                    counts[tuple(palette[(b>>4)&0xF])][(b>>4)&0xF]+=1
                    counts[tuple(palette[b&0xF])][b&0xF]+=1
            preferred={c:max(v,key=v.get) for c,v in counts.items()}
        indices=[]
        rgb_fallback=defaultdict(list)
        if fmt=='ci8':
            for i,c in enumerate(palette):
                rgb_fallback[tuple(c[:3])].append(i)
        for c in px:
            key=tuple(c)
            opts=by_color.get(key)
            # Some CI8 PNGs carry alpha=0 in the exported pixel while the
            # original TLUT entry has the same RGB and alpha=1. Keep the
            # original TLUT color/index semantics by matching RGB only.
            if not opts and fmt=='ci8' and c[3]==0:
                opts=rgb_fallback.get(tuple(c[:3]))
            if not opts:
                raise ValueError(f'{fmt} PNG contem cor fora da TLUT original: {key}')
            indices.append(preferred.get(key,opts[0]))
        if fmt=='ci8':
            return bytes(indices)
        out=bytearray()
        for i in range(0,len(indices),2):
            hi=indices[i]
            lo=indices[i+1] if i+1<len(indices) else 0
            out.append((hi<<4)|lo)
        return bytes(out)
    raise ValueError(f'formato {fmt} desconhecido')

def png_size(p):
    with Image.open(p) as im:return im.size

def load_kart_palettes(root, rom, entries):
    """Load each kart frame's stitched 256-entry CI8 TLUT from its own
    character JSON. Wheel palette names are reused across characters, so
    they must be resolved through the character-local JSON.
    """
    out={}
    cache={}
    for e in entries:
        if not e.get("_kart"):
            continue
        rel=str(e.get("png","")).replace("\\","/").split("/")
        if len(rel)<3 or rel[0].lower()!="karts":
            raise ValueError(f"caminho KART invalido: {e.get('png')}")
        character=rel[1].lower()
        jp=root/'assets'/'karts'/f'{character}_kart.json'
        if not jp.is_file():
            raise FileNotFoundError(f"JSON do kart ausente: {jp}")
        if jp not in cache:
            cache[jp]=json.loads(jp.read_text(encoding='utf-8'))
        data=cache[jp]
        palette=[]
        refs=e.get("tlut") or []
        if isinstance(refs,str):
            refs=[refs]
        for ref in refs:
            info=data.get(ref)
            if not isinstance(info,dict):
                raise KeyError(f"TLUT {ref} ausente em {jp.name} para {e.get('symbol')}")
            off=int(str(info["rom_offset"]),0)
            size=int(info["width"])*int(info["height"])*2
            raw=rom[off:off+size]
            if len(raw)!=size:
                raise ValueError(f"TLUT {ref} curta em {jp.name}: {len(raw)}/{size}")
            palette.extend(decode_rgba16_palette(raw))
        if len(palette)!=256:
            raise ValueError(f"TLUT KART incompleta para {e.get('symbol')}: {len(palette)}/256")
        out[e.get("symbol")]=palette
    return out


def load_manifest(root, kind):
    if kind=='all':
        files=['generated_texture_manifest.json','texture_tkmk00_manifest.json']
    elif kind=='mio0': files=['generated_texture_manifest.json']
    elif kind=='lakitu': files=[]
    else: files=['texture_tkmk00_manifest.json']
    out=[]
    for f in files:
        p=root/f
        if p.exists():
            d=json.loads(p.read_text(encoding='utf-8')); out.extend(d if isinstance(d,list) else d.get('textures',[]))
    return out

def resolve_png(root, rel):
    p=root/rel
    if p.exists(): return p
    # allow passing the extracted PNG basename/path from current directory
    q=Path(rel)
    if q.exists(): return q
    return None

def tkmk_compress(png, entry, root, rom=None):
    # Reuse the proven encoder from the standalone TKMK tool.
    #
    # IMPORTANT: the TKMK encoder is not guaranteed to reproduce the original
    # compressed bytes for an unchanged texture. Re-encoding an unchanged
    # texture can therefore make it larger than its original slot. When both
    # the decoded pixel hash and original compressed-block hash match the
    # manifest, keep the original block byte-for-byte. This is deliberately
    # conservative and only affects exact unchanged assets.
    import importlib.util
    spec=importlib.util.spec_from_file_location('_tkmk',str(TKMOD)); m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    ac=entry.get('alpha_color','0x01')
    if isinstance(ac,str): ac=int(ac,0)
    w=int(entry['width']); h=int(entry['height'])
    colors=m.rgba5551_from_png(str(png),w,h,ac)

    # Exact native RGBA16 byte stream produced by the TKMK decoder.
    raw=bytearray()
    for c in colors: raw.extend(struct.pack('>H',c & 0xFFFF))

    if rom is not None:
        try:
            off=int(str(entry['rom_offset']),0)
            slot=int(entry['compressed_size'])
            original=bytes(rom[off:off+slot])
            expected_raw=str(entry.get('decoded_hash_fnv1a32','')).lower().replace('0x','')
            expected_comp=str(entry.get('compressed_hash_fnv1a32','')).lower().replace('0x','')
            raw_hash=f'{fnv1a(bytes(raw)):08x}'
            comp_hash=f'{fnv1a(original):08x}'
            if (len(original)==slot and original[:6]==b'TKMK00' and
                expected_raw and raw_hash==expected_raw and
                expected_comp and comp_hash==expected_comp):
                return original, colors
        except Exception:
            # Never let the preservation shortcut interfere with normal
            # encoding if a manifest/hash field is unavailable.
            pass

    blob=m.encode(colors,w,h,ac)
    helper=root/'..'/'src'/'xbox360'/'tkmk00_extract_helper.exe'
    if helper.exists():
        try:
            m.verify_with_helper(blob,w,h,ac,colors,helper)
        except PermissionError:
            # The helper may exist but be unavailable on a non-Windows host.
            pass
    return blob, colors

def tkmk_test(blob, entry, root):
    # Decode using the project's helper if available; otherwise the encoder's
    # internal verification is used by tkmk00_rom_tool.
    return True

def allocation_for_mio0(rom, offset):
    dec,end=mio0_decode(rom[offset:])
    return len(dec),end

def _mio0_align_offsets(blob, alignment=4):
    """Align the MIO0 compressed-data section like the original toolchain.

    The reference MIO0 encoder aligns ``comp_offset`` to a 4-byte boundary.
    The compressed stream consists of 2-byte tokens, so this also leaves the
    raw-data offset at a valid halfword boundary without adding a second pad.
    """
    if len(blob) < 16 or blob[:4] != b'MIO0':
        raise ValueError('invalid MIO0 blob')
    n, cp, rp = struct.unpack('>III', blob[4:16])
    if not (16 <= cp <= rp <= len(blob)):
        raise ValueError('invalid MIO0 section offsets')
    pad_cp = (-cp) % alignment
    cp2 = cp + pad_cp
    out = (b'MIO0' + struct.pack('>III', n, cp2, rp + pad_cp) +
           blob[16:cp] + b'\x00' * pad_cp + blob[cp:])
    decoded,_ = mio0_decode(out)
    original_decoded,_ = mio0_decode(blob)
    if decoded != original_decoded:
        raise ValueError('MIO0 alignment changed decoded bytes')
    return out


def _mio0_best_blob(raw, original_cap):
    """Compress a complete MIO0 allocation, preserving the proven selection logic."""
    candidates=[_mio0_encode_greedy(raw)]
    blob2=mio0_encode(raw,128)
    if len(blob2)<len(candidates[0]):
        candidates.append(blob2)
    best_so_far=min(candidates,key=len)
    if len(blob2)<len(candidates[0]) or len(best_so_far)>original_cap:
        blob3=_mio0_encode_full(raw)
        if len(blob3)<len(best_so_far):
            candidates.append(blob3)
    valid=[]
    for candidate in candidates:
        decoded,_=mio0_decode(candidate)
        if decoded!=raw:
            raise ValueError('MIO0 candidate round-trip pixel bytes mismatch')
        valid.append(candidate)
    fitting=[candidate for candidate in valid if len(candidate)<=original_cap]
    blob=min(fitting or valid,key=len)
    decoded,_=mio0_decode(blob)
    if decoded!=raw:
        raise ValueError('MIO0 round-trip pixel bytes mismatch')
    blob=_mio0_align_offsets(blob, 4)
    if len(blob)>original_cap:
        raise ValueError(f'MIO0 aligned blob exceeds safe capacity: {len(blob)}/{original_cap}')
    decoded,_=mio0_decode(blob)
    if decoded!=raw:
        raise ValueError('MIO0 aligned round-trip mismatch')
    return blob


def _find_mio0_subrange(block, expected_hash, size, raw_hint=None):
    """Find a manifest texture inside a shared decoded MIO0 block.

    Use direct byte search whenever a decoded-byte hint is available. This is
    both exact and dramatically faster than recalculating FNV-1a at every byte.
    The hash scan remains as a fallback for unusual CI palette-index streams.
    """
    expected_hash=str(expected_hash or '').lower().replace('0x','')
    if size<0 or size>len(block):
        return None
    if size==len(block):
        if not expected_hash or fnv1a(block)==int(expected_hash,16):
            return 0
        return None
    if raw_hint is not None and len(raw_hint)==size:
        pos=block.find(raw_hint)
        if pos>=0:
            return pos
    if not expected_hash:
        return None
    target=int(expected_hash,16)
    step=2 if size % 2 == 0 else 1
    for off in range(0,len(block)-size+1,step):
        if fnv1a(block[off:off+size])==target:
            return off
    if step==2:
        for off in range(1,len(block)-size+1,2):
            if fnv1a(block[off:off+size])==target:
                return off
    return None


def zero_padding_capacity(rom, offset, compressed_size, max_scan=0x100000):
    """Return the safe MIO0 capacity including only contiguous zero padding.

    The original MIO0 allocation ends at ``offset + compressed_size``.  Some
    ROMs have real zero-filled padding before the next asset.  It is safe to
    consume that padding when a replacement MIO0 grows, but never a non-zero
    byte.
    """
    end=offset+compressed_size
    limit=min(len(rom), end+max_scan)
    p=end
    while p<limit and rom[p]==0:
        p+=1
    return p-offset


def tkmk_padding_capacity(rom, offset, compressed_size, known_offsets=(), max_scan=0x100000):
    """Return safe TKMK00 capacity using only real zero-filled padding.

    TKMK00 allocations are normally adjacent, so the original compressed
    size is the hard limit unless the ROM really contains zero bytes after the
    block.  The scan is additionally capped by the next known manifest asset
    address, so a padding run can never cross into another known allocation.
    No padding is invented: if the first byte after the original block is
    non-zero, the returned capacity is exactly ``compressed_size``.
    """
    end=offset+compressed_size
    limit=min(len(rom), end+max_scan)

    # Use every known manifest address as a second safety barrier.  This is
    # deliberately broader than only TKMK00 entries: a different asset may
    # be the next known allocation in a future ROM/layout.
    for nxt in known_offsets:
        if nxt > end:
            limit=min(limit, nxt)
            break

    p=end
    while p<limit and rom[p]==0:
        p+=1
    return p-offset



def load_lakitu_entries(root):
    """Load the raw CI8 Lakitu sprites and their shared RGBA16 TLUTs.

    Each assets/lakitu/*.json contains one common_tlut_* record followed by
    the CI8 frames that use it.  The TLUTs are separate 512-byte blocks in the
    ROM, so an edited palette can be written back without relocating anything.
    """
    lakitu_dir=root/'assets'/'lakitu'
    if not lakitu_dir.is_dir():
        return []
    out=[]
    for jp in sorted(lakitu_dir.glob('*.json')):
        try: d=json.loads(jp.read_text(encoding='utf-8'))
        except Exception: continue
        if not isinstance(d,dict): continue
        tluts={k:v for k,v in d.items()
               if isinstance(v,dict) and str(v.get('type','')).lower()=='rgba16'
               and k.startswith('common_tlut')}
        for sym,rec in d.items():
            if not isinstance(rec,dict) or str(rec.get('type','')).lower()!='ci8':
                continue
            tlut=rec.get('tlut')
            if not isinstance(tlut,str) or tlut not in tluts:
                continue
            out.append({
                'symbol':sym,
                'png':f"lakitu/{rec.get('output_dir',jp.stem)}/{sym}.png",
                'rom_offset':rec.get('rom_offset'),
                'width':int(rec.get('width',0)),
                'height':int(rec.get('height',0)),
                'tlut':tlut,
                'tlut_rec':tluts[tlut],
                'source_json':jp.name,
            })
    return out


def _rgba16_palette_bytes(palette):
    """Encode a 256-entry decoded RGBA palette back to N64 RGBA16."""
    if len(palette)!=256:
        raise ValueError(f'TLUT CI8 inesperada: {len(palette)} entradas')
    out=bytearray()
    for r,g,b,a in palette:
        out += struct.pack('>H', rgba16(int(r),int(g),int(b),int(a)))
    return bytes(out)


def _same_rgb_alpha(c1,c2):
    return tuple(c1)==tuple(c2) or (c1[3]==0 and tuple(c1[:3])==tuple(c2[:3]))


def build_lakitu_palette_updates(source_root, extracted_root, rom, entries):
    """Derive Lakitu TLUT edits directly from ROM bytes + extracted PNGs.

    No auxiliary baseline file is required.  The original CI8 frame bytes in
    the ROM tell us which TLUT index each pixel used; the unedited colors of
    those indices are recovered from the extracted PNGs themselves.  When a
    PNG changes a color for an index, only that TLUT entry is replaced.

    This is intentionally generic for every Lakitu TLUT group.  If an index is
    split into two different colors inside one shared TLUT group, the edit is
    rejected rather than silently corrupting another frame.
    """
    by_tlut=defaultdict(list)
    for e in entries:
        by_tlut[e['tlut']].append(e)

    updates={}
    frame_raw={}
    reports=[]
    changed_symbols=set()

    for tlut,group in by_tlut.items():
        tr=group[0]['tlut_rec']
        tlut_off=int(str(tr['rom_offset']),0)
        tlut_block=int(str(tr.get('block_offset',0)),0)
        tlut_size=int(tr.get('width',16))*int(tr.get('height',16))*2
        # The Lakitu TLUT records point into the shared MIO0 allocation at
        # 0x132B50.  Decode that allocation first; block_offset is an offset
        # inside the decoded MIO0 data, not a direct ROM address.
        alloc, _ = mio0_decode(rom[tlut_off:])
        raw=bytes(alloc[tlut_block:tlut_block+tlut_size])
        if len(raw)!=512:
            raise ValueError(f'TLUT {tlut} nao possui 512 bytes na alocacao MIO0')
        original_palette=decode_rgba16_palette(raw)

        # Recover the palette as represented by the extracted PNGs.  We do not
        # assume that PNG RGB values are byte-identical to RGBA16-expanded ROM
        # values; the index relationship is the authoritative baseline.
        baseline_colors=defaultdict(set)
        edited_colors=defaultdict(set)
        group_changed=False

        for e in group:
            png=resolve_png(extracted_root,e['png'])
            if not png:
                reports.append((e['symbol'],'MISSING_PNG',e['png'],hex(int(str(e['rom_offset']),0)),'LAKITU_CI8'))
                continue
            try:
                w,h=png_size(png)
                if (w,h)!=(e['width'],e['height']):
                    raise ValueError(f'dimensions PNG={w}x{h}, manifest={e["width"]}x{e["height"]}')
                frame_off=int(str(e['rom_offset']),0)
                n=e['width']*e['height']
                old=bytes(rom[frame_off:frame_off+n])
                if len(old)!=n:
                    raise ValueError(f'CI8 original incompleto ({len(old)}/{n})')
                frame_raw[e['symbol']]=old
                px=list(Image.open(png).convert('RGBA').getdata())

                # First pass: establish the original PNG color for each ROM
                # palette index from pixels that were not made transparent.
                for i,c in enumerate(px):
                    if c[3]==0:
                        continue
                    baseline_colors[old[i]].add(tuple(c))

                # Second pass: detect edits relative to that recovered baseline.
                local_changes={}
                for i,c in enumerate(px):
                    if c[3]==0:
                        continue
                    idx=old[i]
                    nc=tuple(c)
                    base=baseline_colors[idx]
                    # The first pass includes this exact pixel, so this test
                    # would always pass.  Actual comparison is deferred until
                    # all group PNGs are known; collect desired colors here.
                    edited_colors[idx].add(nc)
                reports.append((e['symbol'],'OK',f'{n}/{n}',hex(frame_off),'LAKITU_CI8'))
            except Exception as ex:
                reports.append((e['symbol'],'ERROR',str(ex),hex(int(str(e['rom_offset']),0)),'LAKITU_CI8'))

        # The extracted PNG is normally the original art unless the user edited
        # it.  Compare each pixel color to the physical ROM TLUT-expanded color
        # first.  If it differs only by RGBA16 quantization, it is still native.
        # Otherwise promote that exact index to the edited PNG color.
        proposed=[None]*256
        for e in group:
            if e['symbol'] not in frame_raw:
                continue
            png=resolve_png(extracted_root,e['png'])
            px=list(Image.open(png).convert('RGBA').getdata())
            old=frame_raw[e['symbol']]
            local={}
            for i,c in enumerate(px):
                if c[3]==0:
                    continue
                idx=old[i]
                nc=tuple(c)
                native=original_palette[idx]
                if _same_rgb_alpha(nc,native):
                    continue
                # The PNG can use an 8-bit expansion of an N64 RGBA16 color;
                # tolerate the exact round-trip through the RGBA16 quantizer.
                q=decode_rgba16_texel(rgba16(*nc)) if 'decode_rgba16_texel' in globals() else None
                if q is not None and _same_rgb_alpha(q,native):
                    continue
                prev=local.get(idx)
                if prev is not None and prev!=nc:
                    raise ValueError(f'TLUT {tlut} indice {idx} recebe mais de uma cor no frame')
                local[idx]=nc
            if local:
                changed_symbols.add(e['symbol'])
                group_changed=True
            for idx,nc in local.items():
                prev=proposed[idx]
                if prev is not None and prev!=nc:
                    raise ValueError(f'TLUT {tlut} indice {idx} recebe cores diferentes entre frames')
                proposed[idx]=nc

        if group_changed:
            newpal=list(original_palette)
            for idx,c in enumerate(proposed):
                if c is not None:
                    newpal[idx]=decode_rgba16_texel(rgba16(*c))
            newraw=_rgba16_palette_bytes(newpal)
            if newraw!=raw:
                updates[tlut]=(tlut_off,tlut_block,newraw,group,{i for i,c in enumerate(proposed) if c is not None})
            else:
                updates[tlut]=None
        else:
            updates[tlut]=None
    return updates, frame_raw, reports, changed_symbols

def process_rainbow_neon_ci8(extracted_root, rom, frames_override=None):
    """Safely repack the Rainbow Road neon CI8 palette animation.

    All 22 animation records live inside one MIO0 allocation. Their PNGs are
    rendered frames, but the ROM stores reusable CI8 index planes plus one
    256-entry RGBA16 TLUT per animation frame. The manifest's decoded_size
    (16384) is NOT the size of a single CI8 image; pixel planes are 64*64 and
    the TLUTs are 512-byte subranges in the shared decoded allocation.

    This handler changes palette bytes only. It starts from the original ROM
    allocation, rejects ambiguous per-index colors, verifies every subrange,
    recompresses the complete allocation once, and emits a normal MIO0 patch so
    the existing duplicate-allocation merger can safely combine other edits.
    """
    mp=extracted_root/'rainbow_road_neon_frames_manifest.json'
    if not mp.is_file():
        return [], []
    if frames_override is None:
        data=json.loads(mp.read_text(encoding='utf-8'))
        frames=data if isinstance(data,list) else data.get('textures',[])
    else:
        frames=frames_override
    if not frames:
        return [], []

    reports=[]
    parsed=[]
    for e in frames:
        if str(e.get('format','')).lower()!='ci8' or str(e.get('compression','')).lower()!='raw':
            raise ValueError(f"Rainbow neon manifest inesperado para {e.get('symbol')}")
        w,h=int(e.get('width',0)),int(e.get('height',0))
        if w<=0 or h<=0 or w*h!=4096:
            raise ValueError(f"Dimensoes CI8 inesperadas em {e.get('symbol')}: {w}x{h}")
        off=int(str(e['rom_offset']),0)
        tlut_off=int(str(e['tlut_rom_offset']),0)
        if off!=tlut_off:
            raise ValueError(f"Alocacoes ROM divergentes em {e.get('symbol')}")
        parsed.append((e,off,int(e['block_offset']),int(e['tlut_block_offset']),w,h))

    offsets={x[1] for x in parsed}
    if len(offsets)!=1:
        raise ValueError(f"Frames Rainbow apontam para mais de uma alocacao: {sorted(offsets)}")
    off=next(iter(offsets))
    decoded_size,original_cap=allocation_for_mio0(rom,off)
    decoded,_=mio0_decode(rom[off:off+original_cap])
    if len(decoded)!=decoded_size:
        raise ValueError(f'Rainbow Road MIO0 decoded size inconsistente: {len(decoded)}/{decoded_size}')
    safe_cap=zero_padding_capacity(rom,off,original_cap)
    original_blob=bytes(rom[off:off+original_cap])
    work=bytearray(decoded)
    changed=False
    seen_tluts=set()

    for e,frame_off,pixel_block,tlut_block,w,h in parsed:
        symbol=e.get('symbol','?')
        png=resolve_png(extracted_root,e.get('png',''))
        if not png:
            reports.append((symbol,'MISSING_PNG',e.get('png'),hex(off),'RAINBOW_CI8_TLUT'))
            continue
        try:
            if png_size(png)!=(w,h):
                raise ValueError(f"dimensoes PNG={png_size(png)}, manifest={w}x{h}")
            n=w*h
            if pixel_block<0 or pixel_block+n>len(decoded):
                raise ValueError(f"subregiao de pixels fora da alocacao: {pixel_block}+{n}/{len(decoded)}")
            if tlut_block<0 or tlut_block+512>len(decoded):
                raise ValueError(f"subregiao TLUT fora da alocacao: {tlut_block}+512/{len(decoded)}")
            if tlut_block in seen_tluts:
                raise ValueError(f"TLUT repetida inesperadamente em {symbol}: {tlut_block}")
            seen_tluts.add(tlut_block)

            indices=bytes(decoded[pixel_block:pixel_block+n])
            palraw=bytes(decoded[tlut_block:tlut_block+512])
            if len(indices)!=n or len(palraw)!=512:
                raise ValueError('pixels/TLUT truncados')
            palette=decode_rgba16_palette(palraw)
            with Image.open(png) as im:
                pixels=list(im.convert('RGBA').getdata())

            proposed={}
            for idx,c in zip(indices,pixels):
                # Fully transparent pixels do not define a palette color. This
                # preserves transparent/unused palette entries from the ROM.
                if c[3]==0:
                    continue
                native=palette[idx]
                if _same_rgb_alpha(tuple(c),native):
                    continue
                q=decode_rgba16_texel(rgba16(*c))
                if _same_rgb_alpha(q,native):
                    continue
                prev=proposed.get(idx)
                if prev is not None and prev!=tuple(c):
                    raise ValueError(f"indice CI8 {idx} exige cores diferentes no mesmo frame")
                proposed[idx]=tuple(c)

            if proposed:
                newpal=list(palette)
                for idx,c in proposed.items():
                    newpal[idx]=decode_rgba16_texel(rgba16(*c))
                newraw=_rgba16_palette_bytes(newpal)
                if newraw!=palraw:
                    work[tlut_block:tlut_block+512]=newraw
                    changed=True
                    reports.append((symbol,'OK_EDITED',f"TLUT {len(proposed)} indices",hex(off),'RAINBOW_CI8_TLUT'))
                else:
                    reports.append((symbol,'OK','TLUT sem mudanca efetiva',hex(off),'RAINBOW_CI8_TLUT'))
            else:
                reports.append((symbol,'OK','pixels correspondem a TLUT original',hex(off),'RAINBOW_CI8_TLUT'))
        except Exception as ex:
            reports.append((symbol,'ERROR',str(ex),hex(off),'RAINBOW_CI8_TLUT'))

    # If a PNG is missing or ambiguous, never produce a partial animation patch.
    if any(r[1] in ('ERROR','MISSING_PNG') for r in reports):
        return [], reports
    if not changed:
        # No patch needed: keep the original allocation byte-for-byte.
        return [], reports

    # Palette-only edits do not justify the exhaustive encoder search. Try
    # the normal bounded-search encoder (often smaller than greedy), align it
    # like the original toolchain, and fail closed if it still does not fit.
    blob=_mio0_align_offsets(mio0_encode(bytes(work),128),4)
    decoded_check,_=mio0_decode(blob)
    if decoded_check!=bytes(work):
        raise ValueError('Rainbow Road CI8 MIO0 round-trip falhou')
    if len(blob)>safe_cap:
        raise ValueError(f"Rainbow Road CI8 MIO0 maior que capacidade segura: {len(blob)}/{safe_cap}")
    return [(None,None,off,blob,safe_cap)], reports


def process_lakitu(source_root, extracted_root, rom):
    """Validate Lakitu CI8 and patch edited shared TLUTs inside their MIO0 allocation.

    Lakitu CI8 frames are raw 4032-byte index streams. Their palettes are not
    standalone ROM blocks: the assets/lakitu JSON stores a block_offset inside
    the shared MIO0 allocation at 0x132B50. Therefore a palette edit must be
    applied to the decoded MIO0 allocation and the complete allocation must be
    recompressed in-place, preserving its existing zero-padding capacity.
    """
    entries=load_lakitu_entries(source_root)
    if not entries:
        return [],[],[]
    updates, frame_raw, reports, changed_symbols=build_lakitu_palette_updates(source_root,extracted_root,rom,entries)
    patches=[]
    grouped=defaultdict(list)
    for tlut,info in updates.items():
        if info is None:
            continue
        tlut_off,tlut_block,newraw,group,_=info
        grouped[tlut_off].append((tlut,tlut_block,newraw))

    for tlut_off,changes in grouped.items():
        decoded,original_cap=mio0_decode(rom[tlut_off:])
        safe_cap=zero_padding_capacity(rom,tlut_off,original_cap)
        work=bytearray(decoded)
        for tlut,tlut_block,newraw in changes:
            if tlut_block < 0 or tlut_block + len(newraw) > len(work):
                raise ValueError(f'TLUT {tlut} fora da alocacao MIO0')
            work[tlut_block:tlut_block+len(newraw)]=newraw
        blob=_mio0_encode_greedy(bytes(work))
        if len(blob)>safe_cap:
            raise ValueError(f'MIO0 Lakitu maior que capacidade: {len(blob)}/{safe_cap} (padding: {safe_cap-original_cap})')
        patches.append((None,None,tlut_off,blob,safe_cap))
    return patches, reports, entries


def process(args):
    root=Path(args.extracted).resolve(); romp=Path(args.rom).resolve()
    rom=romp.read_bytes()
    entries=load_manifest(root,args.type)

    # Standalone RAW textures that are physically fixed-size and already proven
    # against the ROM decoder.  Only I4 and RGBA16 are enabled here; RAW CI8
    # animation frames are handled by the dedicated palette-aware path below.
    raw_entries=[]
    if args.type in ('mio0','all'):
        raw_entries=[e for e in entries
                     if str(e.get('compression','')).lower()=='raw'
                     and str(e.get('format','')).lower() in ('i4','rgba16')
                     and e.get('rom_offset') is not None]

    # KART sprites are MIO0 CI8 blocks in their own manifest.  Keep the
    # existing generated/TKMK loaders untouched and append these entries.
    kart_entries=[]
    if args.type in ('mio0','all'):
        kpath=root/'kart_sprite_manifest.json'
        if kpath.is_file():
            kd=json.loads(kpath.read_text(encoding='utf-8'))
            kart_entries=kd if isinstance(kd,list) else kd.get('textures',[])
            for ke in kart_entries:
                ke['_kart']=True
                ke['compression']='MIO0'
                ke['decoded_size']=int(ke['width'])*int(ke['height'])
            entries.extend(kart_entries)
    if args.symbol:
        wanted=set(args.symbol)
        entries=[e for e in entries if e.get('symbol') in wanted]
        raw_entries=[e for e in raw_entries if e.get('symbol') in wanted]
        kart_entries=[e for e in kart_entries if e.get('symbol') in wanted]
    if not entries and args.type != 'lakitu': raise SystemExit('Nenhuma entrada encontrada.')

    # Sorted ROM addresses from all loaded manifests form a conservative
    # barrier for TKMK00 padding.  A TKMK block may only consume zero bytes
    # before the next known asset, never beyond it.
    known_manifest_offsets=sorted({
        int(str(e['rom_offset']),0)
        for e in entries
        if e.get('rom_offset') is not None
    })
    source_root=root if (root/'assets').is_dir() else root.parent
    ci_palettes,source_texture_tluts=load_ci_palettes(source_root,rom) if args.type in ('mio0','all') else ({}, {})
    kart_palettes=load_kart_palettes(source_root,rom,kart_entries) if kart_entries else {}
    texture_tluts=load_texture_tlut_map(source_root)

    reports=[]
    patches=[]
    processed_offsets=set()

    # ---- Rainbow Road neon CI8 palette animation ------------------------
    # Separate from generic RAW/MIO0 handling: all animation palettes are
    # subranges of one shared MIO0 allocation and must be patched together.
    if args.type in ('mio0','all'):
        rainbow_manifest=root/'rainbow_road_neon_frames_manifest.json'
        if rainbow_manifest.is_file():
            rainbow_data=json.loads(rainbow_manifest.read_text(encoding='utf-8'))
            rainbow_entries=rainbow_data if isinstance(rainbow_data,list) else rainbow_data.get('textures',[])
            if args.symbol:
                wanted=set(args.symbol)
                rainbow_entries=[e for e in rainbow_entries if e.get('symbol') in wanted]
                # A --symbol selection unrelated to this group must not invoke it.
                run_rainbow=bool(rainbow_entries)
            else:
                run_rainbow=True
            if run_rainbow:
                rp,rr=process_rainbow_neon_ci8(root,rom,rainbow_entries if args.symbol else None)
                patches.extend(rp)
                reports.extend(rr)

    # ---- Lakitu CI8: raw frames + shared per-sequence TLUTs --------------
    # This is deliberately independent from MIO0/TKMK/kart handling.  An
    # unchanged Lakitu frame/TLUT is preserved byte-for-byte; edited CI8
    # colors cause only the corresponding shared TLUT and 4032-byte frames to
    # be patched.  No relocation is performed.
    if args.type in ('all', 'lakitu'):
        lak_patches, lak_reports, lak_entries = process_lakitu(source_root, root, rom)
        reports.extend(lak_reports)
        patches.extend(lak_patches)

    # Hash-prefixed aliases are intentionally NOT part of the normal generated
    # patch set. An alias becomes active automatically only when its current PNG
    # is actually edited relative to the physical ROM subrange. No JSON is changed.
    alias_paths=set()
    if args.type in ('mio0','all'):
        apath=root/'texture_alias_manifest.json'
        if apath.is_file():
            try:
                ad=json.loads(apath.read_text(encoding='utf-8'))
                alias_paths={str(x).replace('\\','/').lower()
                             for x in ad.get('kept_hash_aliases',[]) if x}
            except Exception:
                alias_paths=set()

    promoted_alias_entries=[]
    if alias_paths:
        kept=[]
        for e in entries:
            rel=str(e.get('png','')).replace('\\','/').lower()
            if rel in alias_paths and Path(rel).parent == Path('.'):
                promoted_alias_entries.append(e)
            else:
                kept.append(e)
        entries=kept

    if args.symbol:
        # --symbol was already applied above; keep alias candidates only when
        # their exact symbol was requested.
        wanted=set(args.symbol)
        promoted_alias_entries=[e for e in promoted_alias_entries
                                if e.get('symbol') in wanted]


    # MIO0 allocations are sometimes shared by many manifest entries. The
    # old patcher treated every entry as a whole compressed block and therefore
    # repeatedly overwrote the same ROM offset with a tiny independent MIO0.
    # Grouping by ROM offset lets us edit the decoded allocation in place and
    # recompress it exactly once. This is the critical safety fix.
    # The Rainbow Road neon base CI8 PNGs are rendered views of textures
    # whose pixels are stored with animated TLUTs. Their palette edits must be
    # handled exclusively by process_rainbow_neon_ci8 above; sending these
    # base PNGs through the generic CI8 encoder incorrectly compares their
    # edited colors against a single static TLUT and poisons the entire shared
    # MIO0 group (all 15 entries then receive the same error).
    rainbow_base_symbols=set()
    rainbow_manifest=root/'rainbow_road_neon_frames_manifest.json'
    if rainbow_manifest.is_file():
        try:
            rd=json.loads(rainbow_manifest.read_text(encoding='utf-8'))
            rf=rd if isinstance(rd,list) else rd.get('textures',[])
            rainbow_base_symbols={str(x.get('source_texture')) for x in rf if x.get('source_texture')}
        except Exception:
            # The dedicated handler reports malformed manifests; do not guess.
            rainbow_base_symbols=set()
    if rainbow_base_symbols:
        entries=[e for e in entries if not (
            e.get('symbol') in rainbow_base_symbols
            and str(e.get('compression','')).upper()=='MIO0'
            and str(e.get('format','')).lower()=='ci8'
        )]

    mio_groups=defaultdict(list)
    non_mio=[]
    for e in entries:
        comp=e.get('compression')
        if comp=='MIO0':
            try: key=int(str(e.get('rom_offset')),0)
            except Exception: key=None
            if key is not None: mio_groups[key].append(e)
            else: non_mio.append(e)
        elif comp=='TKMK00':
            non_mio.append(e)

    def encode_entry_raw(e, original_raw=None, fallback_palette=None):
        png=resolve_png(root,e.get('png',''))
        if not png:
            raise FileNotFoundError(f"PNG ausente: {e.get('png')}")
        w,h=png_size(png); ew,eh=int(e['width']),int(e['height'])
        if (w,h)!=(ew,eh):
            raise ValueError(f'dimensions PNG={w}x{h}, manifest={ew}x{eh}')
        fmt=e['format'].lower()
        palette=None
        if fmt in ('ci4','ci8'):
            if e.get('_kart'):
                palette=kart_palettes.get(e.get('symbol'))
                if palette is None:
                    raise ValueError(f"TLUT KART nao localizada para {e.get('symbol')}")
            else:
                tlut=e.get('tlut')
                if isinstance(tlut,list): tlut=tlut[0] if tlut else None
                if not isinstance(tlut,str): tlut=e.get('tlut_symbol')
                if not isinstance(tlut,str): tlut=texture_tluts.get(e.get('symbol'))
                if isinstance(tlut,str):
                    palette=ci_palettes.get(tlut)
                    if palette is None and fallback_palette is not None:
                        palette=fallback_palette
                elif fallback_palette is not None:
                    palette=fallback_palette
            if palette is None:
                raise ValueError(f"{fmt} sem TLUT declarada para {e.get('symbol')}")
            # Preserve exact original CI index bytes when the PNG is only the
            # extracted representation of the same visual texture.
            if original_raw is not None and _ci_raw_matches_image(Image.open(png),fmt,palette,original_raw):
                return bytes(original_raw)
        return image_bytes(Image.open(png),fmt,palette,original_raw if fmt in ('ci4','ci8') else None)


    # ---- Standalone RAW I4/RGBA16 ---------------------------------------
    # These assets have a fixed physical size, so there is no compression or
    # padding decision involved.  Re-encoding is deterministic; the entry is
    # accepted only when the generated byte stream is exactly the manifest
    # decoded size.  Unchanged PNGs reproduce the original bytes, while edited
    # PNGs replace only their own fixed-size ROM range.
    for e in raw_entries:
        png=resolve_png(root,e.get('png',''))
        off=int(str(e.get('rom_offset')),0)
        size=int(e.get('decoded_size',0))
        if not png:
            reports.append((e.get('symbol'),'MISSING_PNG',e.get('png'),hex(off),'RAW'))
            continue
        try:
            w,h=png_size(png)
            ew,eh=int(e['width']),int(e['height'])
            if (w,h)!=(ew,eh):
                raise ValueError(f'dimensions PNG={w}x{h}, manifest={ew}x{eh}')
            raw=encode_entry_raw(e,bytes(rom[off:off+size]),None)
            if len(raw)!=size:
                raise ValueError(f'bytes codificados ({len(raw)}) != decoded_size ({size})')
            if off<0 or off+size>len(rom):
                raise ValueError(f'intervalo RAW fora da ROM: {hex(off)}+{size}')
            patches.append((e,png,off,raw,size))
            reports.append((e.get('symbol'),'OK',f'{size}/{size}',hex(off),'RAW'))
        except Exception as ex:
            reports.append((e.get('symbol'),'ERROR',str(ex),hex(off),'RAW'))


    # Discover kept aliases directly from assets.json.  The alias filename
    # carries the physical decoded hash and the original symbol, e.g.
    # ``1e038bfb__common_texture_hud_lap_time.rgba16.inc.png``.  We reconstruct
    # only the metadata needed by the repacker in memory; no manifest is edited.
    # This makes the behavior generic for every kept alias, not just VOLTA.
    if alias_paths:
        try:
            aj=json.loads((source_root/'assets.json').read_text(encoding='utf-8'))
            if isinstance(aj,dict):
                for key,info in aj.items():
                    if not isinstance(info,dict):
                        continue
                    symbol=str(key)
                    width=info.get('width'); height=info.get('height')
                    fmt=info.get('type',info.get('format'))
                    meta=info.get('meta')
                    if isinstance(meta,dict) and isinstance(meta.get('dims'),(list,tuple)) and len(meta['dims'])>=2:
                        width=meta['dims'][0]; height=meta['dims'][1]
                    try:
                        width=int(width); height=int(height)
                    except (TypeError,ValueError):
                        continue
                    if width<=0 or height<=0 or not fmt:
                        continue
                    for rel in sorted(alias_paths):
                        stem=Path(rel).name
                        if '__' not in stem:
                            continue
                        alias_symbol=stem.split('__',1)[1]
                        m=re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\.(rgba32|rgba16|ia16|ia8|ia4|i8|i4|ci8|ci4)\.inc\.png$',alias_symbol,re.I)
                        if not m or m.group(1)!=symbol:
                            continue
                        alias_fmt=m.group(2).lower()
                        bits={'rgba32':32,'rgba16':16,'ia16':16,'ia8':8,'ia4':4,'i8':8,'i4':4,'ci8':8,'ci4':4}.get(alias_fmt)
                        if bits is None:
                            continue
                        decoded_size=(width*height*bits+7)//8
                        hv=Path(rel).name.split('__',1)[0]
                        e={
                            'png':rel, 'bank':'asset_json.c', 'symbol':symbol,
                            'rom_offset':info.get('rom_offset'),
                            'block_offset':info.get('block_offset'),
                            'width':width, 'height':height, 'format':alias_fmt,
                            'compression':'MIO0', 'decoded_size':decoded_size,
                            'decoded_hash_fnv1a32':hv, 'asset_json_discovered':True,
                        }
                        if e['rom_offset'] is not None and e['block_offset'] is not None:
                            promoted_alias_entries.append(e)
        except Exception:
            pass

    # Activate only aliases whose current PNG bytes differ from the original
    # decoded subrange. No manifest is modified.
    active_aliases=[]
    alias_ranges_by_offset=defaultdict(set)
    for e in promoted_alias_entries:
        try:
            comp=e.get('compression')
            if comp!='MIO0':
                continue
            off=int(str(e.get('rom_offset')),0)
            decoded_size,cap=allocation_for_mio0(rom,off)
            block=mio0_decode(bytes(rom[off:off+cap]))[0]
            size=int(e.get('decoded_size',0))
            bo=e.get('block_offset')
            if bo is None:
                pos=_find_mio0_subrange(block,e.get('decoded_hash_fnv1a32'),size,None)
            else:
                pos=int(str(bo),0)
            if pos is None or pos<0 or pos+size>len(block):
                continue
            png=resolve_png(root,e.get('png',''))
            if not png:
                continue
            raw=encode_entry_raw(e,bytes(block[pos:pos+size]),None)
            if raw != bytes(block[pos:pos+size]):
                active_aliases.append(e)
                alias_ranges_by_offset[off].add((pos,pos+size))
        except Exception:
            # A malformed promoted alias must never poison the proven normal set.
            continue

    if active_aliases:
        entries.extend(active_aliases)
        # If an active alias targets exactly the same physical bytes as a canonical
        # entry, the alias is the user's edited version and must replace that one.
        filtered=[]
        active_keys={(int(str(e.get('rom_offset')),0),
                      int(str(e.get('block_offset',0)),0),
                      int(e.get('decoded_size',0))) for e in active_aliases}
        for e in entries:
            if e in active_aliases:
                filtered.append(e); continue
            if e.get('compression')=='MIO0':
                try:
                    key=(int(str(e.get('rom_offset')),0),
                         int(str(e.get('block_offset',0)),0),
                         int(e.get('decoded_size',0)))
                    if key in active_keys:
                        continue
                except Exception:
                    pass
            filtered.append(e)
        entries=filtered

        # Rebuild the physical MIO0 groups after activating an alias.  The
        # original V3 grouped entries before this optional activation; without
        # rebuilding, the newly activated alias would never reach the shared
        # block patch loop.
        entries=[e for e in entries if not (
            e.get('symbol') in rainbow_base_symbols
            and str(e.get('compression','')).upper()=='MIO0'
            and str(e.get('format','')).lower()=='ci8'
        )]
        mio_groups=defaultdict(list)
        non_mio=[]
        for e in entries:
            comp=e.get('compression')
            if comp=='MIO0':
                try: key=int(str(e.get('rom_offset')),0)
                except Exception: key=None
                if key is not None: mio_groups[key].append(e)
                else: non_mio.append(e)
            elif comp=='TKMK00':
                non_mio.append(e)

    # ---- MIO0: one physical patch per compressed allocation -------------
    for off,group in mio_groups.items():
        try:
            decoded_size,original_cap=allocation_for_mio0(rom,off)
            safe_cap=zero_padding_capacity(rom,off,original_cap)
            original_blob=bytes(rom[off:off+original_cap])
            original_decoded=mio0_decode(original_blob)[0]
            if len(original_decoded)!=decoded_size:
                raise ValueError(f'MIO0 decoded size inconsistente em {hex(off)}')
            work=bytearray(original_decoded)
            ranges=[]
            changed=False
            entry_positions={}

            shared_palette=None
            for sibling in group:
                st=sibling.get('tlut') or sibling.get('tlut_symbol') or texture_tluts.get(sibling.get('symbol'))
                if isinstance(st,list): st=st[0] if st else None
                if isinstance(st,str) and st in ci_palettes:
                    shared_palette=ci_palettes[st]
                    break

            for e in group:
                size=int(e.get('decoded_size',0))
                expected_hash=e.get('decoded_hash_fnv1a32')
                bo=e.get('block_offset')
                if bo is not None:
                    pos=int(str(bo),0)
                    if pos<0 or pos+size>len(work):
                        raise ValueError(f'block_offset fora do bloco para {e.get("symbol")}')
                    # Verify the manifest points at the bytes it claims.
                    if expected_hash and fnv1a(bytes(work[pos:pos+size])) != int(str(expected_hash),16):
                        raise ValueError(f'block_offset/hash nao conferem para {e.get("symbol")}')
                else:
                    # Full-block aliases are unambiguous. For smaller entries,
                    # first build a decoded-byte hint and use fast bytes.find().
                    raw_hint=encode_entry_raw(e,None,shared_palette)
                    pos=_find_mio0_subrange(bytes(work),expected_hash,size,raw_hint)
                    if pos is None:
                        if size==len(work): pos=0
                        else: raise ValueError(f'nao foi possivel localizar {e.get("symbol")} dentro do MIO0 compartilhado')
                entry_positions[id(e)]=pos
                ranges.append((pos,pos+size,e))

            # Reject accidental partial overlaps. Exact aliases are allowed and
            # must stay synchronized because they are the same physical bytes.
            for i,(a0,a1,ae) in enumerate(ranges):
                for b0,b1,be in ranges[i+1:]:
                    overlap=max(a0,b0)<min(a1,b1)
                    exact=(a0==b0 and a1==b1)
                    contains=(a0<=b0 and a1>=b1) or (b0<=a0 and b1>=a1)
                    # A full-block manifest entry (for example gTextureGhosts)
                    # legitimately contains the smaller frame entries. Only
                    # partial/ambiguous overlaps are unsafe.
                    if overlap and not exact and not contains:
                        raise ValueError(f'subregioes MIO0 sobrepostas: {ae.get("symbol")} / {be.get("symbol")}')

            replacements={}
            for pos,end,e in ranges:
                pos=entry_positions[id(e)]
                old_raw=bytes(original_decoded[pos:end])
                raw=encode_entry_raw(e,old_raw,shared_palette)
                size=end-pos
                if len(raw)!=size:
                    raise ValueError(f'bytes codificados ({len(raw)}) != decoded_size ({size}) para {e.get("symbol")}')
                prev=replacements.get((pos,end))
                if prev is not None and prev!=raw:
                    raise ValueError(f'aliases fisicos diferentes no mesmo subbloco: {e.get("symbol")}')
                replacements[(pos,end)]=raw

            # Apply containing/full-block replacements first; child frames
            # are then allowed to refine their own byte ranges.
            for (pos,end),raw in sorted(replacements.items(), key=lambda kv: (-(kv[0][1]-kv[0][0]), kv[0][0])):
                if bytes(work[pos:end])!=raw:
                    work[pos:end]=raw
                    changed=True

            if changed:
                blob=_mio0_best_blob(bytes(work),safe_cap)
                if len(blob)>safe_cap:
                    for e in group:
                        reports.append((e.get('symbol'),'TOO_LARGE',f'{len(blob)}/{original_cap} (padding disponivel: {safe_cap-original_cap})',hex(off),'MIO0_SHARED'))
                    continue
                decoded,_=mio0_decode(blob)
                if decoded!=bytes(work):
                    raise ValueError(f'MIO0 compartilhado nao fez round-trip em {hex(off)}')
                patches.append((None,None,off,blob,safe_cap))
                processed_offsets.add(off)
            else:
                # Byte-for-byte preservation is intentional: an unchanged
                # shared allocation is never needlessly recompressed.
                patches.append((None,None,off,original_blob,original_cap))
                processed_offsets.add(off)

            for e in group:
                shown = len(original_blob) if not changed else len(blob)
                detail = f'{shown}/{original_cap}'
                if changed and safe_cap>original_cap:
                    detail += f' (padding: {safe_cap-original_cap})'
                report_kind='KART_MIO0' if e.get('_kart') else 'MIO0_SHARED'
                reports.append((e.get('symbol'),'OK',detail,hex(off),report_kind))
        except Exception as ex:
            for e in group:
                reports.append((e.get('symbol'),'ERROR',str(ex),hex(off),'MIO0_SHARED'))

    # ---- TKMK00 and any non-MIO0 entries retain the proven original path --
    for e in non_mio:
        comp=e.get('compression')
        if comp!='TKMK00':
            if comp=='MIO0':
                continue
            continue
        png=resolve_png(root,e.get('png',''))
        if not png:
            reports.append((e.get('symbol'),'MISSING_PNG',e.get('png')));continue
        try:
            w,h=png_size(png); ew,eh=int(e['width']),int(e['height'])
            if (w,h)!=(ew,eh): raise ValueError(f'dimensions PNG={w}x{h}, manifest={ew}x{eh}')
            off=int(str(e['rom_offset']),0)
            blob,_=tkmk_compress(png,e,root,rom)
            original_cap=int(e['compressed_size'])
            safe_cap=tkmk_padding_capacity(rom,off,original_cap,known_manifest_offsets)
            ok=len(blob)<=safe_cap
            detail=f'{len(blob)}/{original_cap} (padding: {safe_cap-original_cap})'
            reports.append((e.get('symbol'),'OK' if ok else 'TOO_LARGE',detail,hex(off),comp))
            if ok: patches.append((e,png,off,blob,safe_cap))
        except Exception as ex:
            reports.append((e.get('symbol'),'ERROR',str(ex)))

    for r in reports: print(' | '.join(map(str,r)))
    print(f'\nRESULTADO: {sum(1 for r in reports if str(r[1]).startswith("OK"))} OK / {sum(1 for r in reports if r[1]=="TOO_LARGE")} maiores / {sum(1 for r in reports if not str(r[1]).startswith("OK") and r[1]!="TOO_LARGE")} erros')
    if args.patch:
        if any(r[1] in ('ERROR','TOO_LARGE','MISSING_PNG') for r in reports):
            raise SystemExit('PATCH ABORTADO: existe entrada inválida, bloco compartilhado inconsistente ou maior que o espaço original.')
        out=Path(args.output).resolve()
        data=bytearray(rom)

        # Multiple logical editors can legitimately target the same physical
        # MIO0 allocation.  Lakitu Final Lap TLUTs are inside 0x132B50, and
        # common HUD textures (including common_texture_hud_lap_time) are also
        # subranges of that allocation.  The old writer silently kept only
        # the first full-block patch, which made the second edit disappear.
        # Merge duplicate MIO0 patches against the original decoded allocation
        # so both independent edits survive in the same compressed block.
        # Group patches by physical ROM offset. Different handlers can report
        # different safe capacities for the same shared MIO0 allocation; using
        # (offset, capacity) as the key lets the same block be written twice,
        # potentially erasing the Rainbow Road animated TLUT patch.
        by_physical=defaultdict(list)
        for item in patches:
            _e,_png,off,blob,cap=item
            by_physical[off].append(item)

        for off,items in by_physical.items():
            # Conservative capacity: never write beyond the smallest boundary
            # reported by any patch targeting this physical allocation.
            cap=min(int(item[4]) for item in items)
            if len(items)==1:
                _e,_png,_off,blob,_cap=items[0]
                data[off:off+cap]=blob+b'\0'*(cap-len(blob))
                continue

            blobs=[bytes(item[3]) for item in items]
            if not all(blob[:4]==b'MIO0' for blob in blobs):
                # Keep the historical behavior for unexpected non-MIO0
                # collisions rather than guessing how unrelated raw blocks
                # should be combined.
                blob=blobs[0]
                data[off:off+cap]=blob+b'\0'*(cap-len(blob))
                continue

            original_decoded,original_comp=mio0_decode(rom[off:])
            merged=bytearray(original_decoded)
            for candidate_blob in blobs:
                candidate_decoded,_=mio0_decode(candidate_blob)
                if len(candidate_decoded)!=len(original_decoded):
                    raise SystemExit(f'MIO0 duplicado incompatível em {hex(off)}')
                for i,(orig_byte,new_byte) in enumerate(zip(original_decoded,candidate_decoded)):
                    if new_byte==orig_byte:
                        continue
                    current=merged[i]
                    if current!=orig_byte and current!=new_byte:
                        raise SystemExit(f'Conflito entre patches MIO0 em {hex(off)} no byte {i}')
                    merged[i]=new_byte

            merged_blob=_mio0_best_blob(bytes(merged),cap)
            decoded_check,_=mio0_decode(merged_blob)
            if decoded_check!=bytes(merged):
                raise SystemExit(f'MIO0 mesclado sem round-trip em {hex(off)}')
            data[off:off+cap]=merged_blob+b'\0'*(cap-len(merged_blob))

        out.write_bytes(data); print('ROM criada:',out)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--rom',required=True);ap.add_argument('--extracted',required=True)
    ap.add_argument('--type',choices=['mio0','tkmk00','lakitu','all'],default='all')
    ap.add_argument('--symbol',action='append');ap.add_argument('--patch',action='store_true');ap.add_argument('--output')
    a=ap.parse_args()
    if a.patch and not a.output: ap.error('--patch exige --output')
    process(a)
if __name__=='__main__': main()
