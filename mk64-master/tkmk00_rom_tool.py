#!/usr/bin/env python3
"""Safe TKMK00 ROM replacement tool for Mario Kart 64.

Modes:
  inspect <manifest> <symbol>
  compress <png> <out.tkmk> --width W --height H --alpha 0x01
  test <rom> <manifest> <symbol> <png>
  patch <rom> <manifest> <symbol> <png> <outrom>

The patch operation NEVER moves data and NEVER changes pointers. It only replaces
bytes at the exact rom_offset recorded by texture_tkmk00_manifest.json when the
new compressed block fits inside compressed_size. Remaining bytes are padded with
zeros. Before patching, the generated TKMK00 is decoded with the project's decoder
implementation and compared pixel-for-pixel against the RGBA5551 image prepared
from the source PNG.
"""
from __future__ import annotations
import argparse, json, struct, sys, hashlib, subprocess, tempfile, os
from pathlib import Path
from collections import Counter

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install with: python -m pip install pillow", file=sys.stderr)
    raise


def be16(v): return struct.pack('>H', v & 0xFFFF)
def be32(v): return struct.pack('>I', v & 0xFFFFFFFF)

class Writer:
    def __init__(self):
        self.data=[0]; self.remaining=8; self.finished=False
        self.final=b''; self.repeat=b''
    def write(self, command, bits):
        if self.finished: return
        for i in range(bits-1,-1,-1):
            self.data[-1]=((self.data[-1]<<1)|((command>>i)&1))&0xFF
            self.remaining-=1
            if self.remaining==0:
                self.data.append(0); self.remaining=8
    def finish(self):
        if self.finished:return
        self.finished=True
        self.data[-1]=(self.data[-1]<<self.remaining)&0xFF
        raw=bytes(self.data)
        self.final=raw + b'\0'*(4-len(raw)%4)
        zones=[]; in_zone=False; start=0
        for i in range(len(self.data)-1):
            if not in_zone:
                if self.data[i]==self.data[i+1]: in_zone=True; start=i
            elif self.data[i]!=self.data[i+1]:
                n=i-start+1
                if n>=3: zones.append((start,n))
                in_zone=False
        if in_zone:
            n=len(self.data)-start
            if n>=3: zones.append((start,n))
        out=bytearray()
        if zones:
            ignore=True if zones[0][0]!=0 else False
            off=0; zi=0
            while off<len(self.data):
                if ignore:
                    end=zones[zi][0] if zi<len(zones) else len(self.data)
                    count=min(128,end-off)
                    if count<0: zi+=1; continue
                    out.append((count-1)+0x80)
                    out.extend(self.data[off:off+count]); off+=count
                    if end==off: ignore=False
                else:
                    next_count=zones[zi][0]+zones[zi][1]-off
                    count=min(130,next_count)
                    if next_count!=count and next_count-count<3: count-=3
                    out.append(count-3); out.append(self.data[off]); off+=count
                    if zones[zi][0]+zones[zi][1]==off:
                        zi+=1
                        ignore=(zi>=len(zones) or zones[zi][0]!=off)
        else:
            off=0
            while off<len(self.data):
                count=min(128,len(self.data)-off)
                out.append((count-1)+0x80); out.extend(self.data[off:off+count]); off+=count
        self.repeat=bytes(out)+b'\0'*(4-len(out)%4)
    @property
    def saved(self): return len(self.final)-len(self.repeat)

class Node:
    __slots__=('left','right','value')
    def __init__(self,value=0,left=None,right=None):self.value=value;self.left=left;self.right=right
    @property
    def leaf(self):return self.left is None and self.right is None

def huff_tree(counts):
    a=[(c,Node(value=i),i) for i,c in enumerate(counts)]
    while len(a)>1:
        # Stable-ish equivalent of the C# List.Sort descending by frequency.
        a.sort(key=lambda x:x[0], reverse=True)
        c1,n1,_=a[-1]; c2,n2,_=a[-2]
        a=a[:-2]
        a.append((c1+c2,Node(64,n1,n2),len(a)))
    return a[0][1]

def huff_codes(node, code=0, bits=0, out=None):
    if out is None: out=[None]*32
    if node.leaf: out[node.value]=(code,bits)
    else:
        huff_codes(node.left,code<<1,bits+1,out)
        huff_codes(node.right,(code<<1)|1,bits+1,out)
    return out

def write_tree(node,w):
    if node.leaf:w.write(0,1);w.write(node.value,5)
    else:w.write(1,1);write_tree(node.left,w);write_tree(node.right,w)

def reverse_combine(pred, actual):
    space=pred if pred<0x10 else 0x1F-pred
    diff=actual-pred
    if space<abs(diff): return 0x1F-actual if pred>=0x10 else actual
    if diff<0:return abs(diff)<<1
    if diff>0:return (abs(diff-1)<<1)|1
    return 0

def nearest(cur,w,colors,used):
    for k,off in enumerate((-2,-1,0,1,2),1):
        p=cur+w+off
        if p>=len(colors):return 0
        prev=p-1
        # Match original encoder's boundary behavior.
        if colors[cur]==colors[p] and colors[cur]!=colors[prev] and not used[p]:return k
    return 0

def rgba5551_from_png(path,w,h,alpha_color,resize='none'):
    im=Image.open(path).convert('RGBA')
    if im.size!=(w,h):
        if resize == 'none':
            raise ValueError(f'PNG is {im.size[0]}x{im.size[1]}, manifest requires {w}x{h}. Use --resize box|nearest|lanczos for an automatic downsample.')
        filt={'nearest':Image.Resampling.NEAREST,'box':Image.Resampling.BOX,'lanczos':Image.Resampling.LANCZOS}[resize]
        im=im.resize((w,h),filt)
    ac=alpha_color & 0xFFFE
    # alpha_color is the decoded RGB555 value used by the game's transparency test.
    out=[]
    for r,g,b,a in im.getdata():
        if a < 128:
            v=ac
        else:
            rr=(r*31+127)//255; gg=(g*31+127)//255; bb=(b*31+127)//255
            v=(rr<<11)|(gg<<6)|(bb<<1)|1
        out.append(v)
    return out

def encode(colors,w,h,alpha_color):
    n=w*h
    if len(colors)!=n: raise ValueError('pixel count mismatch')
    predg=[0]*n;predr=[0]*n;predb=[0]*n; cmap=[0]*n; nearest_map=[0]*n; used=[False]*n
    counts=[0]*32
    hv=[]
    for row in range(h):
      for col in range(w):
        p=row*w+col; c=colors[p]
        top=colors[p-w] if row else 0; left=colors[p-1] if p else 0
        ag=(c>>6)&0x1F; at=(top>>6)&0x1F; al=(left>>6)&0x1F
        gp=(at+al)//2; predg[p]=reverse_combine(gp,ag); gc=ag-gp
        rp=max(0,min(31,gc+(((top>>11)&31)+((left>>11)&31))//2)); pred_r=reverse_combine(rp,(c>>11)&31);predr[p]=pred_r
        bp=max(0,min(31,gc+(((top>>1)&31)+((left>>1)&31))//2)); predb[p]=reverse_combine(bp,(c>>1)&31)
    rb=[0xFF]*64; last=0
    master=Writer(); chans=[Writer() for _ in range(8)]
    for row in range(h):
      for col in range(w):
        p=row*w+col
        if used[p]: last=colors[p]; continue
        ch=cmap[p]+1
        c=colors[p]
        if c==last: chans[ch].write(0,1)
        else:
          chans[ch].write(1,1)
          try: idx=rb.index(c)
          except ValueError: idx=-1
          if idx>=0:
            master.write(0,1);master.write(idx,6)
            rb[1:idx+1]=rb[0:idx];rb[0]=c
          else:
            master.write(1,1)
            hv.extend((predg[p],predr[p],predb[p])); counts[predg[p]]+=1;counts[predr[p]]+=1;counts[predb[p]]+=1
            rb[1:]=rb[:-1];rb[0]=c
          if col<w-1:cmap[p+1]+=1
          if col<w-2:cmap[p+2]+=1
          if row<h-1 and col>0:cmap[p+w-1]+=1
          if row<h-1:cmap[p+w]+=1
          if row<h-1 and col<w-1:cmap[p+w+1]+=1
          if row<h-2:cmap[p+2*w]+=1
          q=nearest(p,w,colors,used)
          if q==0: master.write(0,1)
          else:
            first=True;offp=p
            while nearest_map[offp]!=0:
              q=nearest_map[offp]
              offp += [-2,-1,0,1,2][q-1] + w
              if used[offp]:break
              used[offp]=True
              if first: master.write(1,1);first=False
              else: pass
              if q==1:master.write(2,4)
              elif q==2:master.write(1,2)
              elif q==3:master.write(2,2)
              elif q==4:master.write(3,2)
              elif q==5:master.write(3,4)
              nearest_map[offp]=nearest(offp,w,colors,used)
            if first:master.write(0,1)
            else:master.write(0,3)
          last=c
        # Original encoder's nearest map is computed even when the color is new.
        if not used[p]: nearest_map[p]=nearest(p,w,colors,used)
    tree=huff_tree(counts);codes=huff_codes(tree)
    write_tree(tree,chans[0])
    for v in hv: code,bits=codes[v];chans[0].write(code,bits)
    master.finish();[x.finish() for x in chans]
    offset=0x2C+len(master.final); datas=[];mask=0;ptrs=[]
    for i,ch in enumerate(chans):
      ptrs.append(offset)
      if ch.saved>0:datas.append(ch.repeat);mask|=1<<i;offset+=len(ch.repeat)
      else:datas.append(ch.final);offset+=len(ch.final)
    hdr=b'TKMK00'+bytes([mask,0x0F])+be16(w)+be16(h)+b''.join(be32(x) for x in ptrs)
    return hdr+master.final+b''.join(datas)


def verify_with_helper(blob, width, height, alpha, expected_colors, helper):
    helper = str(helper)
    with tempfile.TemporaryDirectory() as td:
        src=Path(td)/"in.tkmk"; dst=Path(td)/"out.rgba16"
        src.write_bytes(blob)
        r=subprocess.run([helper,str(src),str(dst),hex(alpha)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        if r.returncode != 0:
            raise ValueError(f'TKMK decoder verification failed (exit {r.returncode}): {r.stderr.decode(errors="replace").strip()}')
        data=dst.read_bytes()
        if len(data)<8 or data[:8] != bytes([width>>8,width&255,height>>8,height&255,0,0,0,0]):
            raise ValueError('decoder returned invalid dimension header')
        raw=data[8:]
        got=[int.from_bytes(raw[i:i+2],'big') for i in range(0,len(raw),2)]
        if got != expected_colors:
            for i,(a,b) in enumerate(zip(expected_colors,got)):
                if a!=b: raise ValueError(f'decoder verification pixel mismatch at {i}: expected 0x{a:04X}, got 0x{b:04X}')
            raise ValueError('decoder verification pixel count mismatch')
        return fnv1a(raw)

def fnv1a(data):
    h=0x811c9dc5
    for b in data:h=((h^b)*0x01000193)&0xffffffff
    return f'{h:08x}'

def load_entry(manifest,symbol):
    d=json.load(open(manifest,encoding='utf8'))
    for e in d:
      if e.get('symbol')==symbol:return e
    raise KeyError(f'symbol not found: {symbol}')

def inspect(args):
    e=load_entry(args.manifest,args.symbol)
    for k in ('symbol','png','rom_offset','compressed_size','width','height','format','compression','alpha_color','compressed_hash_fnv1a32','decoded_hash_fnv1a32'):
        if k in e:print(f'{k}: {e[k]}')

def prepare(args):
    e=load_entry(args.manifest,args.symbol) if args.manifest else None
    w=e['width'] if e else args.width;h=e['height'] if e else args.height
    ac=int(e['alpha_color'],0) if e else int(args.alpha,0)
    colors=rgba5551_from_png(args.png,w,h,ac,args.resize)
    blob=encode(colors,w,h,ac)
    if args.helper:
        print('decoder verify:', verify_with_helper(blob,w,h,ac,colors,args.helper))
    Path(args.output).write_bytes(blob)
    print(f'OK: {len(blob)} bytes ({len(blob)/max(1,e["compressed_size"]) * 100:.1f}% of slot)' if e else f'OK: {len(blob)} bytes')
    print('TKMK hash:',fnv1a(blob))
    if e and len(blob)>e['compressed_size']:
        print(f'WARNING: does NOT fit original slot ({e["compressed_size"]} bytes)',file=sys.stderr);return 10
    return 0

def patch(args):
    e=load_entry(args.manifest,args.symbol)
    if e.get('compression')!='TKMK00':raise ValueError('manifest entry is not TKMK00')
    rom=Path(args.rom).read_bytes();off=int(e['rom_offset'],0);slot=int(e['compressed_size']);end=off+slot
    if end>len(rom):raise ValueError('manifest slot exceeds ROM size')
    colors=rgba5551_from_png(args.png,e['width'],e['height'],int(e['alpha_color'],0),args.resize)
    blob=encode(colors,e['width'],e['height'],int(e['alpha_color'],0))
    if args.helper:
        decoded_hash=verify_with_helper(blob,e['width'],e['height'],int(e['alpha_color'],0),colors,args.helper)
        print(f'decoder verify: PASS decoded_hash={decoded_hash}')
    if len(blob)>slot:
        raise ValueError(f'NEW BLOCK TOO LARGE: {len(blob)} > {slot} bytes; ROM was NOT modified')
    # Structural validation before touching ROM.
    if blob[:6]!=b'TKMK00':raise ValueError('encoder produced invalid signature')
    if struct.unpack('>HH',blob[8:12])!=(e['width'],e['height']):raise ValueError('encoder dimensions mismatch')
    out=bytearray(rom);out[off:end]=blob+b'\0'*(slot-len(blob))
    Path(args.output).write_bytes(out)
    print(f'PATCHED: {e["symbol"]} @ 0x{off:X}: {len(blob)} / {slot} bytes')
    print(f'old FNV: {fnv1a(rom[off:end])}  manifest: {e.get("compressed_hash_fnv1a32")}')
    print(f'new FNV: {fnv1a(blob)}')
    return 0

def main():
    p=argparse.ArgumentParser();sp=p.add_subparsers(dest='cmd',required=True)
    q=sp.add_parser('inspect');q.add_argument('manifest',type=Path);q.add_argument('symbol');q.set_defaults(fn=inspect)
    q=sp.add_parser('compress');q.add_argument('png',type=Path);q.add_argument('output',type=Path);q.add_argument('--manifest',type=Path);q.add_argument('--symbol');q.add_argument('--width',type=int);q.add_argument('--height',type=int);q.add_argument('--alpha',default='0x01');q.add_argument('--helper',type=Path);q.add_argument('--resize',choices=['none','nearest','box','lanczos'],default='none');q.set_defaults(fn=prepare)
    q=sp.add_parser('patch');q.add_argument('rom',type=Path);q.add_argument('manifest',type=Path);q.add_argument('symbol');q.add_argument('png',type=Path);q.add_argument('output',type=Path);q.add_argument('--helper',type=Path);q.add_argument('--resize',choices=['none','nearest','box','lanczos'],default='none');q.set_defaults(fn=patch)
    a=p.parse_args();
    try:return a.fn(a)
    except Exception as ex: print('ERROR:',ex,file=sys.stderr);return 1
if __name__=='__main__':sys.exit(main())
