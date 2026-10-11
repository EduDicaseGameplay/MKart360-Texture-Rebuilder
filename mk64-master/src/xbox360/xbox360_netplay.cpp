/* MK64_R62_COMPLETE_RESULTS_SOCIAL_PLAYERS_IN_GAME */
/* MK64_R61_SOCIAL_HUB_PROFILE_MAIL_FONT_FIX */
/* MK64_R60_BRIDGED_NAT_HARDENING */
// Copyright (c) 2026 Sirdankz
// SPDX-License-Identifier: MPL-2.0
// Original netplay/crossplay portions: see NETPLAY-LICENSE.md.
#include <xtl.h>
#include <winsockx.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "xbox360/netplay.h"
/* MK64_R55_LOW_LATENCY_EXACT_CURRENT */
#ifndef MK64_ENABLE_LOGGER_OPTIONS
#define MK64_ENABLE_LOGGER_OPTIONS 0
#endif
/* MK64_R54_1_X360_UI_UX_DELETE_REPAIR */
#include "xbox360/race8.h"
#include "xbox360/race8_state.h"
#include "xbox360/netplay_protocol.h"
#include "xbox360/netplay_state_history.h"
#include "xbox360/netplay_diagnostic.h"
#include "xbox360/platform.h" /* X360_CRT_480I_NATIVE_BACKBUFFER */
extern "C" IDirect3DDevice9 *x360_d3d_device(void);
extern "C" void x360_log(const char *);
extern "C" HRESULT x360_party_publish_host(unsigned int publicIp,unsigned short port,unsigned int token);
extern "C" void x360_party_clear_host(void);
extern "C" int x360_party_is_active(void);
extern "C" DWORD x360_party_open_social_ui(void);
extern "C" int x360_party_find_host(unsigned int *publicIp,unsigned short *port,unsigned int *token,char *gamerTag,unsigned int gamerTagSize);
extern "C" int x360_party_logging_enabled(void);
extern "C" void x360_party_set_logging(int enabled);
extern "C" int x360_crossplay_state_pack(unsigned char *out,int cap);
/* UI uses target clears, so it needs no external font file or shader state. */
#include "xbox360_netfont.h"
#include "xbox360/diagnostic_options.h"
#include "xbox360/log_privacy.h"

static bool display_wide=true;
extern "C" int x360_display_widescreen(void){return display_wide?1:0;}
extern "C" float x360_display_aspect(void){return display_wide?16.0f/9.0f:4.0f/3.0f;}
/* MK64_R54_1_X360_UI_UX_DELETE_REPAIR: safe-area layout; colors unchanged. */
static int r54_360_fit(const char *t,int p,int minv,int maxw){if(!t||!*t)return p;while(p>minv&&(int)strlen(t)*6*p>maxw)--p;return p;}
static int r57_online_count=-1;static bool r57_presence_active=false;
static int r62_game_total=-1;static bool r62_modal_lobby=false;
static void r62_end_game();
/* R59 world chat state is separate from per-room chat so RS can swap the
 * lobby panel without destroying either history. */
static bool r59_world_ui_active=false,r59_world_overlay_suppress=false,r59_world_lobby_view=false,r59_world_keyboard=false;
static unsigned r59_world_seq=0;static DWORD r59_world_last_poll=0,r59_world_resume=0;static bool r59_world_pending_ready=false;
static char r59_world_pending[56]={0};static char r59_world_lines[6][96]={{0}};
static bool r61_social_packet(const char *b);
static void r57_presence_tick(bool drain);
static int gR73ControlScrollRow=-1;
static void r59_world_chat_view();static void r59_world_compose(bool lobby_pump);
static void screen(const char *title,const char *a,const char *b,const char *c,const char *d,const char *footer="B BACK",const char *detail=0){
    IDirect3DDevice9 *dev=x360_d3d_device();if(!dev)return;const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);/* R61: net_text accepts 1280x720 virtual coordinates; do not pre-scale by the physical backbuffer. */int x=96,mw=gR73ControlScrollRow>=0?1000:1060,ts=4,bs=3,ss=2;const char *tt=title?title:"MARIO KART 64 - ONLINE";
    net_text(dev,x,(int)(720*.065f),tt,r54_360_fit(tt,ts,2,mw),0xFFFFD050);if(r59_world_ui_active){const char *hub="(C) 2026 SIRDANKZ - ORIGINAL CODE: MPL-2.0";net_text(dev,x,(int)(720*.125f),hub,r54_360_fit(hub,ss,1,790),0xFF90D0FF);net_text(dev,x,(int)(720*.165f),"NINTENDO / TEAM RESURGENT / UPSTREAM RIGHTS UNCHANGED",r54_360_fit("NINTENDO / TEAM RESURGENT / UPSTREAM RIGHTS UNCHANGED",ss,1,850),0xFF8AA7C0);}if(r57_presence_active){char online[32];if(r57_online_count>=0)_snprintf(online,sizeof(online)-1,"PLAYERS ONLINE: %d",r57_online_count);else _snprintf(online,sizeof(online)-1,"PLAYERS ONLINE: --");online[sizeof(online)-1]=0;net_text(dev,894,(int)(720*.135f),online,ss,0xFF90D0FF);char inGame[40];_snprintf(inGame,sizeof(inGame)-1,"PLAYERS IN GAME: %d",r62_game_total>=0?r62_game_total:0);inGame[sizeof(inGame)-1]=0;net_text(dev,894,(int)(720*.173f),inGame,ss,0xFF90D0FF);}
    const char *l0=a?a:"",*l1=b?b:"",*l2=c?c:"",*l3=d?d:"";net_text(dev,x,(int)(720*.235f),l0,r54_360_fit(l0,bs,2,mw),0xFFFFFFFF);net_text(dev,x,(int)(720*.335f),l1,r54_360_fit(l1,bs,2,mw),0xFFFFFFFF);net_text(dev,x,(int)(720*.465f),l2,r54_360_fit(l2,bs,2,mw),0xFF90D0FF);net_text(dev,x,(int)(720*.565f),l3,r54_360_fit(l3,bs,2,mw),0xFF90D0FF);
    /* R59.2: adaptive world-chat placement. Sparse screens use the open
     * center area for more history; dense screens keep a compact ticker. */
    if(r59_world_ui_active&&!r59_world_overlay_suppress){int first=4,count=2;float ty=.650f,my=.700f,step=.048f;if(!l1[0]&&!l2[0]&&!l3[0]){first=0;count=6;ty=.315f;my=.365f;step=.065f;}else if(!l2[0]&&!l3[0]){first=1;count=5;ty=.430f;my=.480f;step=.060f;}else if(!l3[0]){first=3;count=3;ty=.575f;my=.625f;step=.055f;}net_text(dev,x,(int)(720*ty),"WORLD CHAT   RS OPEN",ss,0xFF90D0FF);bool any=false;int row=0;for(int i=first;i<6&&row<count;++i){if(!r59_world_lines[i][0])continue;const char *w=r59_world_lines[i];net_text(dev,x,(int)(720*(my+row*step)),w,r54_360_fit(w,ss,1,mw),0xFFFFFFFF);any=true;++row;}if(!any)net_text(dev,x,(int)(720*my),"NO WORLD MESSAGES YET",ss,0xFFFFFFFF);}
    if(gR73ControlScrollRow>=0&&tt&&strstr(tt,"CONTROLLER ")==tt){
        int top=(gR73ControlScrollRow/4)*4;
        D3DRECT track={(LONG)(1138*sw/1280),(LONG)(169*sh/720),(LONG)(1149*sw/1280),(LONG)(552*sh/720)};
        D3DRECT thumb={(LONG)(1138*sw/1280),(LONG)((174+(top/4)*295/4)*sh/720),
                       (LONG)(1149*sw/1280),(LONG)((174+(top/4)*295/4+78)*sh/720)};
        dev->Clear(1,&track,D3DCLEAR_TARGET,0xFF27415A,1,0);
        dev->Clear(1,&thumb,D3DCLEAR_TARGET,0xFFFFD050,1,0);
        char pageInfo[32];_snprintf(pageInfo,sizeof(pageInfo)-1,"%02d-%02d / 18  UP/DOWN",top+1,top+4>18?18:top+4);
        pageInfo[sizeof(pageInfo)-1]=0;
        net_text(dev,812,575,pageInfo,2,0xFF90D0FF);
    }
    net_text(dev,x,(int)(720*.835f),footer?footer:"",r54_360_fit(footer?footer:"",ss,1,mw),0xFFAAAAAA);const char *dd=detail?detail:"";net_text(dev,x,(int)(720*.905f),dd,r54_360_fit(dd,ss,1,mw),0xFFAAAAAA);dev->Present(0,0,0,0);
}
static DWORD r54_360_del_begin=0; static bool r54_360_del_fired=false;
static bool r54_360_delete_hold(void){XINPUT_STATE st;memset(&st,0,sizeof(st));if(XInputGetState(0,&st)!=ERROR_SUCCESS){r54_360_del_begin=0;r54_360_del_fired=false;return false;}WORD need=(WORD)(XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y);bool down=(st.Gamepad.wButtons&need)==need;DWORD now=GetTickCount();if(!down){r54_360_del_begin=0;r54_360_del_fired=false;return false;}if(!r54_360_del_begin)r54_360_del_begin=now;if(!r54_360_del_fired&&now-r54_360_del_begin>=3000U){r54_360_del_fired=true;return true;}return false;}
/* MK64_R62_3_RIGHT_TRIGGER_STATS */
/* Physical trigger is analog, not part of XInput wButtons. Keep its
 * edge in a private high bit returned only by premenu input helpers. */
static const DWORD kUiRightTriggerStats=0x00010000UL;
static bool sUiRightTriggerPrev=false;
static WORD prev_buttons;
static DWORD pressed(bool loggingHotkey=true) {
    XINPUT_STATE s;memset(&s,0,sizeof(s));XInputGetState(0,&s);
    const bool rt=(s.Gamepad.bRightTrigger>0x40);
    DWORD p=(DWORD)(s.Gamepad.wButtons&~prev_buttons);
    if(rt&&!sUiRightTriggerPrev)p|=kUiRightTriggerStats;
    prev_buttons=s.Gamepad.wButtons;sUiRightTriggerPrev=rt;
    (void)loggingHotkey;
    return p;
}

/*
 * MK64_MENU_INPUT_CARRYOVER_FIX_V1
 *
 * pressed() is edge-triggered: current_buttons & ~prev_buttons.
 * Snapshot the buttons that are physically down when changing menus so a
 * held A/B/D-pad does not become a fresh press in the next menu.
 */
static void x360_menu_consume_current_buttons(void) {
    XINPUT_STATE s;
    memset(&s, 0, sizeof(s));
    if (XInputGetState(0, &s) == ERROR_SUCCESS) {
        prev_buttons = s.Gamepad.wButtons;
        sUiRightTriggerPrev = (s.Gamepad.bRightTrigger > 0x40);
    } else {
        prev_buttons = 0;
        sUiRightTriggerPrev = false;
    }
}


/* B19.4R explicit xboxkrnl XEX resolver declarations */
/* xtl.h in this XDK configuration does not expose these prototypes. */
extern "C" DWORD XexGetModuleHandle(PSZ moduleName, PHANDLE hand);
extern "C" DWORD XexGetProcedureAddress(HANDLE hand, DWORD dwOrdinal, PVOID Address);

/* MK64NET B19.4R - XBOX 360 NETDLL IMPLEMENTATION
 *
 * Xbox 360 NetDll networking ABI used by MK64:
 * caller/xnc = 1, modern XNet/WSA startup exports when available, and the
 * normal NetDll socket family for all sockets used by netplay and UPnP.
 */
#define MK64_XNC_TITLE  1
#define MK64_XNC_SYSAPP 2
#define MK64_NETDLL_VERSION 0x20530800

/*
 * Normally we use the title caller (1), using the MK64 title-caller path for
 * Internet hosting on retail-like/freeboot consoles.  Some XDK-like
 * runtimes using the SYSAPP networking context may reject the
 * undocumented 0x5801 unencrypted-socket option for TITLE with WSAEACCES.
 * In that case open_network() switches this to SYSAPP (2) and retries.
 */
static DWORD mk64_xnc=MK64_XNC_TITLE;

typedef int    (__cdecl *MK_ND_XNETSTARTUP_OLD)(DWORD, XNetStartupParams *);
typedef int    (__cdecl *MK_ND_XNETSTARTUP_NEW)(DWORD, XNetStartupParams *, DWORD);
typedef int    (__cdecl *MK_ND_XNETRANDOM)(DWORD, BYTE *, UINT);
typedef DWORD  (__cdecl *MK_ND_XNETGETTITLEXNADDR)(DWORD, XNADDR *);

typedef int    (__cdecl *MK_ND_WSASTARTUP_OLD)(DWORD, WORD, WSADATA *);
typedef int    (__cdecl *MK_ND_WSASTARTUP_NEW)(DWORD, WORD, WSADATA *, DWORD);
typedef SOCKET (__cdecl *MK_ND_SOCKET)(DWORD, int, int, int);
typedef int    (__cdecl *MK_ND_CLOSESOCKET)(DWORD, SOCKET);
typedef int    (__cdecl *MK_ND_IOCTLSOCKET)(DWORD, SOCKET, long, u_long *);
typedef int    (__cdecl *MK_ND_SETSOCKOPT)(DWORD, SOCKET, int, int, const char *, int);
typedef int    (__cdecl *MK_ND_BIND)(DWORD, SOCKET, const struct sockaddr *, int);
typedef int    (__cdecl *MK_ND_CONNECT)(DWORD, SOCKET, const struct sockaddr *, int);
typedef int    (__cdecl *MK_ND_SELECT)(DWORD, int, fd_set *, fd_set *, fd_set *, const timeval *);
typedef int    (__cdecl *MK_ND_RECV)(DWORD, SOCKET, char *, int, int);
typedef int    (__cdecl *MK_ND_RECVFROM)(DWORD, SOCKET, char *, int, int, struct sockaddr *, int *);
typedef int    (__cdecl *MK_ND_SEND)(DWORD, SOCKET, const char *, int, int);
typedef int    (__cdecl *MK_ND_SENDTO)(DWORD, SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int    (__cdecl *MK_ND_WSAERROR)(void);

static MK_ND_XNETSTARTUP_OLD      mk_nd_xnetstartup_old=0;
static MK_ND_XNETSTARTUP_NEW      mk_nd_xnetstartup_new=0;
static MK_ND_XNETRANDOM           mk_nd_xnetrandom=0;
static MK_ND_XNETGETTITLEXNADDR   mk_nd_xnetgettitlexnaddr=0;
static MK_ND_WSASTARTUP_OLD       mk_nd_wsastartup_old=0;
static MK_ND_WSASTARTUP_NEW       mk_nd_wsastartup_new=0;
static MK_ND_SOCKET               mk_nd_socket=0;
static MK_ND_CLOSESOCKET          mk_nd_closesocket=0;
static MK_ND_IOCTLSOCKET          mk_nd_ioctlsocket=0;
static MK_ND_SETSOCKOPT           mk_nd_setsockopt=0;
static MK_ND_BIND                 mk_nd_bind=0;
static MK_ND_CONNECT              mk_nd_connect=0;
static MK_ND_SELECT               mk_nd_select=0;
static MK_ND_RECV                 mk_nd_recv=0;
static MK_ND_RECVFROM             mk_nd_recvfrom=0;
static MK_ND_SEND                 mk_nd_send=0;
static MK_ND_SENDTO               mk_nd_sendto=0;
static MK_ND_WSAERROR             mk_nd_wsaerror=0;
static bool                       mk_nd_resolved=false;

static DWORD mk_nd_proc(HANDLE h,DWORD ord) {
    DWORD p=0;
    return XexGetProcedureAddress(h,ord,&p)==0?p:0;
}

static bool mk_nd_resolve(void) {
    if(mk_nd_resolved)return true;
    HANDLE h=0;
    if(XexGetModuleHandle((PSZ)"xam.xex",&h)!=0||!h)return false;

    mk_nd_wsastartup_old     =(MK_ND_WSASTARTUP_OLD)    mk_nd_proc(h,0x01);
    mk_nd_socket             =(MK_ND_SOCKET)            mk_nd_proc(h,0x03);
    mk_nd_closesocket        =(MK_ND_CLOSESOCKET)       mk_nd_proc(h,0x04);
    mk_nd_ioctlsocket        =(MK_ND_IOCTLSOCKET)       mk_nd_proc(h,0x06);
    mk_nd_setsockopt         =(MK_ND_SETSOCKOPT)        mk_nd_proc(h,0x07);
    mk_nd_bind               =(MK_ND_BIND)              mk_nd_proc(h,0x0B);
    mk_nd_connect            =(MK_ND_CONNECT)           mk_nd_proc(h,0x0C);
    mk_nd_select             =(MK_ND_SELECT)            mk_nd_proc(h,0x0F);
    mk_nd_recv               =(MK_ND_RECV)              mk_nd_proc(h,0x12);
    mk_nd_recvfrom           =(MK_ND_RECVFROM)          mk_nd_proc(h,0x14);
    mk_nd_send               =(MK_ND_SEND)              mk_nd_proc(h,0x16);
    mk_nd_sendto             =(MK_ND_SENDTO)            mk_nd_proc(h,0x18);
    mk_nd_wsaerror           =(MK_ND_WSAERROR)          mk_nd_proc(h,0x1B);

    mk_nd_wsastartup_new     =(MK_ND_WSASTARTUP_NEW)    mk_nd_proc(h,0x24);
    mk_nd_xnetstartup_old    =(MK_ND_XNETSTARTUP_OLD)   mk_nd_proc(h,0x33);
    mk_nd_xnetrandom         =(MK_ND_XNETRANDOM)        mk_nd_proc(h,0x35);
    mk_nd_xnetgettitlexnaddr =(MK_ND_XNETGETTITLEXNADDR)mk_nd_proc(h,0x49);
    mk_nd_xnetstartup_new    =(MK_ND_XNETSTARTUP_NEW)   mk_nd_proc(h,0x50);

    mk_nd_resolved =
        (mk_nd_xnetstartup_new||mk_nd_xnetstartup_old) &&
        (mk_nd_wsastartup_new||mk_nd_wsastartup_old) &&
        mk_nd_xnetrandom && mk_nd_xnetgettitlexnaddr &&
        mk_nd_socket && mk_nd_closesocket && mk_nd_ioctlsocket &&
        mk_nd_setsockopt && mk_nd_bind && mk_nd_connect && mk_nd_select &&
        mk_nd_recv && mk_nd_recvfrom && mk_nd_send && mk_nd_sendto &&
        mk_nd_wsaerror;
    return mk_nd_resolved;
}

static int mk64_net_xnetstartup(XNetStartupParams *p) {
    if(!mk_nd_resolve())return SOCKET_ERROR;
    if(mk_nd_xnetstartup_new)
        return mk_nd_xnetstartup_new(mk64_xnc,p,MK64_NETDLL_VERSION);
    return mk_nd_xnetstartup_old(mk64_xnc,p);
}
static int mk64_net_xnetrandom(BYTE *p,UINT cb) {
    return mk_nd_resolve()?mk_nd_xnetrandom(mk64_xnc,p,cb):SOCKET_ERROR;
}
static DWORD mk64_net_xnetgettitlexnaddr(XNADDR *a) {
    return mk_nd_resolve()?mk_nd_xnetgettitlexnaddr(mk64_xnc,a):0;
}
static int mk64_net_wsastartup(WORD v,WSADATA *w) {
    if(!mk_nd_resolve())return SOCKET_ERROR;
    if(mk_nd_wsastartup_new)
        return mk_nd_wsastartup_new(mk64_xnc,v,w,MK64_NETDLL_VERSION);
    return mk_nd_wsastartup_old(mk64_xnc,v,w);
}
static SOCKET mk64_net_socket(int af,int type,int proto) {
    return mk_nd_resolve()?mk_nd_socket(mk64_xnc,af,type,proto):INVALID_SOCKET;
}
static int mk64_net_closesocket(SOCKET s) {
    return mk_nd_resolve()?mk_nd_closesocket(mk64_xnc,s):SOCKET_ERROR;
}
static int mk64_net_ioctlsocket(SOCKET s,long cmd,u_long *argp) {
    return mk_nd_resolve()?mk_nd_ioctlsocket(mk64_xnc,s,cmd,argp):SOCKET_ERROR;
}
static int mk64_net_setsockopt(SOCKET s,int level,int opt,const char *val,int len) {
    return mk_nd_resolve()?mk_nd_setsockopt(mk64_xnc,s,level,opt,val,len):SOCKET_ERROR;
}
static int mk64_net_bind(SOCKET s,const struct sockaddr *a,int n) {
    return mk_nd_resolve()?mk_nd_bind(mk64_xnc,s,a,n):SOCKET_ERROR;
}
static int mk64_net_connect(SOCKET s,const struct sockaddr *a,int n) {
    return mk_nd_resolve()?mk_nd_connect(mk64_xnc,s,a,n):SOCKET_ERROR;
}
static int mk64_net_select(int nfds,fd_set *r,fd_set *w,fd_set *e,const timeval *tv) {
    return mk_nd_resolve()?mk_nd_select(mk64_xnc,nfds,r,w,e,tv):SOCKET_ERROR;
}
static int mk64_net_recv(SOCKET s,char *b,int n,int f) {
    return mk_nd_resolve()?mk_nd_recv(mk64_xnc,s,b,n,f):SOCKET_ERROR;
}
static int mk64_net_recvfrom(SOCKET s,char *b,int n,int f,struct sockaddr *a,int *alen) {
    return mk_nd_resolve()?mk_nd_recvfrom(mk64_xnc,s,b,n,f,a,alen):SOCKET_ERROR;
}
static int mk64_net_send(SOCKET s,const char *b,int n,int f) {
    return mk_nd_resolve()?mk_nd_send(mk64_xnc,s,b,n,f):SOCKET_ERROR;
}
/* B19.4R3 built-in network stress simulator
 *
 * Only MK64 UDP/6464 traffic is affected. STUN, DNS and UPnP are untouched.
 *
 * The selected "simulated RTT" is split in half and applied as outbound
 * delay. Set the same profile on both peers to approximate the displayed RTT.
 *
 * Delayed packets are queued instead of sleeping the game thread, so this
 * behaves like network latency rather than a frame-rate stall.
 */
static const unsigned mk_stress_rtt_opts[]    = {0,50,100,200,300,500};
static const unsigned mk_stress_jitter_opts[] = {0,10,20,50,100,200};
static const unsigned mk_stress_loss_opts[]   = {0,1,2,5,10};
static int mk_stress_rtt_index=0;
static int mk_stress_jitter_index=0;
static int mk_stress_loss_index=0;

/* B19.4R4 variable-latency jitter simulation */

#define MK_STRESS_QUEUE 96
struct MkStressPacket {
    bool used;
    SOCKET s;
    int n;
    int flags;
    int alen;
    DWORD due;
    sockaddr_in to;
    char data[mknet::MAX_PACKET];
};
static MkStressPacket mk_stress_q[MK_STRESS_QUEUE];
static DWORD mk_stress_rng=0x4D4B3634U;
static unsigned mk_stress_queued=0;
static unsigned mk_stress_sent=0;
static unsigned mk_stress_dropped=0;
static unsigned mk_stress_overflow=0;
static unsigned mk_stress_delay_samples=0;
static unsigned mk_stress_delay_sum=0;
static unsigned mk_stress_delay_min=0xFFFFFFFFU;
static unsigned mk_stress_delay_max=0;

static unsigned mk_stress_rtt_ms(void) {
    return mk_stress_rtt_opts[mk_stress_rtt_index];
}
static unsigned mk_stress_jitter_ms(void) {
    return mk_stress_jitter_opts[mk_stress_jitter_index];
}
static unsigned mk_stress_loss_pct(void) {
    return mk_stress_loss_opts[mk_stress_loss_index];
}
static unsigned mk_stress_oneway_ms(void) {
    return mk_stress_rtt_ms()/2;
}
static bool mk_stress_enabled(void) {
    return mk_stress_rtt_ms()!=0 || mk_stress_jitter_ms()!=0 || mk_stress_loss_pct()!=0;
}
static unsigned mk_stress_rand32(void) {
    mk_stress_rng=mk_stress_rng*1664525U+1013904223U;
    return mk_stress_rng;
}
static unsigned mk_stress_rand100(void) {
    return (mk_stress_rand32()>>16)%100U;
}
static unsigned mk_stress_packet_delay_ms(void) {
    int delay=(int)mk_stress_oneway_ms();
    unsigned jitter=mk_stress_jitter_ms()/2;

    if(jitter) {
        unsigned span=jitter*2U+1U;
        int delta=(int)(mk_stress_rand32()%span)-(int)jitter;
        delay+=delta;
    }

    if(delay<0)delay=0;

    unsigned d=(unsigned)delay;
    ++mk_stress_delay_samples;
    mk_stress_delay_sum+=d;
    if(d<mk_stress_delay_min)mk_stress_delay_min=d;
    if(d>mk_stress_delay_max)mk_stress_delay_max=d;
    return d;
}
static void mk_stress_clear_queue(void) {
    memset(mk_stress_q,0,sizeof(mk_stress_q));
}
static void mk_stress_reset_stats(void) {
    mk_stress_queued=mk_stress_sent=mk_stress_dropped=mk_stress_overflow=0;
    mk_stress_delay_samples=mk_stress_delay_sum=mk_stress_delay_max=0;
    mk_stress_delay_min=0xFFFFFFFFU;
    mk_stress_rng^=GetTickCount()+0x9E3779B9U;
}
static void mk_stress_flush_due(void) {
    if(!mk_nd_resolve()||!mk_nd_sendto)return;
    DWORD now=GetTickCount();
    for(int i=0;i<MK_STRESS_QUEUE;++i) {
        MkStressPacket &q=mk_stress_q[i];
        if(!q.used)continue;
        if((LONG)(now-q.due)<0)continue;
        mk_nd_sendto(mk64_xnc,q.s,q.data,q.n,q.flags,(const sockaddr*)&q.to,q.alen);
        q.used=false;
        ++mk_stress_sent;
    }
}
static int mk_stress_sendto(SOCKET s,const char *b,int n,int f,const struct sockaddr *a,int alen) {
    if(!mk_nd_resolve())return SOCKET_ERROR;

    const bool game_packet = n>=mknet::HEADER && mknet::valid((const uint8_t*)b,n);
    if(!mk_stress_enabled() || !game_packet ||
       n<=0 || n>(int)sizeof(mk_stress_q[0].data)) {
        return mk_nd_sendto(mk64_xnc,s,b,n,f,a,alen);
    }

    /* Always let a real session GOODBYE leave immediately. */
    if(n>=mknet::HEADER && ((const unsigned char*)b)[5]==mknet::GOODBYE) {
        return mk_nd_sendto(mk64_xnc,s,b,n,f,a,alen);
    }

    if(mk_stress_loss_pct() && mk_stress_rand100()<mk_stress_loss_pct()) {
        ++mk_stress_dropped;
        /* Real UDP sendto() still reports success when the network later drops it. */
        return n;
    }

    unsigned delay=mk_stress_packet_delay_ms();
    if(!delay) {
        return mk_nd_sendto(mk64_xnc,s,b,n,f,a,alen);
    }

    mk_stress_flush_due();

    for(int i=0;i<MK_STRESS_QUEUE;++i) {
        MkStressPacket &q=mk_stress_q[i];
        if(q.used)continue;
        q.used=true;
        q.s=s;
        q.n=n;
        q.flags=f;
        q.alen=(int)sizeof(sockaddr_in);
        q.due=GetTickCount()+delay;
        q.to=*(const sockaddr_in*)a;
        memcpy(q.data,b,n);
        ++mk_stress_queued;
        return n;
    }

    /* Queue saturation behaves like a network drop, not an application error. */
    ++mk_stress_overflow;
    return n;
}

static int mk64_net_sendto(SOCKET s,const char *b,int n,int f,const struct sockaddr *a,int alen) {
    return mk_stress_sendto(s,b,n,f,a,alen);
}
static int mk64_net_wsaerror(void) {
    return mk_nd_resolve()?mk_nd_wsaerror():-1;
}

#define XNetStartup(p)                  mk64_net_xnetstartup((p))
#define XNetRandom(p,n)                 mk64_net_xnetrandom((BYTE*)(p),(UINT)(n))
#define XNetGetTitleXnAddr(a)           mk64_net_xnetgettitlexnaddr((a))
#define WSAStartup(v,w)                 mk64_net_wsastartup((v),(w))
#define socket(a,t,p)                   mk64_net_socket((a),(t),(p))
#define closesocket(s)                  mk64_net_closesocket((s))
#define ioctlsocket(s,c,a)              mk64_net_ioctlsocket((s),(c),(a))
#define setsockopt(s,l,o,v,n)           mk64_net_setsockopt((s),(l),(o),(v),(n))
#define bind(s,a,n)                     mk64_net_bind((s),(a),(n))
#define connect(s,a,n)                  mk64_net_connect((s),(a),(n))
#define select(n,r,w,e,t)               mk64_net_select((n),(r),(w),(e),(t))
#define recv(s,b,n,f)                   mk64_net_recv((s),(b),(n),(f))
#define recvfrom(s,b,n,f,a,l)           mk64_net_recvfrom((s),(b),(n),(f),(a),(l))
#define send(s,b,n,f)                   mk64_net_send((s),(b),(n),(f))
#define sendto(s,b,n,f,a,l)             mk64_net_sendto((s),(b),(n),(f),(a),(l))
#define WSAGetLastError()               mk64_net_wsaerror()

/* R72_HOST_GO_CAPTURE_V1: declaration for disconnect-only host state dump. */
extern "C" void mk64_astra_diag_dump(void);
/* B22N1 2-4 PLAYER HOST-RELAY NETPLAY */
/* MK64_V3_2P_4P_EARLY_RELAY_LOW_LATENCY */
static SOCKET sock=INVALID_SOCKET;
/* R58.2: Public Match NAT/gameplay uses a dedicated ephemeral UDP socket. */
static SOCKET r58_sock=INVALID_SOCKET;
static bool initialized,hosting,active,start_sent,failed;
/* MK64_R69_EXPERIMENTAL_60FPS_CROSSPLAY: V11 only with 2-4-player capacity. */
static bool r69_requested_60=false;
/* MK64_R45_CONTROLS_PREMENU */
static bool return_to_premenu=false;
static mknet::BootBarrier boot;
static uint8_t session[16],nonce[16];
static sockaddr_in host_peer;
static sockaddr_in join_target;
static bool host_session_known;
static unsigned assigned_slot;
static unsigned player_count=1;
static unsigned local_slot=0;
static unsigned local_count=1,join_local_count=1,host_local_count=1;
static bool session_split=false,join_full=false;
static unsigned chosen_delay=4;
static DWORD last_received;
static mknet::Stream4 stream;
static char public_address[80],local_address[80];


/* MK64_R42_X360_PUBLIC_IP_UI
 * Display privacy only. public_address stays intact for Party/UPnP.
 */
static bool r42_public_ip_visible = true;

static void r42_public_ip_line(char *dst, int cap) {
    if (!dst || cap <= 0) return;
    if (!r42_public_ip_visible) {
        _snprintf(dst, cap - 1, "PUBLIC IP: HIDDEN");
        dst[cap - 1] = 0;
        return;
    }

    if (public_address[0] >= '0' && public_address[0] <= '9') {
        int i = 0;
        while (public_address[i] && public_address[i] != ':' && i < cap - 12)
            ++i;
        _snprintf(dst, cap - 1, "PUBLIC IP: %.*s", i, public_address);
    } else {
        _snprintf(dst, cap - 1, "PUBLIC IP: UNAVAILABLE");
    }
    dst[cap - 1] = 0;
}


struct PeerState {
    bool used,ready,acked,gameplay_seen;
    sockaddr_in addr;
    uint8_t nonce[16];
    unsigned slot,local_count,platform;
    DWORD last_received,last_offer;
    unsigned best_rtt;
    mknet::Latency latency;
};
static PeerState peers[mknet::MAX_PLAYERS-1];

/* MK64_R56A_DIRECT_MESH_FALLBACK
 * 3-4 player optimization only. Guest CLIENT_INPUT is duplicated directly
 * guest-to-guest while the existing host relay remains enabled as fallback. */
struct MeshPeerState {
    bool used,direct_seen;
    sockaddr_in addr;
    unsigned slot,local_count;
    DWORD last_probe;
    mknet::Latency latency;
};
struct MeshReportState {unsigned budget,samples;bool seen;};
static MeshPeerState mesh_peers[mknet::MAX_PLAYERS-1];
static MeshReportState mesh_reports[mknet::MAX_PLAYERS][mknet::MAX_PLAYERS];
static unsigned mesh_direct_tx,mesh_direct_rx,mesh_probe_tx,mesh_probe_rx;
static DWORD mesh_last_announce=0;

static unsigned net_rx_total,net_rx_valid,net_rx_hello,net_rx_offer,net_rx_ready,net_rx_start,net_rx_ack;
static unsigned net_tx_punch,net_tx_punch_fail,net_rx_input,net_rx_input_reject,net_tx_input,net_tx_input_fail;
static bool first_gameplay_input;
static bool menu_sync=true;
static bool crossplay=false;
static mknet::CrossStateHistory crossplay_states;
static unsigned host_platform=mknet::PLATFORM_UNKNOWN;
static bool have_host_commit=false;
static uint32_t host_commit_frame=0;
static bool crossplay_host_required=false;

/* Independent NETPLAY file logger. It deliberately does not call x360_log(). */
static bool netplay_logging_enabled=false;
static bool crossplay_diagnostics_enabled=false; /* R41: ALL logging/trace default OFF. */
static bool netplay_log_file_ok=false;
static DWORD netplay_log_last_error=0;
static const char *netplay_log_path="game:\\mk64-netplay.log";

/* MK64_SPLIT_SCREEN_DIAG_V1
 * Small, rate-limited diagnostics for local-controller -> local-player mapping.
 * Output goes to game:\\mk64-netplay.log when NETPLAY LOGGING is enabled. */
static unsigned split_diag_samples=0;
static int split_diag_last_c2=-1;

static void net_log_reset(void) {
    if(!crossplay_diagnostics_enabled)return;
    netplay_log_file_ok=false;
    netplay_log_last_error=0;
    netplay_log_path="game:\\mk64-netplay.log";

    HANDLE f=CreateFileA(netplay_log_path,GENERIC_WRITE,FILE_SHARE_READ,
        NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(f==INVALID_HANDLE_VALUE){
        netplay_log_last_error=GetLastError();
        netplay_log_path="mk64-netplay.log";
        f=CreateFileA(netplay_log_path,GENERIC_WRITE,FILE_SHARE_READ,
            NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    }
    if(f!=INVALID_HANDLE_VALUE){
        netplay_log_file_ok=true;
        netplay_log_last_error=0;
        CloseHandle(f);
    }else{
        netplay_log_last_error=GetLastError();
    }
}

static void net_log(const char *fmt,...) {
    if(!crossplay_diagnostics_enabled || !netplay_logging_enabled)return;
    char text[384];
    char safe[512];
    va_list ap;va_start(ap,fmt);_vsnprintf(text,sizeof(text)-1,fmt,ap);va_end(ap);
    text[sizeof(text)-1]=0;
    mk64_log_scrub_ipv4(text,safe,sizeof(safe));

    /* R41: debugger mirror and disk copy receive privacy-scrubbed text. */
    OutputDebugStringA(safe);
    x360_log(safe);

    HANDLE f=CreateFileA(netplay_log_path,GENERIC_WRITE,FILE_SHARE_READ,
        NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(f!=INVALID_HANDLE_VALUE){
        SetFilePointer(f,0,NULL,FILE_END);
        DWORD written=0;
        if(WriteFile(f,safe,(DWORD)strlen(safe),&written,NULL)){
            netplay_log_file_ok=true;
            netplay_log_last_error=0;
        }else{
            netplay_log_file_ok=false;
            netplay_log_last_error=GetLastError();
        }
        CloseHandle(f);
    }else{
        netplay_log_file_ok=false;
        netplay_log_last_error=GetLastError();
    }
}


/* MK64_ASTRA_TRACE_R17 diagnostic sink. */
extern "C" void x360_net_trace(const char *fmt,...){char text[384];va_list ap;va_start(ap,fmt);_vsnprintf(text,sizeof(text)-1,fmt,ap);va_end(ap);text[sizeof(text)-1]=0;net_log("%s",text);}
static void net_set_logging(bool enabled) {
    if(enabled==netplay_logging_enabled)return;
    netplay_logging_enabled=enabled;
    if(enabled && crossplay_diagnostics_enabled){
        net_log_reset();
        net_log("MK64NET4: NETPLAY LOGGING enabled\n");
    }
}
extern "C" int x360_net_diagnostics_enabled(void){return crossplay_diagnostics_enabled?1:0;}
static void x360_set_master_diagnostics(bool enabled){
    crossplay_diagnostics_enabled=enabled;
    if(!enabled){
        netplay_logging_enabled=false;
        x360_set_logging(0);
        x360_party_set_logging(0);
        netplay_log_file_ok=false;
    }else{
        netplay_logging_enabled=true;
        x360_party_set_logging(0);
#if MK64_ENABLE_LOGGER_OPTIONS
        x360_set_logging(1);
#endif
        net_log_reset();
        net_log("R30_DIAGNOSTICS: ENABLED\n");
    }
}
static void reset_net_counters(){
    net_rx_total=net_rx_valid=net_rx_hello=net_rx_offer=net_rx_ready=net_rx_start=net_rx_ack=0;
    net_tx_punch=net_tx_punch_fail=net_rx_input=net_rx_input_reject=net_tx_input=net_tx_input_fail=0;
    first_gameplay_input=false;
}
static void peer_text(char *dst,const sockaddr_in &a){uint32_t ip=ntohl(a.sin_addr.s_addr);_snprintf(dst,79,"%u.%u.%u.%u:%u",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255,ntohs(a.sin_port));dst[79]=0;}
static bool same_address(const sockaddr_in &a,const sockaddr_in &b){return a.sin_addr.s_addr==b.sin_addr.s_addr&&a.sin_port==b.sin_port;}
static bool same_ip(const sockaddr_in &a,const sockaddr_in &b){return a.sin_addr.s_addr==b.sin_addr.s_addr;}
static void address_text(char *dst,uint32_t ip,unsigned port){_snprintf(dst,79,"%u.%u.%u.%u:%u",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255,port);dst[79]=0;}
#include "xbox360_upnp.h"

/* MK64_CROSSPLAY_COMPONENT_DIAG_R15_NET */
extern "C" void mk64_crossplay_component_diag(unsigned int *out, int cap);
static void mkdiag_write_component_snapshot(void);

static int peer_count(void){int n=0;while(n<(int)(mknet::MAX_PLAYERS-1)&&peers[n].used)++n;return n;}
static unsigned lobby_slots(void){unsigned slots=local_count;for(int i=0;i<peer_count();++i)slots+=peers[i].local_count;return slots;}
static void assign_slots(void){unsigned slot=local_count;for(int i=0;i<peer_count();++i){
    PeerState &p=peers[i];if(p.slot!=slot){p.ready=p.acked=false;p.last_offer=0;}p.slot=slot;slot+=p.local_count;
}}
static int ready_count(void){int n=peer_count(),r=0;for(int i=0;i<n;++i)if(peers[i].ready)++r;return r;}
static int ack_count(void){int n=peer_count(),r=0;for(int i=0;i<n;++i)if(peers[i].acked)++r;return r;}
static bool all_ready(void){int n=peer_count();if(n<1)return false;for(int i=0;i<n;++i)if(!peers[i].ready)return false;return true;}
static bool r55_latency_ready(void){int n=peer_count();if(n<1)return false;for(int i=0;i<n;++i)if(peers[i].latency.samples<3U)return false;return true;}
static bool all_acked(void){int n=peer_count();if(n<1)return false;for(int i=0;i<n;++i)if(!peers[i].acked)return false;return true;}
static int find_peer_address(const sockaddr_in &a){int n=peer_count();for(int i=0;i<n;++i)if(same_address(peers[i].addr,a))return i;return -1;}
static int find_peer_nonce(const uint8_t n[16]){int c=peer_count();for(int i=0;i<c;++i)if(!memcmp(peers[i].nonce,n,16))return i;return -1;}
static void reset_peer(PeerState &p){memset(&p,0,sizeof(p));p.best_rtt=0xFFFFFFFFU;}
static void remove_peer(int idx){
    int n=peer_count();if(idx<0||idx>=n)return;
    char who[80];peer_text(who,peers[idx].addr);net_log("MK64NET4: removing P%u %s before start\n",peers[idx].slot+1,who);
    for(int i=idx;i<n-1;++i){peers[i]=peers[i+1];peers[i].ready=false;peers[i].acked=false;peers[i].last_offer=0;}
    reset_peer(peers[n-1]);assign_slots();
}


/* R58.2 relay envelope is 4 bytes; current largest MK4P packet remains within a 1472-byte UDP payload. */
/* MK64_R58_AUTO_NAT_RELAY
 * Public Match uses a dedicated ephemeral UDP socket to rendezvous
 * through the directory server on UDP 6465.  Peers try direct UDP hole
 * punching first.  If the direct path is unavailable, MK4P datagrams are
 * transparently wrapped and relayed through the directory server.
 * Direct/manual play is unchanged. */
struct R58NatRoute {
    bool used;
    bool direct_seen;
    uint8_t cookie[8];
    uint8_t relay_id;
    sockaddr_in addr;
    DWORD last_direct_rx;
    DWORD last_punch;
    unsigned relay_tx;
    unsigned relay_rx;
    unsigned direct_valid_rx; /* R60: only real MK4P packets validate direct NAT. */
};
static bool r58_nat_enabled=false, r58_nat_is_host=false;
static bool r58_nat_have_self=false;
static uint8_t r58_nat_self[8];
static uint8_t r58_nat_self_id=0;
static char r58_nat_room[20]={0}, r58_nat_auth1[40]={0}, r58_nat_auth2[40]={0};
static DWORD r58_nat_last_control=0;
static R58NatRoute r58_nat_routes[mknet::MAX_PLAYERS];
static const char *k_r58_nat_server_ip="172.233.145.244";
static const unsigned k_r58_nat_server_port=6465;

static bool r58_same_address(const sockaddr_in &a,const sockaddr_in &b){return a.sin_addr.s_addr==b.sin_addr.s_addr&&a.sin_port==b.sin_port;}
static sockaddr_in r58_server_addr(){sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=inet_addr(k_r58_nat_server_ip);a.sin_port=htons((u_short)k_r58_nat_server_port);return a;}
static bool r58_from_server(const sockaddr_in &a){sockaddr_in s=r58_server_addr();return r58_same_address(a,s);}
static int r58_hexval(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static bool r58_parse_cookie(const char *s,uint8_t out[8]){if(!s||strlen(s)<16)return false;for(int i=0;i<8;++i){int a=r58_hexval(s[i*2]),b=r58_hexval(s[i*2+1]);if(a<0||b<0)return false;out[i]=(uint8_t)((a<<4)|b);}return true;}
static void r58_cookie_text(const uint8_t c[8],char out[17]){static const char h[]="0123456789abcdef";for(int i=0;i<8;++i){out[i*2]=h[c[i]>>4];out[i*2+1]=h[c[i]&15];}out[16]=0;}
static bool r58_cookie_equal(const uint8_t a[8],const uint8_t b[8]){return memcmp(a,b,8)==0;}
static int r58_route_cookie(const uint8_t c[8]){for(unsigned i=0;i<mknet::MAX_PLAYERS;++i)if(r58_nat_routes[i].used&&r58_cookie_equal(r58_nat_routes[i].cookie,c))return (int)i;return -1;}
static int r58_route_addr(const sockaddr_in &a){for(unsigned i=0;i<mknet::MAX_PLAYERS;++i)if(r58_nat_routes[i].used&&r58_same_address(r58_nat_routes[i].addr,a))return (int)i;return -1;}
static void r58_rebind_known(const sockaddr_in &oldAddr,const sockaddr_in &newAddr){
    if(r58_same_address(host_peer,oldAddr))host_peer=newAddr;
    if(r58_same_address(join_target,oldAddr))join_target=newAddr;
    for(int i=0;i<peer_count();++i)if(r58_same_address(peers[i].addr,oldAddr))peers[i].addr=newAddr;
    for(unsigned i=0;i<mknet::MAX_PLAYERS-1;++i)if(mesh_peers[i].used&&r58_same_address(mesh_peers[i].addr,oldAddr))mesh_peers[i].addr=newAddr;
}
static int r58_route_install(const uint8_t c[8],const sockaddr_in &a){
    int i=r58_route_cookie(c);if(i<0){for(unsigned j=0;j<mknet::MAX_PLAYERS;++j)if(!r58_nat_routes[j].used){i=(int)j;memset(&r58_nat_routes[j],0,sizeof(r58_nat_routes[j]));r58_nat_routes[j].used=true;memcpy(r58_nat_routes[j].cookie,c,8);break;}}
    if(i<0)return -1;R58NatRoute &r=r58_nat_routes[i];if(r.addr.sin_family&& !r58_same_address(r.addr,a)){r58_rebind_known(r.addr,a);r.direct_seen=false;r.direct_valid_rx=0;r.last_direct_rx=0;}r.addr=a;return i;
}
static void r58_transport_close(){if(r58_sock!=INVALID_SOCKET){closesocket(r58_sock);r58_sock=INVALID_SOCKET;}}
static bool r58_transport_open(){
    if(r58_sock!=INVALID_SOCKET)return true;
    r58_sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(r58_sock==INVALID_SOCKET){net_log("R58.2_NAT: dedicated socket failed wsa=%d\n",WSAGetLastError());return false;}
    BOOL rawSocket=TRUE;if(setsockopt(r58_sock,SOL_SOCKET,0x5801,(PCSTR)&rawSocket,sizeof(rawSocket))==SOCKET_ERROR){net_log("R58.2_NAT: dedicated 0x5801 failed wsa=%d\n",WSAGetLastError());r58_transport_close();return false;}
    int netbuf=128*1024;setsockopt(r58_sock,SOL_SOCKET,SO_RCVBUF,(PCSTR)&netbuf,sizeof(netbuf));setsockopt(r58_sock,SOL_SOCKET,SO_SNDBUF,(PCSTR)&netbuf,sizeof(netbuf));
    sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(0);if(bind(r58_sock,(sockaddr*)&a,sizeof(a))==SOCKET_ERROR){net_log("R58.2_NAT: dedicated bind failed wsa=%d\n",WSAGetLastError());r58_transport_close();return false;}
    u_long nb=1;if(ioctlsocket(r58_sock,FIONBIO,&nb)==SOCKET_ERROR){net_log("R58.2_NAT: dedicated FIONBIO failed wsa=%d\n",WSAGetLastError());r58_transport_close();return false;}
    net_log("R58.2_NAT: dedicated ephemeral Public Match socket ready\n");return true;
}
static SOCKET r58_io_socket(){return (r58_nat_enabled&&r58_sock!=INVALID_SOCKET)?r58_sock:sock;}
static void r58_nat_reset(){r58_transport_close();r58_nat_enabled=r58_nat_is_host=r58_nat_have_self=false;memset(r58_nat_self,0,sizeof(r58_nat_self));r58_nat_self_id=0;r58_nat_room[0]=r58_nat_auth1[0]=r58_nat_auth2[0]=0;r58_nat_last_control=0;memset(r58_nat_routes,0,sizeof(r58_nat_routes));}
static bool r58_nat_host(const char *room,const char *token){r58_nat_reset();if(!r58_transport_open())return false;r58_nat_enabled=r58_nat_is_host=true;strncpy(r58_nat_room,room?room:"",sizeof(r58_nat_room)-1);strncpy(r58_nat_auth1,token?token:"",sizeof(r58_nat_auth1)-1);net_log("R58.2_NAT: public host dedicated NAT transport enabled\n");return true;}
static bool r58_nat_join(const char *room,const char *pid,const char *secret){r58_nat_reset();if(!r58_transport_open())return false;r58_nat_enabled=true;r58_nat_is_host=false;strncpy(r58_nat_room,room?room:"",sizeof(r58_nat_room)-1);strncpy(r58_nat_auth1,pid?pid:"",sizeof(r58_nat_auth1)-1);strncpy(r58_nat_auth2,secret?secret:"",sizeof(r58_nat_auth2)-1);net_log("R58.2_NAT: public join dedicated NAT transport enabled\n");return true;}
static int r58_raw_send(const sockaddr_in &to,const void *p,int n){if(r58_sock==INVALID_SOCKET)return SOCKET_ERROR;return sendto(r58_sock,(const char*)p,n,0,(const sockaddr*)&to,sizeof(to));}
static void r58_send_control(DWORD now){
    if(!r58_nat_enabled||r58_sock==INVALID_SOCKET||!r58_nat_room[0])return;
    DWORD interval=r58_nat_have_self?500U:300U;if(r58_nat_last_control&&now-r58_nat_last_control<interval)return;
    char m[180];if(r58_nat_have_self){char c[17];r58_cookie_text(r58_nat_self,c);_snprintf(m,sizeof(m)-1,"MKNAT1|KEEP|%s",c);}else if(r58_nat_is_host)_snprintf(m,sizeof(m)-1,"MKNAT1|HOST|%s|%s",r58_nat_room,r58_nat_auth1);else _snprintf(m,sizeof(m)-1,"MKNAT1|JOIN|%s|%s|%s",r58_nat_room,r58_nat_auth1,r58_nat_auth2);m[sizeof(m)-1]=0;sockaddr_in s=r58_server_addr();r58_raw_send(s,m,(int)strlen(m));r58_nat_last_control=now;
}
static void r58_send_punches(DWORD now){
    if(!r58_nat_enabled||!r58_nat_have_self)return;char self[17];r58_cookie_text(r58_nat_self,self);char m[48];_snprintf(m,sizeof(m)-1,"MKNAT1|PUNCH|%s",self);m[sizeof(m)-1]=0;
    for(unsigned i=0;i<mknet::MAX_PLAYERS;++i){R58NatRoute &r=r58_nat_routes[i];if(!r.used)continue;if(r.direct_seen&&r.direct_valid_rx>=3U&&now-r.last_direct_rx<1500U)continue;if(r.last_punch&&now-r.last_punch<120U)continue;r58_raw_send(r.addr,m,(int)strlen(m));r.last_punch=now;}
}
static void r58_nat_tick(DWORD now){r58_send_control(now);r58_send_punches(now);}
static void r58_mark_direct(const sockaddr_in &from){int i=r58_route_addr(from);if(i>=0){R58NatRoute &r=r58_nat_routes[i];if(!r.direct_seen)net_log("R60_NAT: validated direct MK4P path\n");r.direct_seen=true;if(r.direct_valid_rx<0xFFFFFFFFU)++r.direct_valid_rx;r.last_direct_rx=GetTickCount();}}
static bool r58_handle_control(const uint8_t *p,int n,const sockaddr_in &from){
    if(n<7||memcmp(p,"MKNAT1|",7)!=0)return false;char b[220];int c=n<(int)sizeof(b)-1?n:(int)sizeof(b)-1;memcpy(b,p,c);b[c]=0;
    if(!strncmp(b,"MKNAT1|SELF|",12)&&r58_from_server(from)){char hex[17]={0};unsigned rid=0;if(sscanf(b,"MKNAT1|SELF|%16[^|]|%u",hex,&rid)==2&&rid>0&&rid<256){uint8_t ck[8];if(r58_parse_cookie(hex,ck)){memcpy(r58_nat_self,ck,8);r58_nat_self_id=(uint8_t)rid;if(!r58_nat_have_self)net_log("R58_NAT: rendezvous registered relay_id=%u\n",rid);r58_nat_have_self=true;}}return true;}
    if(!strcmp(b,"MKNAT1|RESET")&&r58_from_server(from)){r58_nat_have_self=false;r58_nat_self_id=0;memset(r58_nat_self,0,sizeof(r58_nat_self));memset(r58_nat_routes,0,sizeof(r58_nat_routes));r58_nat_last_control=0;net_log("R58.2_NAT: server route reset - re-registering\n");return true;}
    if(!strncmp(b,"MKNAT1|INTRO|",13)&&r58_from_server(from)){char hex[17]={0},ip[32]={0},role[4]={0};unsigned rid=0,port=0;if(sscanf(b,"MKNAT1|INTRO|%16[^|]|%u|%31[^|]|%u|%3s",hex,&rid,ip,&port,role)==5&&rid>0&&rid<256){uint8_t ck[8];sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=inet_addr(ip);a.sin_port=htons((u_short)port);if(a.sin_addr.s_addr!=INADDR_NONE&&r58_parse_cookie(hex,ck)){int ri=r58_route_install(ck,a);if(ri>=0)r58_nat_routes[ri].relay_id=(uint8_t)rid;if(!r58_nat_is_host&&role[0]=='H'){join_target=a;host_peer=a;}net_log("R58_NAT: peer introduced role=%s relay_id=%u port=%u\n",role,rid,port);}}return true;}
    if(!strncmp(b,"MKNAT1|PUNCH|",13)&&!r58_from_server(from)){uint8_t ck[8];if(r58_parse_cookie(b+13,ck)){int i=r58_route_cookie(ck);if(i>=0){R58NatRoute &r=r58_nat_routes[i];if(!r58_same_address(r.addr,from)){sockaddr_in old=r.addr;r.addr=from;r58_rebind_known(old,from);r.direct_seen=false;r.direct_valid_rx=0;r.last_direct_rx=0;}/* R60: punch is only a reachability hint; relay stays active until valid MK4P. */net_log("R60_NAT: direct punch hint received; relay remains armed\n");}}return true;}
    return true;
}
static int r58_route_relay_id(unsigned rid){for(unsigned i=0;i<mknet::MAX_PLAYERS;++i)if(r58_nat_routes[i].used&&r58_nat_routes[i].relay_id==rid)return (int)i;return -1;}
static bool r58_relay_unwrap(uint8_t *p,int &n,sockaddr_in &from){
    if(n<4||p[0]!='M'||p[1]!='R'||!r58_from_server(from)||!r58_nat_have_self)return false;unsigned src=p[2],dst=p[3];if(dst!=r58_nat_self_id)return true;int i=r58_route_relay_id(src);if(i<0)return true;unsigned len=(unsigned)n-4U;if(len>mknet::MAX_PACKET)return true;R58NatRoute &r=r58_nat_routes[i];memmove(p,p+4,len);n=(int)len;from=r.addr;if(!r.relay_rx)net_log("R58.2_NAT: relay fallback receiving gameplay\n");++r.relay_rx;return true;
}
static int r58_relay_send(R58NatRoute &r,const uint8_t *p,int n){
    if(!r58_nat_enabled||!r58_nat_have_self||!r58_nat_self_id||!r.relay_id||n<=0||n>(int)mknet::MAX_PACKET)return SOCKET_ERROR;uint8_t b[mknet::MAX_PACKET+4];b[0]='M';b[1]='R';b[2]=r58_nat_self_id;b[3]=r.relay_id;memcpy(b+4,p,n);sockaddr_in s=r58_server_addr();int rc=r58_raw_send(s,b,n+4);if(rc!=SOCKET_ERROR){if(!r.relay_tx)net_log("R58.2_NAT: relay fallback sending gameplay\n");++r.relay_tx;}return rc;
}
static bool r60_force_relay_type(mknet::Type t){
    return t==mknet::HELLO||t==mknet::OFFER||t==mknet::READY||t==mknet::START||
           t==mknet::START_ACK||t==mknet::JOIN_REJECT||t==mknet::BOOT_READY||
           t==mknet::BOOT_GO||t==mknet::GOODBYE;
}
static int r58_send_game_packet(const sockaddr_in &to,const uint8_t *p,int n,bool allow_relay,bool force_relay=false){
    SOCKET io=r58_io_socket();if(io==INVALID_SOCKET)return SOCKET_ERROR;int direct=sendto(io,(const char*)p,n,0,(const sockaddr*)&to,sizeof(to));int relay=SOCKET_ERROR;int ri=r58_route_addr(to);if(allow_relay&&ri>=0){R58NatRoute &r=r58_nat_routes[ri];DWORD now=GetTickCount();bool direct_stable=r.direct_seen&&r.direct_valid_rx>=3U&&now-r.last_direct_rx<=1500U;if(force_relay||!direct_stable)relay=r58_relay_send(r,p,n);}return direct!=SOCKET_ERROR?direct:relay;
}

static int send_packet_to(const sockaddr_in &to,mknet::Type t,const uint8_t sid[16],const uint8_t *payload,int size){
    if(sock==INVALID_SOCKET||size<0||size>mknet::MAX_PACKET-mknet::HEADER||(size&&!payload))return SOCKET_ERROR;
    uint8_t p[mknet::MAX_PACKET];int n=mknet::header(p,t,sid,size);
    if(size)memcpy(p+mknet::HEADER,payload,size);
    bool force_relay=r58_nat_enabled&&(!active||r60_force_relay_type(t));
    return r58_send_game_packet(to,p,n,t!=mknet::PEER_PROBE,force_relay);
}
static int send_peer_message(int idx,mknet::Type t,const uint8_t *payload,int size){
    int n=peer_count();if(idx<0||idx>=n)return SOCKET_ERROR;
    return send_packet_to(peers[idx].addr,t,session,payload,size);
}
static int send_host_message(mknet::Type t,const uint8_t *payload,int size){
    if(!host_session_known&&t!=mknet::HELLO)return SOCKET_ERROR;
    return send_packet_to(host_peer,t,t==mknet::HELLO?nonce:session,payload,size);
}

static void mesh_reset(){memset(mesh_peers,0,sizeof(mesh_peers));memset(mesh_reports,0,sizeof(mesh_reports));mesh_direct_tx=mesh_direct_rx=mesh_probe_tx=mesh_probe_rx=0;mesh_last_announce=0;}
static int mesh_find_slot(unsigned slot,unsigned locals){for(unsigned i=0;i<mknet::MAX_PLAYERS-1;++i)if(mesh_peers[i].used&&mesh_peers[i].slot==slot&&mesh_peers[i].local_count==locals)return (int)i;return -1;}
static int mesh_alloc_slot(unsigned slot,unsigned locals){int m=mesh_find_slot(slot,locals);if(m>=0)return m;for(unsigned i=0;i<mknet::MAX_PLAYERS-1;++i)if(!mesh_peers[i].used){memset(&mesh_peers[i],0,sizeof(mesh_peers[i]));mesh_peers[i].used=true;mesh_peers[i].slot=slot;mesh_peers[i].local_count=locals;return (int)i;}return -1;}
static void mesh_install_info(const uint8_t *q){
    unsigned slot=q[0],locals=(unsigned)q[1]+1U;unsigned mySlot=active?local_slot:assigned_slot;if(slot==mySlot||(slot<mySlot+local_count&&slot+locals>mySlot))return;int mi=mesh_alloc_slot(slot,locals);if(mi<0)return;
    MeshPeerState &m=mesh_peers[mi];memset(&m.addr,0,sizeof(m.addr));m.addr.sin_family=AF_INET;m.addr.sin_addr.s_addr=htonl(mknet::get32(q+2));m.addr.sin_port=htons((u_short)(((unsigned)q[6]<<8)|q[7]));m.last_probe=0;
    net_log("R56_MESH: peer P%u locals=%u announced\n",slot+1,locals);
}
static void send_mesh_info_to(int target){
    if(lobby_slots()<=2||peer_count()<2||target<0||target>=peer_count())return;
    for(int j=0;j<peer_count();++j){if(j==target)continue;const PeerState &ps=peers[j];uint8_t q[8];q[0]=(uint8_t)ps.slot;q[1]=(uint8_t)(ps.local_count-1);mknet::put32(q+2,ntohl(ps.addr.sin_addr.s_addr));unsigned port=ntohs(ps.addr.sin_port);q[6]=(uint8_t)(port>>8);q[7]=(uint8_t)port;send_peer_message(target,mknet::PEER_INFO,q,sizeof(q));}
}
static void mesh_send_report(const MeshPeerState &m){if(hosting||!host_session_known||!m.latency.samples)return;uint8_t q[8];memset(q,0,sizeof(q));q[0]=(uint8_t)(active?local_slot:assigned_slot);q[1]=(uint8_t)m.slot;q[2]=(uint8_t)(m.latency.samples>255U?255U:m.latency.samples);q[3]=m.direct_seen?1:0;mknet::put32(q+4,m.latency.budget());send_packet_to(host_peer,mknet::MESH_REPORT,session,q,sizeof(q));}
static void mesh_send_probes(DWORD now){
    if(hosting||!host_session_known)return;unsigned mySlot=active?local_slot:assigned_slot;if(!mySlot)return;
    for(unsigned i=0;i<mknet::MAX_PLAYERS-1;++i){MeshPeerState &m=mesh_peers[i];if(!m.used||(m.last_probe&&now-m.last_probe<100U))continue;uint8_t q[8];memset(q,0,sizeof(q));q[0]=(uint8_t)mySlot;q[1]=(uint8_t)(local_count-1);q[2]=0;mknet::put32(q+4,now);if(send_packet_to(m.addr,mknet::PEER_PROBE,session,q,sizeof(q))!=SOCKET_ERROR)++mesh_probe_tx;m.last_probe=now;}
}
static bool r57_mesh_delay(unsigned &outDelay,unsigned relayDelay){int n=peer_count();if(n<2){outDelay=relayDelay;return false;}unsigned worst=0;for(int i=0;i<n;++i){unsigned b=peers[i].latency.budget();if(b>worst)worst=b;}for(int i=0;i<n;++i)for(int j=i+1;j<n;++j){unsigned a=peers[i].slot,b=peers[j].slot;const MeshReportState &ab=mesh_reports[a][b],&ba=mesh_reports[b][a];if(!ab.seen||!ba.seen||ab.samples<3U||ba.samples<3U){outDelay=relayDelay;return false;}unsigned pair=ab.budget>ba.budget?ab.budget:ba.budget;if(pair>worst)worst=pair;}unsigned md=mknet::input_delay_2p(worst);outDelay=md<relayDelay?md:relayDelay;return true;}

/* MK64_R59_1_LOBBY_PRESENCE_SMOOTHNESS */
static void close_network(){
    r62_end_game();
    /* MK360_PARTY_CLEAR_ON_CLOSE_V8 */
    if(hosting)x360_party_clear_host();
    if(sock!=INVALID_SOCKET){
        if(hosting){
            int n=peer_count();
            for(int i=0;i<n;++i)for(int retry=0;retry<3;++retry)send_peer_message(i,mknet::GOODBYE,0,0);
        }else if(host_session_known){
            for(int retry=0;retry<3;++retry)send_host_message(mknet::GOODBYE,0,0);
        }else if(join_target.sin_family==AF_INET&&join_target.sin_port!=0){
            for(int retry=0;retry<3;++retry)send_packet_to(join_target,mknet::GOODBYE,nonce,0,0);
        }
        if(mk_stress_enabled()){
            unsigned davg=mk_stress_delay_samples?mk_stress_delay_sum/mk_stress_delay_samples:0;
            unsigned dmin=mk_stress_delay_samples?mk_stress_delay_min:0;
            net_log("MK64NET4: STRESS summary rtt=%u jitter=+-%u loss=%u%% delay=%u/%u/%u ms queued=%u sent=%u dropped=%u overflow=%u\n",
                    mk_stress_rtt_ms(),mk_stress_jitter_ms(),mk_stress_loss_pct(),dmin,davg,mk_stress_delay_max,
                    mk_stress_queued,mk_stress_sent,mk_stress_dropped,mk_stress_overflow);
        }
        closesocket(sock);sock=INVALID_SOCKET;
    }
    mk_stress_clear_queue();
    crossplay_states.reset();
    active=start_sent=failed=false;boot.reset(1);host_session_known=false;assigned_slot=0;player_count=1;local_slot=0;local_count=1;session_split=false;join_full=false;crossplay=false;host_platform=mknet::PLATFORM_UNKNOWN;have_host_commit=false;host_commit_frame=0;crossplay_host_required=false;
    split_diag_samples=0;split_diag_last_c2=-1;mesh_reset();r58_nat_reset();
    memset(&host_peer,0,sizeof(host_peer));memset(&join_target,0,sizeof(join_target));
    for(unsigned i=0;i<mknet::MAX_PLAYERS-1;++i)reset_peer(peers[i]);
    unmap_router();
}

static bool open_network(bool host){
    close_network();hosting=host;reset_net_counters();mk_stress_reset_stats();
    net_log(host?"MK64NET4: host requested (up to 4 players)\n":"MK64NET4: join requested\n");
    if(!initialized){
        if(!mk_nd_resolve()){net_log("MK64NET4: MK64 NetDll export resolve FAILED\n");return false;}
        mk64_xnc=MK64_XNC_TITLE;
        net_log("MK64NET4: MK64 NetDll exports OK xnc=%u startup=%s wsa=%s ver=0x20530800\n",
                (unsigned)mk64_xnc,mk_nd_xnetstartup_new?"ord50":"ord33",mk_nd_wsastartup_new?"ord24":"ord01");
        XNetStartupParams p;memset(&p,0,sizeof(p));p.cfgSizeOfStruct=sizeof(p);p.cfgFlags=XNET_STARTUP_BYPASS_SECURITY;
        int xe=XNetStartup(&p);if(xe){net_log("MK64NET4: XNetStartup failed err=%d\n",xe);return false;}
        WSADATA w;int we=WSAStartup(MAKEWORD(2,2),&w);if(we){net_log("MK64NET4: WSAStartup failed err=%d\n",we);XNetCleanup();return false;}
        initialized=true;net_log("MK64NET4: MK64 NetDll XNet/WSA ready\n");
    }
    sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(sock==INVALID_SOCKET){net_log("MK64NET4: socket failed wsa=%d\n",WSAGetLastError());return false;}
    /*
     * MK64 INTERNET SOCKET SETUP:
     * Keep the game UDP socket on the exact MK64 NetDll socket setup that was
     * proven to discover the public STUN endpoint and accept Internet peers
     * in the confirmed-working 2-player build.  Earlier experimental builds added 0x5802 here;
     * that was not part of the proven setup.
     */
    BOOL rawSocket=TRUE;
    int opt5801=setsockopt(sock,SOL_SOCKET,0x5801,(PCSTR)&rawSocket,sizeof(rawSocket));
    int err5801=(opt5801==SOCKET_ERROR)?WSAGetLastError():0;
    net_log("MK64NET4: MK64 socket opt 5801=%s(%d) xnc=%u [MK64 Internet]\n",
            opt5801==0?"OK":"FAIL",err5801,(unsigned)mk64_xnc);

    /*
     * MK64 SYSAPP compatibility fallback.  A title-caller socket can be
     * denied the private 0x5801 option with WSAEACCES (10013) even though
     * XNet/WSA startup itself succeeds.  Recreate the complete NetDll context
     * as SYSAPP rather than mixing caller IDs on one socket.
     */
    if(opt5801==SOCKET_ERROR && err5801==10013 && mk64_xnc==MK64_XNC_TITLE){
        net_log("MK64NET4: 5801 denied for TITLE; retrying as SYSAPP xnc=2 [MK64 SYSAPP compat]\n");
        closesocket(sock);sock=INVALID_SOCKET;
        mk64_xnc=MK64_XNC_SYSAPP;

        XNetStartupParams p2;memset(&p2,0,sizeof(p2));p2.cfgSizeOfStruct=sizeof(p2);p2.cfgFlags=XNET_STARTUP_BYPASS_SECURITY;
        /* SYSAPP homebrew convention: prefer legacy XNetStartup (ord33). */
        int xe2=mk_nd_xnetstartup_old
            ?mk_nd_xnetstartup_old(MK64_XNC_SYSAPP,&p2)
            :mk_nd_xnetstartup_new(MK64_XNC_SYSAPP,&p2,MK64_NETDLL_VERSION);
        if(xe2){
            net_log("MK64NET4: SYSAPP XNetStartup failed err=%d api=%s\n",xe2,mk_nd_xnetstartup_old?"ord33":"ord50");
            mk64_xnc=MK64_XNC_TITLE;
            return false;
        }
        WSADATA w2;
        /* NetDll_WSAStartupEx SYSAPP examples use final version/context arg 2. */
        int we2=mk_nd_wsastartup_new
            ?mk_nd_wsastartup_new(MK64_XNC_SYSAPP,MAKEWORD(2,2),&w2,2)
            :mk_nd_wsastartup_old(MK64_XNC_SYSAPP,MAKEWORD(2,2),&w2);
        if(we2){
            net_log("MK64NET4: SYSAPP WSAStartup failed err=%d api=%s\n",we2,mk_nd_wsastartup_new?"ord24":"ord01");
            mk64_xnc=MK64_XNC_TITLE;
            return false;
        }

        sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(sock==INVALID_SOCKET){
            net_log("MK64NET4: SYSAPP socket failed wsa=%d\n",WSAGetLastError());
            mk64_xnc=MK64_XNC_TITLE;
            return false;
        }
        opt5801=setsockopt(sock,SOL_SOCKET,0x5801,(PCSTR)&rawSocket,sizeof(rawSocket));
        err5801=(opt5801==SOCKET_ERROR)?WSAGetLastError():0;
        net_log("MK64NET4: SYSAPP socket opt 5801=%s(%d) xnc=%u [MK64 SYSAPP compat]\n",
                opt5801==0?"OK":"FAIL",err5801,(unsigned)mk64_xnc);
    }

    if(opt5801==SOCKET_ERROR){
        net_log("MK64NET4: required MK64 0x5801 socket option failed xnc=%u\n",(unsigned)mk64_xnc);
        closesocket(sock);sock=INVALID_SOCKET;
        mk64_xnc=MK64_XNC_TITLE;
        return false;
    }
    int netbuf=128*1024;
    setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(PCSTR)&netbuf,sizeof(netbuf));
    setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(PCSTR)&netbuf,sizeof(netbuf));
    sockaddr_in bindaddr;memset(&bindaddr,0,sizeof(bindaddr));bindaddr.sin_family=AF_INET;bindaddr.sin_addr.s_addr=htonl(INADDR_ANY);bindaddr.sin_port=htons(6464);
    if(bind(sock,(sockaddr*)&bindaddr,sizeof(bindaddr))==SOCKET_ERROR){net_log("MK64NET4: bind failed port=6464 wsa=%d\n",WSAGetLastError());close_network();return false;}
    u_long nonblocking=1;if(ioctlsocket(sock,FIONBIO,&nonblocking)){net_log("MK64NET4: FIONBIO failed wsa=%d\n",WSAGetLastError());close_network();return false;}
    if(XNetRandom(session,sizeof(session))||XNetRandom(nonce,sizeof(nonce))){net_log("MK64NET4: XNetRandom failed\n");close_network();return false;}
    last_received=GetTickCount();
    if(host)net_log("MK64NET4: UDP 6464 bind OK; waiting for up to 3 guests\n");else net_log("MK64NET4: join UDP 6464 bind OK\n");
    return true;
}

/* MK64_R50_1_PUBLIC_MATCH_IP_PRIVACY */
static void public_ip(bool hide_addresses){
    strcpy(public_address,"PUBLIC ADDRESS UNAVAILABLE");strcpy(local_address,"LOCAL ADDRESS UNAVAILABLE");
    XNADDR addr;memset(&addr,0,sizeof(addr));XNetGetTitleXnAddr(&addr);if(addr.ina.s_addr)address_text(local_address,ntohl(addr.ina.s_addr),6464);

    XNDNS *dns=0;
    int dns_start=XNetDnsLookup("stun.cloudflare.com",0,&dns);
    if(dns_start||!dns){
        net_log("MK64NET4: STUN DNS start failed rc=%d dns=%s\n",dns_start,dns?"yes":"no");
        return;
    }
    DWORD begin=GetTickCount();
    while(dns->iStatus==WSAEINPROGRESS&&GetTickCount()-begin<4000){
        if(hide_addresses)screen("PUBLIC MATCH","PREPARING PUBLIC CONNECTION","FINDING INTERNET ROUTE","UDP 6464","PLEASE WAIT");else screen(mknet::lobby_capacity()==8?"HOST 4-8 PLAYER GAME":"HOST 2-4 PLAYER GAME","FINDING INTERNET ADDRESS",local_address,"","");
        if(pressed()&XINPUT_GAMEPAD_B)break;
        Sleep(10);
    }

    int dns_status=dns->iStatus;
    int dns_count=dns->cina;
    sockaddr_in stun;memset(&stun,0,sizeof(stun));stun.sin_family=AF_INET;stun.sin_port=htons(3478);
    bool found=dns_status==0&&dns_count>0;
    if(found)stun.sin_addr=dns->aina[0];
    XNetDnsRelease(dns);
    if(!found){
        net_log("MK64NET4: STUN DNS failed status=%d count=%d\n",dns_status,dns_count);
        return;
    }

    char stun_text[80];peer_text(stun_text,stun);
    net_log("MK64NET4: STUN target %s\n",stun_text);

    uint8_t request[20]={0,1,0,0,0x21,0x12,0xA4,0x42};
    if(XNetRandom(request+8,12)){
        net_log("MK64NET4: STUN transaction random generation failed\n");
        return;
    }

    begin=GetTickCount();DWORD sent=0;unsigned sends=0;
    while(GetTickCount()-begin<3500){
        if(!sent||GetTickCount()-sent>500){
            int sr=sendto(sock,(char*)request,20,0,(sockaddr*)&stun,sizeof(stun));
            ++sends;
            if(sr==SOCKET_ERROR&&sends<=4)net_log("MK64NET4: STUN send failed try=%u wsa=%d\n",sends,WSAGetLastError());
            sent=GetTickCount();
        }
        uint8_t p[1024];sockaddr_in from;int len=sizeof(from);
        int n=recvfrom(sock,(char*)p,sizeof(p),0,(sockaddr*)&from,&len);
        uint32_t ip;uint16_t port;
        if(n>0&&same_address(from,stun)&&mknet::stun_address(p,n,request+8,ip,port)){
            address_text(public_address,ip,port);
            net_log("MK64NET4: STUN public endpoint=%s\n",public_address);
            return;
        }
        if(hide_addresses)screen("PUBLIC MATCH","PREPARING PUBLIC CONNECTION","FINDING INTERNET ROUTE","UDP 6464","PLEASE WAIT");else screen(mknet::lobby_capacity()==8?"HOST 4-8 PLAYER GAME":"HOST 2-4 PLAYER GAME","FINDING INTERNET ADDRESS",local_address,"","");
        if(pressed()&XINPUT_GAMEPAD_B)break;
        Sleep(10);
    }
    net_log("MK64NET4: STUN timed out after %u sends\n",sends);
}

static void send_offer(int idx){
    if(idx<0||idx>=peer_count())return;
    PeerState &ps=peers[idx];uint8_t payload[24];memset(payload,0,sizeof(payload));
    memcpy(payload,ps.nonce,16);DWORD stamp=GetTickCount();mknet::put32(payload+16,stamp);payload[20]=uint8_t(ps.slot);payload[21]=uint8_t(lobby_slots());payload[22]=uint8_t(ps.local_count-1);payload[23]=uint8_t(mknet::PLATFORM_XBOX360);
    send_peer_message(idx,mknet::OFFER,payload,sizeof(payload));ps.last_offer=stamp;
}
static void send_start(int idx){
    if(idx<0||idx>=peer_count())return;
    uint8_t payload[6];payload[0]=uint8_t(chosen_delay);payload[1]=uint8_t(player_count);payload[2]=uint8_t(peers[idx].slot);payload[3]=uint8_t(peers[idx].local_count-1);payload[4]=session_split?1:0;payload[5]=crossplay?1:0;
    send_peer_message(idx,mknet::START,payload,sizeof(payload));
    send_mesh_info_to(idx);
}
static void send_ready_from_offer(const uint8_t *offer_payload){uint8_t payload[24];memcpy(payload,offer_payload,24);send_host_message(mknet::READY,payload,24);}

static void pump(){
    SOCKET io=r58_io_socket();if(io==INVALID_SOCKET)return;
    mk_stress_flush_due();r58_nat_tick(GetTickCount());
    for(int loop=0;loop<96;++loop){
        uint8_t p[mknet::MAX_PACKET+23];sockaddr_in from;int fl=sizeof(from);int n=recvfrom(io,(char*)p,sizeof(p)-1,0,(sockaddr*)&from,&fl);
        if(n==SOCKET_ERROR){int e=WSAGetLastError();if(e!=WSAEWOULDBLOCK&&e!=WSAEMSGSIZE){if(e==10051||e==10052||e==10054||e==10065){net_log("R58.2_NAT: transient UDP recv error wsa=%d ignored\n",e);break;}net_log("MK64NET4: recvfrom failed wsa=%d\n",e);failed=true;}break;}
        if(r58_handle_control(p,n,from))continue;bool relayed=false;if(n>=2&&p[0]=='M'&&p[1]=='R'){if(!r58_relay_unwrap(p,n,from))continue;relayed=true;}
        ++net_rx_total;
        if(n<mknet::HEADER){if(net_rx_total<=8)net_log("MK64NET4: short datagram bytes=%d dropped\n",n);continue;}
        if(!mknet::valid(p,n)){if(net_rx_total<=8)net_log("MK64NET4: invalid datagram bytes=%d type=%u\n",n,n>5?(unsigned)p[5]:255U);continue;}
        if(!relayed)r58_mark_direct(from);
        ++net_rx_valid;
        if(p[5]==mknet::HELLO)++net_rx_hello;else if(p[5]==mknet::OFFER)++net_rx_offer;else if(p[5]==mknet::READY)++net_rx_ready;else if(p[5]==mknet::START)++net_rx_start;else if(p[5]==mknet::START_ACK)++net_rx_ack;
        uint8_t *q=p+mknet::HEADER;DWORD now=GetTickCount();

        if(hosting){
            if(p[5]==mknet::HELLO&&!active&&!start_sent){
                unsigned requested=q[0],platform=q[1];
                int idx=find_peer_nonce(p+12);
                if(idx<0)idx=find_peer_address(from);
                unsigned previous=idx<0?0:peers[idx].local_count;
                if(!mknet::reservation_fits(lobby_slots(),previous,requested,mknet::lobby_capacity())){
                    uint8_t reason=1;send_packet_to(from,mknet::JOIN_REJECT,p+12,&reason,1);continue;
                }
                idx=find_peer_nonce(p+12);
                if(idx>=0){
                    if(!same_ip(peers[idx].addr,from)&&!r58_nat_enabled){net_log("MK64NET4: HELLO nonce seen from different IP - rejected\n");continue;}
                    if(!same_address(peers[idx].addr,from)){peers[idx].addr=from;peers[idx].ready=false;peers[idx].acked=false;net_log("R58.2_NAT: P%u endpoint rebound during HELLO\n",peers[idx].slot+1);}
                }else{
                    idx=find_peer_address(from);
                    if(idx>=0){
                        memcpy(peers[idx].nonce,p+12,16);peers[idx].ready=false;peers[idx].acked=false;peers[idx].best_rtt=0xFFFFFFFFU;memset(&peers[idx].latency,0,sizeof(peers[idx].latency));
                        net_log("MK64NET4: P%u restarted handshake\n",peers[idx].slot+1);
                    }else{
                        int count=peer_count();
                        if(count>=(int)mknet::lobby_capacity()-1){net_log("MK64NET4: HELLO rejected - lobby full\n");continue;}
                        idx=count;reset_peer(peers[idx]);peers[idx].used=true;peers[idx].addr=from;memcpy(peers[idx].nonce,p+12,16);peers[idx].slot=0;
                        char who[80];peer_text(who,from);net_log("MK64NET4: assigned %s -> P%u\n",who,peers[idx].slot+1);
                    }
                }
                if(peers[idx].local_count!=requested){peers[idx].ready=false;peers[idx].last_offer=0;}
                peers[idx].local_count=requested;peers[idx].platform=platform;assign_slots();
                peers[idx].last_received=now;send_offer(idx);continue;
            }

            /* R59.1: remove a guest immediately when it backs out before START.
             * Before OFFER the header carries its HELLO nonce; after OFFER it
             * carries the session ID. */
            if(!active&&!start_sent&&p[5]==mknet::GOODBYE){
                int leave_idx=find_peer_nonce(p+12);
                if(leave_idx<0&&!memcmp(p+12,session,16))leave_idx=find_peer_address(from);
                if(leave_idx>=0)remove_peer(leave_idx);
                continue;
            }
            if(memcmp(p+12,session,16))continue;
            int idx=find_peer_address(from);
            if(idx<0 && (p[5]==mknet::CLIENT_INPUT || p[5]==mknet::START_ACK)) {
                unsigned claimed=(p[5]==mknet::CLIENT_INPUT)?q[0]:q[0];
                int candidate=-1;for(int j=0;j<peer_count();++j)if(peers[j].slot==claimed){candidate=j;break;}
                if(candidate>=0&&candidate<peer_count()&&same_ip(peers[candidate].addr,from)) {
                    peers[candidate].addr=from;idx=candidate;
                    net_log("MK64NET4: P%u NAT endpoint rebound\n",peers[candidate].slot+1);
                }
            }
            if(idx<0)continue;PeerState &ps=peers[idx];

            if(p[5]==mknet::READY&&!active&&!start_sent){
                if(memcmp(q,ps.nonce,16)||q[20]!=ps.slot||q[22]+1!=ps.local_count){net_log("MK64NET4: READY rejected P%u\n",ps.slot+1);continue;}
                DWORD stamp=mknet::get32(q+16);DWORD rtt=now-stamp;if(stamp!=ps.last_offer||rtt>10000)continue;
                ps.latency.add((unsigned)rtt);
                if(rtt<ps.best_rtt)ps.best_rtt=rtt;ps.ready=true;ps.last_received=now;
                net_log("MK64NET4: P%u READY rtt=%u ms\n",ps.slot+1,(unsigned)rtt);
                net_log("R55_LATENCY: P%u sample=%u rtt=%u mean=%u var=%u budget=%u\n",
                        ps.slot+1,ps.latency.samples,(unsigned)rtt,ps.latency.mean,ps.latency.variation,ps.latency.budget());continue;
            }
            if(!active&&!start_sent&&p[5]==mknet::MESH_REPORT){unsigned reporter=q[0],target=q[1],samples=q[2],direct=q[3];if(reporter!=ps.slot||target==reporter||target>=mknet::lobby_capacity())continue;int ti=-1;for(int k=0;k<peer_count();++k)if(peers[k].slot==target){ti=k;break;}if(ti<0||!direct)continue;MeshReportState &mr=mesh_reports[reporter][target];mr.budget=mknet::get32(q+4);mr.samples=samples;mr.seen=true;ps.last_received=now;if(samples<=3U)net_log("R57_MESH: report P%u->P%u samples=%u budget=%u\n",reporter+1,target+1,samples,mr.budget);continue;}
            if(active&&p[5]==mknet::BOOT_READY){
                if(!boot.ready_span(ps.slot,ps.local_count))continue;ps.last_received=now;
                if(boot.complete)send_peer_message(idx,mknet::BOOT_GO,0,0);
                continue;
            }
            if(p[5]==mknet::START_ACK&&start_sent){
                if(q[0]!=ps.slot)continue;ps.acked=true;ps.last_received=now;net_log("MK64NET4: P%u START_ACK\n",ps.slot+1);continue;
            }
            if((active||start_sent)&&p[5]==mknet::CLIENT_INPUT){
                ++net_rx_input;bool accepted=stream.receive_remote(ps.slot,p,n,ps.local_count);
                if(accepted){
                    ps.last_received=now;ps.gameplay_seen=true;first_gameplay_input=true;if(start_sent)ps.acked=true;
                    if(net_rx_input<=8)net_log("MK64NET4: P%u INPUT accepted frame=%u bytes=%d\n",ps.slot+1,(unsigned)stream.frame,n);
                    /* 3P/4P low latency: relay this guest's input immediately to
                     * every other guest. The complete FRAMESET remains enabled
                     * below as recovery/redundancy and deterministic checking. */
                    if(player_count>2){
                        int pc=peer_count();
                        for(int j=0;j<pc;++j){
                            if(j==idx)continue;
                            int sr=r58_send_game_packet(peers[j].addr,p,n,true);++net_tx_input;
                            if(sr==SOCKET_ERROR){++net_tx_input_fail;if(net_tx_input_fail<=12)net_log("MK64NET4: EARLY_RELAY P%u->P%u fail tx=%u frame=%u wsa=%d\n",ps.slot+1,peers[j].slot+1,net_tx_input,(unsigned)stream.frame,WSAGetLastError());}
                        }
                    }
                }
                else{++net_rx_input_reject;if(net_rx_input_reject<=12)net_log("MK64NET4: P%u INPUT rejected frame=%u fault=%u\n",ps.slot+1,(unsigned)stream.frame,stream.fault?1U:0U);}
                continue;
            }
            if(!active&&!start_sent&&p[5]==mknet::GOODBYE){remove_peer(idx);continue;}
            if((active||start_sent)&&p[5]==mknet::GOODBYE){net_log("MK64NET4: P%u GOODBYE\n",ps.slot+1);if(crossplay&&mknet::rate60()){static bool r72GoCaptured=false;if(!r72GoCaptured){r72GoCaptured=true;net_log("R72_HOST_GO_CAPTURE_V1 frame=%u; dumping recent host ASTRA frames\n",(unsigned)stream.frame);mk64_astra_diag_dump();}}return_to_premenu=true;failed=true;continue;}
        }else{
            if(p[5]==mknet::JOIN_REJECT&&!active&&same_ip(join_target,from)&&!memcmp(p+12,nonce,16)){if(q[0]==2)crossplay_host_required=true;else join_full=true;failed=true;continue;}
            if(p[5]==mknet::OFFER&&!active){
                if(!same_ip(join_target,from))continue;
                if(memcmp(q,nonce,16)){net_log("MK64NET4: OFFER nonce mismatch\n");continue;}
                unsigned slot=q[20];host_platform=q[23];if(slot<1||slot+join_local_count>mknet::lobby_capacity()||q[22]+1!=join_local_count)continue;
                if(host_platform==mknet::PLATFORM_OG_XBOX){crossplay_host_required=true;failed=true;net_log("MK64NET10: Xbox 360 must HOST OG/360 crossplay\n");continue;}
                host_peer=from;memcpy(session,p+12,16);host_session_known=true;assigned_slot=slot;last_received=now;
                send_ready_from_offer(q);net_log("MK64NET10: OFFER accepted; provisional slot P%u hostPlatform=%u\n",assigned_slot+1,host_platform);continue;
            }
            if(!host_session_known||memcmp(p+12,session,16))continue;

            /* R56A optional direct mesh. The normal host path remains below. */
            if(p[5]==mknet::PEER_INFO){if(!same_ip(host_peer,from))continue;mesh_install_info(q);continue;}
            if(p[5]==mknet::PEER_PROBE){unsigned source=q[0],locals=(unsigned)q[1]+1U,reply=q[2];int mi=mesh_find_slot(source,locals);if(mi<0||!same_ip(mesh_peers[mi].addr,from))continue;MeshPeerState &m=mesh_peers[mi];bool first=!m.direct_seen;m.addr=from;m.direct_seen=true;++mesh_probe_rx;if(first)net_log("R57_MESH: direct path P%u established\n",source+1);if(!reply){uint8_t r[8];memset(r,0,sizeof(r));r[0]=(uint8_t)(active?local_slot:assigned_slot);r[1]=(uint8_t)(local_count-1);r[2]=1;mknet::put32(r+4,mknet::get32(q+4));send_packet_to(m.addr,mknet::PEER_PROBE,session,r,sizeof(r));}else{DWORD stamp=mknet::get32(q+4),tn=GetTickCount();if(tn-stamp<=10000U){m.latency.add((unsigned)(tn-stamp));mesh_send_report(m);if(m.latency.samples<=3U)net_log("R57_MESH: P%u direct sample=%u rtt=%u budget=%u\n",source+1,m.latency.samples,(unsigned)(tn-stamp),m.latency.budget());}}continue;}
            if(active&&p[5]==mknet::CLIENT_INPUT){unsigned source=q[0],locals=(unsigned)q[3]+1U;int mi=mesh_find_slot(source,locals);if(mi>=0&&same_address(mesh_peers[mi].addr,from)){++net_rx_input;bool accepted=stream.receive_remote(source,p,n,locals);if(accepted){++mesh_direct_rx;first_gameplay_input=true;if(mesh_direct_rx<=8U)net_log("R56_MESH: DIRECT_INPUT P%u accepted frame=%u\n",source+1,(unsigned)stream.frame);}else ++net_rx_input_reject;continue;}}

            if(!same_address(host_peer,from)) {
                if(same_ip(host_peer,from)&&(p[5]==mknet::START||p[5]==mknet::FRAMESET||p[5]==mknet::CLIENT_INPUT)) {
                    host_peer=from;net_log("MK64NET4: host NAT endpoint rebound\n");
                } else continue;
            }
            if(p[5]==mknet::START){
                unsigned delay=q[0],players=q[1],slot=q[2];if(slot!=assigned_slot||q[3]+1!=join_local_count||slot+join_local_count>players||players>mknet::lobby_capacity()||(mknet::lobby_capacity()==8&&players<4))continue;
                if(!active){chosen_delay=delay;player_count=players;local_slot=slot;local_count=q[3]+1;session_split=q[4]!=0;crossplay=q[5]!=0;if(crossplay&&host_platform!=mknet::PLATFORM_XBOX360){failed=true;continue;}stream.reset(chosen_delay,player_count,local_slot,local_count);boot.reset(player_count);active=true;net_log("MK64NET10: START P%u players=%u delay=%u crossplay=%u\n",local_slot+1,player_count,chosen_delay,crossplay?1U:0U);}
                uint8_t ack=uint8_t(local_slot);send_host_message(mknet::START_ACK,&ack,1);last_received=now;continue;
            }
            if(active&&p[5]==mknet::BOOT_GO){boot.complete=true;last_received=now;continue;}
            if(active&&p[5]==mknet::CLIENT_INPUT){
                unsigned source=q[0];
                if(source>=player_count||source==local_slot){++net_rx_input_reject;continue;}
                ++net_rx_input;bool accepted=stream.receive_remote(source,p,n,q[3]+1);
                if(accepted){last_received=now;first_gameplay_input=true;if(net_rx_input<=16)net_log("MK64NET4: REMOTE_INPUT early accepted P%u frame=%u bytes=%d\n",source+1,(unsigned)stream.frame,n);}
                else{++net_rx_input_reject;if(net_rx_input_reject<=12)net_log("MK64NET4: REMOTE_INPUT early rejected P%u frame=%u fault=%u\n",source+1,(unsigned)stream.frame,stream.fault?1U:0U);}
                continue;
            }
            if(active&&p[5]==mknet::FRAMESET){
                ++net_rx_input;bool accepted=stream.receive_frameset(p,n);
                if(accepted){last_received=now;first_gameplay_input=true;if(crossplay&&menu_sync){uint32_t first=mknet::get32(q+4),last=first+q[1]-1;if(!have_host_commit||last>host_commit_frame){host_commit_frame=last;have_host_commit=true;}}if(net_rx_input<=8)net_log("MK64NET10: FRAMESET accepted frame=%u bytes=%d commit=%u\n",(unsigned)stream.frame,n,have_host_commit?(unsigned)host_commit_frame:0U);}
                else{++net_rx_input_reject;if(net_rx_input_reject<=12)net_log("MK64NET4: FRAMESET rejected frame=%u fault=%u\n",(unsigned)stream.frame,stream.fault?1U:0U);}
                continue;
            }
            if(p[5]==mknet::GOODBYE){net_log("MK64NET4: host GOODBYE\n");return_to_premenu=true;failed=true;continue;}
        }
    }
}

static bool wait_for_xnet_route(){
    DWORD begin=GetTickCount();DWORD lastStatus=0xFFFFFFFF;
    while(GetTickCount()-begin<15000){
        XNADDR a;memset(&a,0,sizeof(a));DWORD st=XNetGetTitleXnAddr(&a);
        if(st!=lastStatus){char ipbuf[64]="0.0.0.0";if(a.ina.s_addr)address_text(ipbuf,ntohl(a.ina.s_addr),6464);net_log("MK64NET4: XNet addr status=0x%08X title=%s\n",st,ipbuf);lastStatus=st;}
        if((st&0x20)!=0&&a.ina.s_addr!=0){char ipbuf[64];address_text(ipbuf,ntohl(a.ina.s_addr),6464);net_log("MK64NET4: Internet route ready status=0x%08X title=%s\n",st,ipbuf);return true;}
        screen("NETWORK INIT","WAITING FOR XNET GATEWAY","PLEASE WAIT","B = CANCEL","");if(pressed()&XINPUT_GAMEPAD_B)return false;Sleep(100);
    }
    net_log("MK64NET4: Internet route NOT ready after 15 seconds\n");return false;
}

/* MK360_PARTY_AUTOJOIN_V8 */
static bool mk360_party_host_lookup(sockaddr_in &out,char *tag,int tagSize) {
    unsigned int ip=0,token=0;
    unsigned short port=0;
    if(!x360_party_find_host(&ip,&port,&token,tag,(unsigned int)tagSize))
        return false;

    memset(&out,0,sizeof(out));
    out.sin_family=AF_INET;
    out.sin_addr.s_addr=htonl(ip);
    out.sin_port=htons(port);

    net_log("MK64NET4: Party host found tag=%s endpoint=%u.%u.%u.%u:%u token=%08X\n",
        tag&&tag[0]?tag:"?",
        (ip>>24)&255,(ip>>16)&255,(ip>>8)&255,ip&255,
        (unsigned)port,(unsigned)token);
    return true;
}

static bool host_ip_editor(sockaddr_in &out){
    DWORD last_party_autoscan=0;
    char partyAutoTag[64];partyAutoTag[0]=0;

    char digits[16]="000.000.000.000";int cursor=0;
    while(true){
        char marker[80];memset(marker,' ',strlen(digits));marker[strlen(digits)]=0;marker[cursor]='^';
        screen(mknet::lobby_capacity()==8?"JOIN 4-8 PLAYER GAME":"JOIN 2-4 PLAYER GAME",digits,marker,"WAIT HERE - ACCEPT HOST PARTY INVITE IN GUIDE","AUTO-CONNECTS AFTER PARTY JOIN - A MANUAL IP");DWORD p=pressed();

        /*
         * V11: The joining player may stay on this screen while accepting the
         * normal Xbox LIVE Party invitation in the Guide overlay.  Once Party
         * membership updates, XPartyGetUserList exposes the host's MK360
         * custom data and we feed the advertised endpoint into the existing
         * UDP join path.
         */
        DWORD partyNow=GetTickCount();
        if(partyNow-last_party_autoscan>=750){
            partyAutoTag[0]=0;
            if(mk360_party_host_lookup(out,partyAutoTag,sizeof(partyAutoTag))){
                char found[96];
                _snprintf(found,sizeof(found)-1,"HOST FOUND: %s",partyAutoTag[0]?partyAutoTag:"PARTY MEMBER");
                found[sizeof(found)-1]=0;
                screen("XBOX PARTY HOST FOUND",found,"PARTY INFO RECEIVED","CONNECTING TO HOST...","");
                Sleep(650);
                return true;
            }
            last_party_autoscan=partyNow;
        }
        if(p&XINPUT_GAMEPAD_B)return false;
        if(p&XINPUT_GAMEPAD_X){
            char partyTag[64];partyTag[0]=0;
            if(mk360_party_host_lookup(out,partyTag,sizeof(partyTag))){
                char found[96];_snprintf(found,sizeof(found)-1,"FOUND HOST: %s",partyTag[0]?partyTag:"PARTY MEMBER");found[sizeof(found)-1]=0;
                screen("PARTY HOST FOUND",found,"CONNECTING WITH MK360 UDP NETCODE","","");
                Sleep(750);
                return true;
            }
            screen("JOIN HOST FROM PARTY","NO REMOTE MK360 HOST FOUND","JOIN THE HOST'S XBOX LIVE PARTY","THEN PRESS X AGAIN","B BACK");
            Sleep(1200);
            x360_menu_consume_current_buttons();
            continue;
        }
        if(p&XINPUT_GAMEPAD_DPAD_LEFT){do{cursor=(cursor+14)%15;}while(digits[cursor]=='.');}
        if(p&XINPUT_GAMEPAD_DPAD_RIGHT){do{cursor=(cursor+1)%15;}while(digits[cursor]=='.');}
        if(p&XINPUT_GAMEPAD_DPAD_UP)digits[cursor]=digits[cursor]=='9'?'0':digits[cursor]+1;
        if(p&XINPUT_GAMEPAD_DPAD_DOWN)digits[cursor]=digits[cursor]=='0'?'9':digits[cursor]-1;
        if(p&XINPUT_GAMEPAD_A){char ep[24];_snprintf(ep,sizeof(ep)-1,"%s:6464",digits);ep[sizeof(ep)-1]=0;uint32_t ip;uint16_t port;if(mknet::parse_endpoint(ep,ip,port)){memset(&out,0,sizeof(out));out.sin_family=AF_INET;out.sin_addr.s_addr=htonl(ip);out.sin_port=htons(6464);return true;}}
        Sleep(16);
    }
}

static void prune_prestart_timeouts(DWORD now){
    if(start_sent)return;
    for(int i=peer_count()-1;i>=0;--i)if(now-peers[i].last_received>8000U)remove_peer(i);
}

/* SPLIT_AUTO_REMAP_V2
 * A local split-screen console owns two logical local slots whenever any
 * additional physical Xbox controller is connected. */
static int split_find_extra_controller(void){
    for(DWORD user=1;user<4;++user){
        XINPUT_STATE s;memset(&s,0,sizeof(s));
        if(XInputGetState(user,&s)==ERROR_SUCCESS)return (int)user;
    }
    return -1;
}

static unsigned split_physical_mask(void){
    unsigned mask=0;
    for(DWORD user=0;user<4;++user){
        XINPUT_STATE s;memset(&s,0,sizeof(s));
        if(XInputGetState(user,&s)==ERROR_SUCCESS)mask|=1U<<user;
    }
    return mask;
}

static bool host_players_menu(void){
    x360_menu_consume_current_buttons();split_diag_last_c2=-1;
    for(;;){
        if(r59_world_ui_active)r57_presence_tick(true);
        const int extra=split_find_extra_controller();
        const bool connected=extra>=0;
        host_local_count=connected?2U:1U;
        const int state=connected?extra:-1;

        if(state!=split_diag_last_c2){
            net_log("SPLIT_DIAG_V2: host auto local_count=%u extra_physical=%d mask=%X\n",
                    host_local_count,extra,split_physical_mask());
            split_diag_last_c2=state;
        }

        char detect[96],hint[96];
        if(connected){
            _snprintf(detect,sizeof(detect)-1,
                      "EXTRA CONTROLLER DETECTED: XINPUT USER %d",extra);
            _snprintf(hint,sizeof(hint)-1,
                      "AUTO ASSIGN: P1 + P2 ON THIS CONSOLE");
        }else{
            _snprintf(detect,sizeof(detect)-1,
                      "NO EXTRA CONTROLLER DETECTED");
            _snprintf(hint,sizeof(hint)-1,
                      "CONNECT ANOTHER CONTROLLER FOR LOCAL P2");
        }
        detect[sizeof(detect)-1]=0;hint[sizeof(hint)-1]=0;

        screen("HOST - PLAYERS ON THIS CONSOLE",
            host_local_count==1?"> 1 PLAYER - FULL SCREEN":"  1 PLAYER - FULL SCREEN",
            host_local_count==2?"> 2 PLAYERS - SPLIT SCREEN":"  2 PLAYERS - SPLIT SCREEN",
            detect,hint,
            "A CONTINUE    B BACK",
            "LOCAL PLAYER COUNT IS AUTO-DETECTED");

        DWORD p=pressed();
        if(r59_world_ui_active&&(p&XINPUT_GAMEPAD_RIGHT_THUMB)){r59_world_chat_view();x360_menu_consume_current_buttons();continue;}
        if(p&XINPUT_GAMEPAD_B)return false;
        if(p&XINPUT_GAMEPAD_A){
            net_log("SPLIT_DIAG_V2: host confirmed local_count=%u extra_physical=%d mask=%X\n",
                    host_local_count,extra,split_physical_mask());
            x360_menu_consume_current_buttons();return true;
        }
        Sleep(16);
    }
}

static bool join_players_menu(void){
    x360_menu_consume_current_buttons();split_diag_last_c2=-1;
    for(;;){
        if(r59_world_ui_active)r57_presence_tick(true);
        const int extra=split_find_extra_controller();
        const bool connected=extra>=0;
        join_local_count=connected?2U:1U;
        const int state=connected?extra:-1;

        if(state!=split_diag_last_c2){
            net_log("SPLIT_DIAG_V2: join auto local_count=%u extra_physical=%d mask=%X\n",
                    join_local_count,extra,split_physical_mask());
            split_diag_last_c2=state;
        }

        char detect[96],hint[96];
        if(connected){
            _snprintf(detect,sizeof(detect)-1,
                      "EXTRA CONTROLLER DETECTED: XINPUT USER %d",extra);
            _snprintf(hint,sizeof(hint)-1,
                      "AUTO ASSIGN: TWO ONLINE RACER SLOTS");
        }else{
            _snprintf(detect,sizeof(detect)-1,
                      "NO EXTRA CONTROLLER DETECTED");
            _snprintf(hint,sizeof(hint)-1,
                      "CONNECT ANOTHER CONTROLLER FOR LOCAL PLAYER 2");
        }
        detect[sizeof(detect)-1]=0;hint[sizeof(hint)-1]=0;

        screen("JOIN - PLAYERS ON THIS CONSOLE",
            join_local_count==1?"> 1 PLAYER - FULL SCREEN":"  1 PLAYER - FULL SCREEN",
            join_local_count==2?"> 2 PLAYERS - SPLIT SCREEN":"  2 PLAYERS - SPLIT SCREEN",
            detect,hint,
            "A CONTINUE    B BACK",
            "LOCAL PLAYER COUNT IS AUTO-DETECTED");

        DWORD p=pressed();
        if(r59_world_ui_active&&(p&XINPUT_GAMEPAD_RIGHT_THUMB)){r59_world_chat_view();x360_menu_consume_current_buttons();continue;}
        if(p&XINPUT_GAMEPAD_B)return false;
        if(p&XINPUT_GAMEPAD_A){
            net_log("SPLIT_DIAG_V2: join confirmed local_count=%u extra_physical=%d mask=%X\n",
                    join_local_count,extra,split_physical_mask());
            x360_menu_consume_current_buttons();return true;
        }
        Sleep(16);
    }
}

static void crossplay_release_gate(){
    if(!crossplay){x360_menu_consume_current_buttons();return;}
    unsigned stable=0;
    while(stable<4){
        bool down=false;
        for(unsigned i=0;i<local_count&&i<4;++i){XINPUT_STATE st;memset(&st,0,sizeof(st));if(XInputGetState(i,&st)==ERROR_SUCCESS){const XINPUT_GAMEPAD &g=st.Gamepad;if(g.wButtons||g.bLeftTrigger>0x20||g.bRightTrigger>0x20)down=true;}}
        screen("CROSSPLAY SYNC","RELEASE ALL BUTTONS","XBOX 360 IS AUTHORITATIVE HOST","STARTING NORMAL MK64 MENUS",down?"WAITING FOR RELEASE":"SYNC READY");
        if(down)stable=0;else ++stable;
        pump();Sleep(16);
    }
    x360_menu_consume_current_buttons();
}

/* MK64_R47_PUBLIC_MATCH
 * Unranked lobby directory only. Gameplay remains direct UDP 6464 P2P.
 * Local test directory: 172.233.145.244:6465. */
static const char *r47_directory_ip="172.233.145.244";
static const unsigned r47_directory_port=6465;
static bool r47_public_mode=false;
static bool r47_room_registered=false;
static char r47_room_id[20]={0};
static char r47_room_token[40]={0};
static DWORD r47_last_heartbeat=0;

struct R47Room360{char id[20],name[40],ip[32],platform[16];unsigned port,players,max_players;};

static void r47_consume_buttons(void){XINPUT_STATE s;memset(&s,0,sizeof(s));XInputGetState(0,&s);prev_buttons=s.Gamepad.wButtons;sUiRightTriggerPrev=(s.Gamepad.bRightTrigger>0x40);}

static bool r47_directory_addr(sockaddr_in &a){
    memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((u_short)r47_directory_port);
    a.sin_addr.s_addr=inet_addr(r47_directory_ip);return a.sin_addr.s_addr!=INADDR_NONE;
}
static int r47_send(const char *msg){sockaddr_in a;if(sock==INVALID_SOCKET||!msg||!r47_directory_addr(a))return SOCKET_ERROR;return sendto(sock,msg,(int)strlen(msg),0,(const sockaddr*)&a,sizeof(a));}

static bool r47_register_room(void){
    char msg[192],buf[256];
    _snprintf(msg,sizeof(msg)-1,"MKDIR1|REGISTER|X360 ROOM|6464|%u|4|X360|0|10|BE100927",local_count);msg[sizeof(msg)-1]=0;
    DWORD begin=GetTickCount(),last=0;
    while(GetTickCount()-begin<1800U){
        DWORD now=GetTickCount();if(!last||now-last>=300U){r47_send(msg);last=now;}
        sockaddr_in from;int flen=sizeof(from);int n=recvfrom(sock,buf,sizeof(buf)-1,0,(sockaddr*)&from,&flen);
        if(n>0){buf[n]=0;char id[20]={0},tok[40]={0};if(sscanf(buf,"MKDIR1|REGOK|%19[^|]|%39s",id,tok)==2){strncpy(r47_room_id,id,sizeof(r47_room_id)-1);strncpy(r47_room_token,tok,sizeof(r47_room_token)-1);r47_room_registered=true;r47_last_heartbeat=0;return true;}}
        screen("MARIO KART HUB","CONNECTING X360 ROOM","MADE BY SIRDANKZ - SIRDANKZ CODE: MPL-2.0","UNRANKED 2-4 PLAYERS","B CANCEL");
        if(pressed()&XINPUT_GAMEPAD_B)return false;Sleep(10);
    }
    return false;
}
static void r47_heartbeat(void){
    if(!r47_room_registered||sock==INVALID_SOCKET)return;DWORD now=GetTickCount();if(r47_last_heartbeat&&now-r47_last_heartbeat<4000U)return;
    char msg[160];_snprintf(msg,sizeof(msg)-1,"MKDIR1|HEARTBEAT|%s|%s|%u",r47_room_id,r47_room_token,lobby_slots());msg[sizeof(msg)-1]=0;r47_send(msg);r47_last_heartbeat=now;
}
static void r47_unregister_room(void){
    if(r47_room_registered&&sock!=INVALID_SOCKET){char msg[160];_snprintf(msg,sizeof(msg)-1,"MKDIR1|UNREGISTER|%s|%s",r47_room_id,r47_room_token);msg[sizeof(msg)-1]=0;r47_send(msg);}
    r47_room_registered=false;r47_room_id[0]=0;r47_room_token[0]=0;r47_last_heartbeat=0;
}
static int r47_fetch_rooms(R47Room360 rooms[8]){
    const char *q="MKDIR1|LIST|10|BE100927|0";int count=0;DWORD begin=GetTickCount(),last=0;bool ended=false;
    while(GetTickCount()-begin<1800U&&!ended){
        DWORD now=GetTickCount();if(!last||now-last>=400U){r47_send(q);last=now;}
        for(;;){char buf[320];sockaddr_in from;int flen=sizeof(from);int n=recvfrom(sock,buf,sizeof(buf)-1,0,(sockaddr*)&from,&flen);if(n<=0)break;buf[n]=0;
            if(!strncmp(buf,"MKDIR1|LISTEND",14)){ended=true;break;}
            if(!strncmp(buf,"MKDIR1|ROOM|",12)&&count<8){R47Room360 r;memset(&r,0,sizeof(r));unsigned ranked=0;
                if(sscanf(buf,"MKDIR1|ROOM|%19[^|]|%39[^|]|%31[^|]|%u|%u|%u|%15[^|]|%u",r.id,r.name,r.ip,&r.port,&r.players,&r.max_players,r.platform,&ranked)==8&&!ranked){
                    /* Reverse crossplay was intentionally abandoned: 360 only joins 360-hosted rooms. */
                    if(!strcmp(r.platform,"X360")){rooms[count++]=r;}
                }}
        }Sleep(10);
    }
    return count;
}
static bool r47_browse_rooms(sockaddr_in &target){
    int selected=0;
    for(;;){R47Room360 rooms[8];memset(rooms,0,sizeof(rooms));int count=r47_fetch_rooms(rooms);
        if(count==0){screen("PUBLIC ROOMS","NO COMPATIBLE X360 ROOMS","","A REFRESH    B BACK","");for(;;){DWORD p=pressed();if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_A)break;Sleep(16);}continue;}
        if(selected>=count)selected=count-1;
        for(;;){char a[96]="",b[96]="",c[96]="",d[96];int first=selected-1;if(first<0)first=0;if(first>count-3)first=count-3;if(first<0)first=0;char *line[3]={a,b,c};
            for(int j=0;j<3;++j){int i=first+j;if(i>=count)continue;_snprintf(line[j],95,"%c %s  %u/%u",i==selected?'>':' ',rooms[i].name,rooms[i].players,rooms[i].max_players);line[j][95]=0;}
            _snprintf(d,sizeof(d)-1,"ROOM %d/%d - UNRANKED",selected+1,count);d[sizeof(d)-1]=0;screen("PUBLIC ROOMS",a,b,c,d,"A JOIN  UP/DOWN  X REFRESH  B BACK");
            DWORD p=pressed();if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_X)break;if(p&XINPUT_GAMEPAD_DPAD_UP)selected=(selected+count-1)%count;if(p&XINPUT_GAMEPAD_DPAD_DOWN)selected=(selected+1)%count;
            if(p&XINPUT_GAMEPAD_A){memset(&target,0,sizeof(target));target.sin_family=AF_INET;target.sin_addr.s_addr=inet_addr(rooms[selected].ip);target.sin_port=htons((u_short)rooms[selected].port);if(target.sin_addr.s_addr!=INADDR_NONE){r47_consume_buttons();return true;}}Sleep(16);
        }
    }
}

/* MK64_R48_ACCOUNTS_LOBBY_CHAT
 * Directory/control plane only. Gameplay remains direct UDP 6464.
 * R48 adds server accounts, custom/password rooms and PRE-GAME text chat.
 * In-game keyboard/chat is deliberately not hooked into deterministic gameplay. */
/* MK64_R50_CHAT_ACCOUNT_UX */
static const char *r48_directory_ip="172.233.145.244";
static const unsigned r48_directory_port=6465;
static SOCKET r48_dir_sock=INVALID_SOCKET;
static bool r48_room_registered=false;
static char r48_room_id[20]={0},r48_room_token[40]={0};
static char r48_room_name[32]={0},r48_room_password[20]={0};
static DWORD r48_last_heartbeat=0,r48_last_poll=0;
static unsigned r48_chat_seq=0;
static bool r57_signed_in=false;static DWORD r57_last_presence=0;
static char r57_members[4][40];static unsigned r57_member_count=0;
static char r48_chat1[80]="MARIO KART HUB: Public lobby ready";
static char r48_chat2[80]="X: type lobby chat";
static char r48_chat3[80]={0},r48_chat4[80]={0},r48_chat5[80]={0},r48_chat6[80]={0};
/* R58.1: chat send is queued until the keyboard has fully returned to the lobby frame. */
static char r581_chat_pending[56]={0};static bool r581_chat_pending_ready=false;static DWORD r582_chat_resume=0;

/* MK64_R49_ACCOUNT_PASSWORD_LOGIN */
struct R48Account360{char id[20],secret[40],name[20],password[20];};
static R48Account360 r48_accounts[4];
static int r48_account_count=0,r48_account_index=0;
static bool r48_accounts_loaded=false;

/* MK64_R48_4_VISIBLE_SAVED_PASSWORD
 * User-requested local convenience settings. The lobby password is stored in
 * readable form so it can be displayed and reused exactly. */
static char r484_saved_user[20]={0},r484_saved_password[20]={0},r484_saved_room[32]={0},r484_region[12]="UNSET";
static bool r484_prefs_loaded=false;
static void r484_load_prefs(){
    if(r484_prefs_loaded)return;r484_prefs_loaded=true;
    HANDLE h=CreateFileA("game:\\mk64-public-r48.cfg",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return;
    char b[512];DWORD got=0;if(!ReadFile(h,b,sizeof(b)-1,&got,NULL)){CloseHandle(h);return;}CloseHandle(h);b[got]=0;
    char *line=strtok(b,"\r\n");while(line){if(!strncmp(line,"USERNAME=",9)){strncpy(r484_saved_user,line+9,sizeof(r484_saved_user)-1);r484_saved_user[sizeof(r484_saved_user)-1]=0;}else if(!strncmp(line,"PASSWORD=",9)){strncpy(r484_saved_password,line+9,sizeof(r484_saved_password)-1);r484_saved_password[sizeof(r484_saved_password)-1]=0;}else if(!strncmp(line,"ROOM=",5)){strncpy(r484_saved_room,line+5,sizeof(r484_saved_room)-1);r484_saved_room[sizeof(r484_saved_room)-1]=0;}else if(!strncmp(line,"REGION=",7)){strncpy(r484_region,line+7,sizeof(r484_region)-1);r484_region[sizeof(r484_region)-1]=0;}line=strtok(NULL,"\r\n");}
}
static void r484_save_prefs(){
    char b[512];_snprintf(b,sizeof(b)-1,"USERNAME=%s\r\nPASSWORD=%s\r\nROOM=%s\r\nREGION=%s\r\n",r484_saved_user,r484_saved_password,r484_saved_room,r484_region);b[sizeof(b)-1]=0;
    HANDLE h=CreateFileA("game:\\mk64-public-r48.cfg",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(h==INVALID_HANDLE_VALUE)return;DWORD wrote=0;WriteFile(h,b,(DWORD)strlen(b),&wrote,NULL);CloseHandle(h);
}
static void r484_remember_user(const char *name){r484_load_prefs();if(name){strncpy(r484_saved_user,name,sizeof(r484_saved_user)-1);r484_saved_user[sizeof(r484_saved_user)-1]=0;}r484_save_prefs();}

struct R48Room360{char id[20],name[32],platform[16],owner[20],region[12];unsigned players,max_players,locked,rate60;};

static void r48_set_chat(const char *name,const char *msg){
    strncpy(r48_chat1,r48_chat2,sizeof(r48_chat1)-1);r48_chat1[sizeof(r48_chat1)-1]=0;
    strncpy(r48_chat2,r48_chat3,sizeof(r48_chat2)-1);r48_chat2[sizeof(r48_chat2)-1]=0;
    strncpy(r48_chat3,r48_chat4,sizeof(r48_chat3)-1);r48_chat3[sizeof(r48_chat3)-1]=0;
    strncpy(r48_chat4,r48_chat5,sizeof(r48_chat4)-1);r48_chat4[sizeof(r48_chat4)-1]=0;
    strncpy(r48_chat5,r48_chat6,sizeof(r48_chat5)-1);r48_chat5[sizeof(r48_chat5)-1]=0;
    _snprintf(r48_chat6,sizeof(r48_chat6)-1,"%s: %s",name?name:"HUB",msg?msg:"");r48_chat6[sizeof(r48_chat6)-1]=0;
}
static void r50_chat_reset(const char *room,const char *name){
    r59_world_lobby_view=false;r581_chat_pending[0]=0;r581_chat_pending_ready=false;
    r48_chat1[0]=r48_chat2[0]=r48_chat3[0]=r48_chat4[0]=0;
    _snprintf(r48_chat5,sizeof(r48_chat5)-1,"ROOM: %s",room?room:"");r48_chat5[sizeof(r48_chat5)-1]=0;
    _snprintf(r48_chat6,sizeof(r48_chat6)-1,"%s: waiting for players",name?name:"PLAYER");r48_chat6[sizeof(r48_chat6)-1]=0;
}
static void r50_public_lobby_screen(const char *title,const char *status,bool host){
    IDirect3DDevice9 *dev=x360_d3d_device();if(!dev)return;const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);
    int x=77,right=627,ss=2,body=2;net_text(dev,x,(int)(720*.045f),title?title:"PUBLIC ROOM",4,0xFFFFD050);char inGame[36];_snprintf(inGame,35,"PLAYERS IN GAME: %d",r62_game_total>=0?r62_game_total:0);inGame[35]=0;net_text(dev,(int)(1280*.735f),(int)(720*.103f),inGame,ss,0xFF90D0FF);char online[32];if(r57_online_count>=0)_snprintf(online,31,"PLAYERS ONLINE: %d",r57_online_count);else strcpy(online,"PLAYERS ONLINE: --");net_text(dev,(int)(1280*.735f),(int)(720*.065f),online,ss,0xFF90D0FF);
    net_text(dev,x,(int)(720*.165f),"PLAYERS",body,0xFF90D0FF);for(unsigned i=0;i<r57_member_count&&i<4;++i)net_text(dev,x+15,(int)(720*(.22f+i*.070f)),r57_members[i],body,0xFFFFFFFF);
    const char *panel=r59_world_lobby_view?"WORLD CHAT":"LOBBY CHAT";net_text(dev,right,(int)(720*.165f),panel,body,0xFF90D0FF);const char *chat[6];if(r59_world_lobby_view){for(int i=0;i<6;++i)chat[i]=r59_world_lines[i];}else{chat[0]=r48_chat1;chat[1]=r48_chat2;chat[2]=r48_chat3;chat[3]=r48_chat4;chat[4]=r48_chat5;chat[5]=r48_chat6;}int rightw=1280-right-28;for(int i=0;i<6;++i){const char *line=chat[i]?chat[i]:"";if(!*line)continue;net_text(dev,right,(int)(720*(.22f+i*.052f)),line,r54_360_fit(line,ss,1,rightw),0xFFFFFFFF);}
    net_text(dev,x,(int)(720*.56f),status?status:"",body,0xFFFFD050);char reg[48];_snprintf(reg,47,"YOUR REGION: %s",r484_region);reg[47]=0;net_text(dev,x,(int)(720*.635f),reg,body,0xFFFFFFFF);net_text(dev,x,(int)(720*.74f),host?"A START  RT STATS  Y MAIL  RIGHT PLAYERS  B CANCEL":"RT STATS  Y MAIL  RIGHT PLAYERS  B CANCEL",body,0xFFAAAAAA);net_text(dev,right,(int)(720*.565f),r59_world_lobby_view?"X SEND WORLD CHAT":"X SEND LOBBY CHAT",body,0xFF90D0FF);net_text(dev,right,(int)(720*.625f),"RS SWITCH CHAT VIEW",body,0xFF90D0FF);net_text(dev,x,(int)(720*.79f),"RIGHT PLAYERS   A PROFILE / FRIEND",body,0xFF90D0FF);net_text(dev,x,(int)(720*.90f),host?"AUTO NAT: DIRECT FIRST - HUB RELAY FALLBACK":"LOBBY + WORLD CHAT STAY LIVE UNTIL START",ss,0xFFAAAAAA);dev->Present(0,0,0,0);
}
static bool r48_dir_addr(sockaddr_in &a){memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((u_short)r48_directory_port);a.sin_addr.s_addr=inet_addr(r48_directory_ip);return a.sin_addr.s_addr!=INADDR_NONE;}
/* MK64_R49_2_X360_DIRECTORY_SOCKET_FIX */
static bool r48_dir_open(){
    if(r48_dir_sock!=INVALID_SOCKET)return true;

    /*
     * R49.2: Xbox 360 directory/control socket must use the same proven
     * NetDll socket setup as the gameplay socket. The old R48/R49 helper
     * created an unbound plain UDP socket; on real 360 hardware it never
     * reached the directory even though the OG Xbox implementation did.
     */
    r48_dir_sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(r48_dir_sock==INVALID_SOCKET){
        net_log("MK64DIR: socket failed wsa=%d\n",WSAGetLastError());
        return false;
    }

    BOOL rawSocket=TRUE;
    if(setsockopt(r48_dir_sock,SOL_SOCKET,0x5801,(PCSTR)&rawSocket,sizeof(rawSocket))==SOCKET_ERROR){
        net_log("MK64DIR: 0x5801 failed wsa=%d\n",WSAGetLastError());
        closesocket(r48_dir_sock);r48_dir_sock=INVALID_SOCKET;
        return false;
    }

    int netbuf=64*1024;
    setsockopt(r48_dir_sock,SOL_SOCKET,SO_RCVBUF,(PCSTR)&netbuf,sizeof(netbuf));
    setsockopt(r48_dir_sock,SOL_SOCKET,SO_SNDBUF,(PCSTR)&netbuf,sizeof(netbuf));

    sockaddr_in bindaddr;memset(&bindaddr,0,sizeof(bindaddr));
    bindaddr.sin_family=AF_INET;
    bindaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    bindaddr.sin_port=htons(0); /* dedicated ephemeral control-plane port */
    if(bind(r48_dir_sock,(sockaddr*)&bindaddr,sizeof(bindaddr))==SOCKET_ERROR){
        net_log("MK64DIR: bind ephemeral failed wsa=%d\n",WSAGetLastError());
        closesocket(r48_dir_sock);r48_dir_sock=INVALID_SOCKET;
        return false;
    }

    u_long nb=1;
    if(ioctlsocket(r48_dir_sock,FIONBIO,&nb)==SOCKET_ERROR){
        net_log("MK64DIR: FIONBIO failed wsa=%d\n",WSAGetLastError());
        closesocket(r48_dir_sock);r48_dir_sock=INVALID_SOCKET;
        return false;
    }

    net_log("MK64DIR: dedicated 360 Mario Kart Hub socket ready\n");
    return true;
}
static void r48_dir_close(){if(r48_dir_sock!=INVALID_SOCKET){closesocket(r48_dir_sock);r48_dir_sock=INVALID_SOCKET;}}
static int r48_send(const char *msg){sockaddr_in a;if(r48_dir_sock==INVALID_SOCKET||!msg||!r48_dir_addr(a))return SOCKET_ERROR;return sendto(r48_dir_sock,msg,(int)strlen(msg),0,(const sockaddr*)&a,sizeof(a));}
static void r57_parse_status(const char *b){unsigned n=0,g=0;if(!b)return;int f=sscanf(b,"MKDIR2|STATUS|%u|%u",&n,&g);if(f>=1)r57_online_count=(int)n;if(f>=2)r62_game_total=(int)g;}
static void r57_parse_state(const char *b){unsigned count=0;char list[220]={0};if(!b||sscanf(b,"MKDIR2|STATE|%u|%219[^\r\n]",&count,list)<1)return;r57_member_count=0;char *tok=strtok(list,",");while(tok&&r57_member_count<4){char name[20]={0},region[12]={0};if(sscanf(tok,"%19[^~]~%11s",name,region)==2)_snprintf(r57_members[r57_member_count],sizeof(r57_members[0])-1,"%s [%s]",name,region);else _snprintf(r57_members[r57_member_count],sizeof(r57_members[0])-1,"%s",tok);r57_members[r57_member_count][sizeof(r57_members[0])-1]=0;++r57_member_count;tok=strtok(NULL,",");}}

static void r59_world_reset(){r59_world_seq=0;r59_world_last_poll=0;r59_world_resume=0;r59_world_pending_ready=false;r59_world_pending[0]=0;r59_world_lobby_view=false;for(int i=0;i<6;++i)r59_world_lines[i][0]=0;}
static void r59_world_push(const char *who,const char *region,const char *platform,const char *msg){for(int i=0;i<5;++i){strncpy(r59_world_lines[i],r59_world_lines[i+1],sizeof(r59_world_lines[i])-1);r59_world_lines[i][sizeof(r59_world_lines[i])-1]=0;}_snprintf(r59_world_lines[5],sizeof(r59_world_lines[5])-1,"%s [%s %s]: %s",who?who:"PLAYER",platform?platform:"?",region?region:"UNSET",msg?msg:"");r59_world_lines[5][sizeof(r59_world_lines[5])-1]=0;}
static bool r59_world_packet(const char *b){if(!b)return false;unsigned seq=0;char who[20]={0},region[12]={0},platform[16]={0},msg[80]={0};if(sscanf(b,"MKDIR2|WORLD|%u|%19[^|]|%11[^|]|%15[^|]|%79[^\r\n]",&seq,who,region,platform,msg)==5){if(seq>r59_world_seq){r59_world_seq=seq;r59_world_push(who,region,platform,msg);}return true;}if(!strncmp(b,"MKDIR2|WORLDOK|",15)||!strncmp(b,"MKDIR2|WORLDEND|",16))return true;return false;}
static void r59_world_send_tick(DWORD now){if(!r59_world_ui_active||r48_dir_sock==INVALID_SOCKET)return;R48Account360 *me=(r48_account_index>=0&&r48_account_index<r48_account_count)?&r48_accounts[r48_account_index]:0;if(!me)return;if(r59_world_pending_ready&&r59_world_pending[0]&&(!r59_world_resume||now>=r59_world_resume)){char m[360];_snprintf(m,sizeof(m)-1,"MKDIR2|WORLD_SEND|%s|%s|%s|X360|%s",me->id,me->secret,r484_region,r59_world_pending);m[sizeof(m)-1]=0;if(r48_send(m)!=SOCKET_ERROR){r59_world_pending_ready=false;r59_world_pending[0]=0;r59_world_last_poll=0;r59_world_resume=now+120U;}return;}if(!r59_world_last_poll||now-r59_world_last_poll>=900U){char m[240];_snprintf(m,sizeof(m)-1,"MKDIR2|WORLD_POLL|%s|%s|%u|%s|X360",me->id,me->secret,r59_world_seq,r484_region);m[sizeof(m)-1]=0;r48_send(m);r59_world_last_poll=now;}}
static void r71_refresh_device();
static void r57_presence_tick(bool drain){
    r71_refresh_device();if(!r57_presence_active||r48_dir_sock==INVALID_SOCKET)return;R48Account360 *me=(r48_account_index>=0&&r48_account_index<r48_account_count)?&r48_accounts[r48_account_index]:0;if(!me)return;DWORD now=GetTickCount();if(!r57_last_presence||now-r57_last_presence>=5000U){char m[220];_snprintf(m,sizeof(m)-1,"MKDIR2|PRESENCE|%s|%s|%s|X360",me->id,me->secret,r484_region);m[sizeof(m)-1]=0;r48_send(m);r57_last_presence=now;}r59_world_send_tick(now);if(drain){for(;;){char b[512];sockaddr_in from;int flen=sizeof(from);int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);if(n<=0)break;b[n]=0;if(r59_world_packet(b))continue;if(r61_social_packet(b))continue;r57_parse_status(b);}}}

static void r48_drain(){if(r48_dir_sock==INVALID_SOCKET)return;char b[512];for(;;){sockaddr_in from;int flen=sizeof(from);int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);if(n<=0)break;b[n]=0;if(r59_world_packet(b))continue;if(r61_social_packet(b))continue;if(!strncmp(b,"MKDIR2|STATUS|",14))r57_parse_status(b);}}
static bool r48_wait_prefix(const char *msg,const char *prefix,char *out,int cap,DWORD timeout=2200U){
    r48_drain();DWORD begin=GetTickCount(),last=0;while(GetTickCount()-begin<timeout){DWORD now=GetTickCount();if(!last||now-last>=350U){r48_send(msg);last=now;}
        char b[512];sockaddr_in from;int flen=sizeof(from);int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);if(n>0){b[n]=0;if(r59_world_packet(b))continue;if(r61_social_packet(b))continue;if(!strncmp(b,"MKDIR2|STATUS|",14)){r57_parse_status(b);continue;}if(!strncmp(b,prefix,strlen(prefix))){if(out&&cap>0){strncpy(out,b,cap-1);out[cap-1]=0;}return true;}if(!strncmp(b,"MKDIR2|ERR|",11)||!strncmp(b,"MKDIR2|ACCOUNT_LIMIT|",21)){if(out&&cap>0){strncpy(out,b,cap-1);out[cap-1]=0;}return false;}}
        Sleep(10);}return false;
}

static void r48_load_accounts(){
    if(r48_accounts_loaded)return;r48_accounts_loaded=true;r48_account_count=0;r48_account_index=0;
    HANDLE h=CreateFileA("game:\\mk64-accounts-r2.cfg",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(h==INVALID_HANDLE_VALUE)return;
    char b[1200];DWORD got=0;if(!ReadFile(h,b,sizeof(b)-1,&got,NULL)){CloseHandle(h);return;}CloseHandle(h);b[got]=0;
    char *line=strtok(b,"\r\n");while(line&&r48_account_count<4){R48Account360 a;memset(&a,0,sizeof(a));int n=sscanf(line,"%19[^|]|%39[^|]|%19[^|]|%19[^\r\n]",a.id,a.secret,a.name,a.password);if(n>=3)r48_accounts[r48_account_count++]=a;line=strtok(NULL,"\r\n");}
}
static void r48_save_accounts(){
    char b[1200];int o=0;for(int i=0;i<r48_account_count&&o<(int)sizeof(b)-140;++i)o+=_snprintf(b+o,sizeof(b)-o-1,"%s|%s|%s|%s\r\n",r48_accounts[i].id,r48_accounts[i].secret,r48_accounts[i].name,r48_accounts[i].password);
    HANDLE h=CreateFileA("game:\\mk64-accounts-r2.cfg",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);if(h==INVALID_HANDLE_VALUE)return;DWORD wrote=0;WriteFile(h,b,(DWORD)strlen(b),&wrote,NULL);CloseHandle(h);
}
/* MK64_R48_3_SIMPLE_KEYBOARD
 * Fixed 4x9 A-Z + 0-9 keyboard. No caps/symbol layers. */
static char r48_key(int row,int col){
    static const char *rows[4]={"ABCDEFGHI","JKLMNOPQR","STUVWXYZ0","123456789"};
    if(row<0||row>3||col<0||col>8)return '?';
    return rows[row][col];
}
static void r48_heartbeat();
static void r48_poll_chat();
static void r48_keyboard_draw(const char *prompt,const char *shown,int row,int col,bool lobbyPump){
    IDirect3DDevice9 *dev=x360_d3d_device();if(!dev)return;const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);
    bool live=lobbyPump||r59_world_keyboard;int px=120,py=live?125:70;if(live){net_text(dev,72,28,r59_world_keyboard?"WORLD CHAT":"LOBBY CHAT",2,0xFF90D0FF);const char *c0=r59_world_keyboard?r59_world_lines[3]:r48_chat4;const char *c1=r59_world_keyboard?r59_world_lines[4]:r48_chat5;const char *c2=r59_world_keyboard?r59_world_lines[5]:r48_chat6;net_text(dev,72,50,c0,r54_360_fit(c0,1,1,1080),0xFFFFFFFF);net_text(dev,72,75,c1,r54_360_fit(c1,1,1,1080),0xFFFFFFFF);net_text(dev,72,100,c2,r54_360_fit(c2,1,1,1080),0xFFFFFFFF);}
    net_text(dev,px,py,prompt?prompt:"KEYBOARD",3,0xFFFFD050);char typed[72];_snprintf(typed,sizeof(typed)-1,"TEXT: %s",shown?shown:"");typed[sizeof(typed)-1]=0;net_text(dev,px,py+42,typed,2,0xFFFFFFFF);int gx=135,gy=py+92,dx=50,dy=46;for(int r=0;r<4;++r){for(int c=0;c<9;++c){char k[4];char ch=r48_key(r,c);if(r==row&&c==col){k[0]='[';k[1]=ch;k[2]=']';k[3]=0;}else{k[0]=ch;k[1]=0;}net_text(dev,gx+c*dx+(r==row&&c==col?-7:0),gy+r*dy,k,2,(r==row&&c==col)?0xFFFFD050:0xFFFFFFFF);}}char selected[48];_snprintf(selected,sizeof(selected)-1,"SELECTED: %c",r48_key(row,col));selected[sizeof(selected)-1]=0;net_text(dev,px,gy+200,selected,2,0xFFFFD050);net_text(dev,px,gy+238,"A TYPE  X DEL  Y SPACE  START DONE  B CANCEL",2,0xFFAAAAAA);dev->Present(0,0,0,0);
}
static bool r48_keyboard(const char *prompt,char *out,int cap,bool secret,bool lobby_pump){
    (void)secret;int row=0,col=0,len=(int)strlen(out);r47_consume_buttons();
    for(;;){
        if(lobby_pump){pump();if(active){r47_consume_buttons();return false;}if(r47_public_mode&&hosting)r48_heartbeat();r57_presence_tick(false);r48_poll_chat();}else if(r59_world_keyboard){r57_presence_tick(true);}
        char shown[56];strncpy(shown,out,sizeof(shown)-1);shown[sizeof(shown)-1]=0;r48_keyboard_draw(prompt,shown,row,col,lobby_pump);
        DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_B){r47_consume_buttons();return false;}
        if(p&XINPUT_GAMEPAD_DPAD_LEFT)col=(col+8)%9;if(p&XINPUT_GAMEPAD_DPAD_RIGHT)col=(col+1)%9;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+3)%4;if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%4;
        if((p&XINPUT_GAMEPAD_X)&&len>0)out[--len]=0;if((p&XINPUT_GAMEPAD_Y)&&len<cap-1){out[len++]=' ';out[len]=0;}if((p&XINPUT_GAMEPAD_A)&&len<cap-1){out[len++]=r48_key(row,col);out[len]=0;}
        if(p&XINPUT_GAMEPAD_START){while(len>0&&out[len-1]==' ')out[--len]=0;if(len>0){r47_consume_buttons();return true;}}
        Sleep(16);
    }
}
/* MK64_R48_5_OG_STYLE_ACCOUNT_CHAT
 * Account creation uses a stable request nonce so UDP retransmits are idempotent
 * with directory R2.2. Keep the nonce if the user retries the same name. */
static unsigned r485_create_counter=0;
static char r485_last_create_name[20]={0},r485_last_create_nonce[24]={0};
static const char *r485_create_nonce_for(const char *name){
    if(!name)name="";
    if(strcmp(r485_last_create_name,name)||!r485_last_create_nonce[0]){
        strncpy(r485_last_create_name,name,sizeof(r485_last_create_name)-1);r485_last_create_name[sizeof(r485_last_create_name)-1]=0;
        ++r485_create_counter;_snprintf(r485_last_create_nonce,sizeof(r485_last_create_nonce)-1,"X%08lX%04X",(unsigned long)GetTickCount(),r485_create_counter&0xFFFFU);r485_last_create_nonce[sizeof(r485_last_create_nonce)-1]=0;
    }
    return r485_last_create_nonce;
}
static void r485_create_nonce_done(){r485_last_create_name[0]=0;r485_last_create_nonce[0]=0;}

/* MK64_R49_ACCOUNT_PASSWORD_LOGIN
 * Account password is visible by user request and saved locally. Directory R2.3
 * hashes it server-side. Room passwords remain optional and separate. */
static unsigned r49_create_counter=0;static char r49_create_key[48]={0},r49_create_nonce[24]={0};
static const char *r49_nonce_for(const char *name,const char *password){char key[48];_snprintf(key,sizeof(key)-1,"%s:%s",name?name:"",password?password:"");key[sizeof(key)-1]=0;if(strcmp(key,r49_create_key)||!r49_create_nonce[0]){strncpy(r49_create_key,key,sizeof(r49_create_key)-1);r49_create_key[sizeof(r49_create_key)-1]=0;++r49_create_counter;_snprintf(r49_create_nonce,sizeof(r49_create_nonce)-1,"P%08lX%04X",(unsigned long)GetTickCount(),r49_create_counter&0xFFFFU);r49_create_nonce[sizeof(r49_create_nonce)-1]=0;}return r49_create_nonce;}
static void r49_nonce_done(){r49_create_key[0]=0;r49_create_nonce[0]=0;}
static int r49_find_local(const char *name){for(int i=0;i<r48_account_count;++i)if(!strcmp(r48_accounts[i].name,name))return i;return -1;}
static bool r49_store_ok(const char *resp,const char *password){R48Account360 a;memset(&a,0,sizeof(a));if(sscanf(resp,"MKDIR2|ACCOUNT_OK|%19[^|]|%39[^|]|%19[^\r\n]",a.id,a.secret,a.name)!=3)return false;strncpy(a.password,password?password:"",sizeof(a.password)-1);a.password[sizeof(a.password)-1]=0;int i=r49_find_local(a.name);if(i<0){if(r48_account_count>=4)return false;i=r48_account_count++;}r48_accounts[i]=a;r48_account_index=i;r48_save_accounts();r484_remember_user(a.name);return true;}
static void r49_wait_a_or_b(){while(!(pressed(false)&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B)))Sleep(16);r47_consume_buttons();}
/* MK64_R51_SECURE_ACCOUNT_CHALLENGE */
/* MK64_R51_SECURE_ACCOUNT_AUTH - self-contained SHA256/HMAC/PBKDF2 */
struct R51ShaCtx{unsigned int h[8];unsigned long long bits;unsigned char b[64];unsigned int used;};
static unsigned int r51_rr(unsigned int x,unsigned n){return (x>>n)|(x<<(32-n));}
static void r51_sha_init(R51ShaCtx *c){static const unsigned int iv[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};memcpy(c->h,iv,32);c->bits=0;c->used=0;}
static void r51_sha_block(R51ShaCtx *c,const unsigned char *p){static const unsigned int k[64]={0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};unsigned int w[64];for(int i=0;i<16;++i)w[i]=((unsigned)p[i*4]<<24)|((unsigned)p[i*4+1]<<16)|((unsigned)p[i*4+2]<<8)|p[i*4+3];for(int i=16;i<64;++i){unsigned int s0=r51_rr(w[i-15],7)^r51_rr(w[i-15],18)^(w[i-15]>>3),s1=r51_rr(w[i-2],17)^r51_rr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}unsigned int a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];for(int i=0;i<64;++i){unsigned int S1=r51_rr(e,6)^r51_rr(e,11)^r51_rr(e,25),ch=(e&f)^((~e)&g),t1=h+S1+ch+k[i]+w[i],S0=r51_rr(a,2)^r51_rr(a,13)^r51_rr(a,22),maj=(a&b)^(a&cc)^(b&cc),t2=S0+maj;h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;}c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;}
static void r51_sha_update(R51ShaCtx *c,const unsigned char *p,unsigned int n){c->bits+=(unsigned long long)n*8ULL;while(n){unsigned int take=64-c->used;if(take>n)take=n;memcpy(c->b+c->used,p,take);c->used+=take;p+=take;n-=take;if(c->used==64){r51_sha_block(c,c->b);c->used=0;}}}
static void r51_sha_final(R51ShaCtx *c,unsigned char out[32]){c->b[c->used++]=0x80;if(c->used>56){while(c->used<64)c->b[c->used++]=0;r51_sha_block(c,c->b);c->used=0;}while(c->used<56)c->b[c->used++]=0;for(int i=7;i>=0;--i)c->b[c->used++]=(unsigned char)(c->bits>>(i*8));r51_sha_block(c,c->b);for(int i=0;i<8;++i){out[i*4]=(unsigned char)(c->h[i]>>24);out[i*4+1]=(unsigned char)(c->h[i]>>16);out[i*4+2]=(unsigned char)(c->h[i]>>8);out[i*4+3]=(unsigned char)c->h[i];}}
static void r51_hmac(const unsigned char *key,unsigned int kn,const unsigned char *msg,unsigned int mn,unsigned char out[32]){unsigned char k0[64],ip[64],op[64],inner[32];memset(k0,0,64);if(kn>64){R51ShaCtx c;r51_sha_init(&c);r51_sha_update(&c,key,kn);r51_sha_final(&c,k0);}else memcpy(k0,key,kn);for(int i=0;i<64;++i){ip[i]=(unsigned char)(k0[i]^0x36);op[i]=(unsigned char)(k0[i]^0x5c);}R51ShaCtx c;r51_sha_init(&c);r51_sha_update(&c,ip,64);r51_sha_update(&c,msg,mn);r51_sha_final(&c,inner);r51_sha_init(&c);r51_sha_update(&c,op,64);r51_sha_update(&c,inner,32);r51_sha_final(&c,out);}
static int r51_hex_nib(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static bool r51_hex32(const char *s,unsigned char out[16]){if(!s||strlen(s)!=32)return false;for(int i=0;i<16;++i){int a=r51_hex_nib(s[i*2]),b=r51_hex_nib(s[i*2+1]);if(a<0||b<0)return false;out[i]=(unsigned char)((a<<4)|b);}return true;}
static void r51_tohex(const unsigned char *p,unsigned int n,char *out){static const char h[]="0123456789abcdef";for(unsigned int i=0;i<n;++i){out[i*2]=h[p[i]>>4];out[i*2+1]=h[p[i]&15];}out[n*2]=0;}
static bool r51_pbkdf2(const char *password,const char *salt_hex,unsigned int rounds,unsigned char out[32]){if(!password||!password[0]||rounds<1||rounds>250000U)return false;unsigned char salt[16],m[20],u[32],t[32];if(!r51_hex32(salt_hex,salt))return false;memcpy(m,salt,16);m[16]=0;m[17]=0;m[18]=0;m[19]=1;r51_hmac((const unsigned char*)password,(unsigned int)strlen(password),m,20,u);memcpy(t,u,32);for(unsigned int i=1;i<rounds;++i){r51_hmac((const unsigned char*)password,(unsigned int)strlen(password),u,32,u);for(int j=0;j<32;++j)t[j]^=u[j];}memcpy(out,t,32);return true;}
static bool r51_verifier_hex(const char *password,const char *salt_hex,unsigned int rounds,char out[65]){unsigned char v[32];if(!r51_pbkdf2(password,salt_hex,rounds,v))return false;r51_tohex(v,32,out);return true;}
static bool r51_proof_hex(const char *password,const char *salt_hex,unsigned int rounds,const char *purpose,const char *pid,const char *nonce,char out[65]){unsigned char v[32],p[32];char msg[128];if(!r51_pbkdf2(password,salt_hex,rounds,v))return false;_snprintf(msg,sizeof(msg)-1,"MK64|%s|%s|%s",purpose,pid,nonce);msg[sizeof(msg)-1]=0;r51_hmac(v,32,(const unsigned char*)msg,(unsigned int)strlen(msg),p);r51_tohex(p,32,out);return true;}

/* R71 console pseudonym. The raw MAC is never transmitted or written to the Hub.
 * If firmware does not supply a usable MAC (e.g. some emulators), use a
 * cryptographically random persistent per-install ID. Neither is attested. */
static char r71_console_key[65]={0};
static bool r71_local_fallback(unsigned char material[16]){
    HANDLE h=CreateFileA("game:\\mk64-device-v1.bin",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h!=INVALID_HANDLE_VALUE){DWORD got=0;BOOL ok=ReadFile(h,material,16,&got,NULL);CloseHandle(h);if(ok&&got==16)return true;}
    if(XNetRandom(material,16)!=0)return false;
    h=CreateFileA("game:\\mk64-device-v1.bin",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(h==INVALID_HANDLE_VALUE)return false;
    DWORD wrote=0;BOOL ok=WriteFile(h,material,16,&wrote,NULL);CloseHandle(h);return ok&&wrote==16;
}
static bool r71_console_id(char out[65]){
    if(r71_console_key[0]){memcpy(out,r71_console_key,65);return true;}
    XNADDR a;memset(&a,0,sizeof(a));XNetGetTitleXnAddr(&a);
    unsigned char material[16]={0};unsigned int len=6;bool found=false, allff=true;
    for(unsigned i=0;i<6;++i){material[i]=a.abEnet[i];if(material[i])found=true;if(material[i]!=255)allff=false;}
    bool hw=found&&!allff;
    if(!hw){len=16;if(!r71_local_fallback(material))return false;}
    static const char domain[]="MK64-HUB-R71-CONSOLE-V1";
    static const char hardware[]="HARDWARE";static const char local[]="INSTALL";
    R51ShaCtx ctx;unsigned char digest[32];r51_sha_init(&ctx);
    r51_sha_update(&ctx,(const unsigned char*)domain,(unsigned int)(sizeof(domain)-1));
    const char *kind=hw?hardware:local;
    r51_sha_update(&ctx,(const unsigned char*)kind,(unsigned int)strlen(kind));
    r51_sha_update(&ctx,material,len);r51_sha_final(&ctx,digest);
    r51_tohex(digest,32,r71_console_key);memcpy(out,r71_console_key,65);return true;
}
static void r71_refresh_device(){
    static DWORD last=0;DWORD now=GetTickCount();if(last&&now-last<30000U)return;
    char id[65],pkt[110];if(!r71_console_id(id))return;
    _snprintf(pkt,sizeof(pkt)-1,"MKDIR2|DEVICE_HELLO|%s",id);pkt[sizeof(pkt)-1]=0;
    if(r48_send(pkt)!=SOCKET_ERROR)last=now;
}
static bool r71_register_console(){
    char id[65],req[110],resp[120]={0};
    if(!r71_console_id(id)){screen("CONSOLE ID ERROR","COULD NOT SAVE CONSOLE ID","CHECK GAME STORAGE / NETWORK","","","A/B BACK");r49_wait_a_or_b();return false;}
    _snprintf(req,sizeof(req)-1,"MKDIR2|DEVICE_HELLO|%s",id);req[sizeof(req)-1]=0;
    if(!r48_wait_prefix(req,"MKDIR2|DEVICE_ACK|",resp,sizeof(resp),4000U) ||
       strcmp(resp,"MKDIR2|DEVICE_ACK|OK")){
        if(strstr(resp,"BANNED"))screen("CONSOLE BANNED","ACCESS DENIED","CONTACT SERVER ADMIN","","","A/B BACK");
        else screen("HUB CONNECTION ERROR","CONSOLE ID NOT ACCEPTED","CHECK SERVER R2.17 / NETWORK","","","A/B BACK");
        r49_wait_a_or_b();return false;
    }
    return true;
}

static bool r49_login_name_password(const char *name,const char *password){
    char begin[128],challenge[320]={0};
    if(!r71_register_console())return false;
    _snprintf(begin,sizeof(begin)-1,"MKDIR2|AUTH_BEGIN|LOGIN|%s",name);begin[sizeof(begin)-1]=0;
    screen("SECURE LOGIN",name,"REQUESTING ONE-TIME CHALLENGE","PASSWORD WILL NOT BE SENT","","PLEASE WAIT");
    if(!r48_wait_prefix(begin,"MKDIR2|AUTH_CHALLENGE|",challenge,sizeof(challenge),5000U)){
        if(strstr(challenge,"ACCOUNT_NOT_FOUND"))screen("LOGIN FAILED","ACCOUNT NOT FOUND",name,"","","A/B BACK");
        else if(strstr(challenge,"LEGACY_ACCOUNT_ADMIN"))screen("LOGIN FAILED","LEGACY ACCOUNT NEEDS MIGRATION",name,"","","A/B BACK");
        else screen("LOGIN FAILED","MARIO KART HUB DID NOT START LOGIN","CHECK MARIO KART HUB / NETWORK","","","A/B BACK");
        r49_wait_a_or_b();return false;
    }
    char purpose[12]={0},pid[20]={0},server_name[20]={0},salt[40]={0},nonce[40]={0};unsigned rounds=0;
    if(sscanf(challenge,"MKDIR2|AUTH_CHALLENGE|%11[^|]|%19[^|]|%19[^|]|%39[^|]|%u|%39s",purpose,pid,server_name,salt,&rounds,nonce)!=6||strcmp(purpose,"LOGIN")){screen("LOGIN ERROR","BAD CHALLENGE RESPONSE","","","","A/B BACK");;r49_wait_a_or_b();return false;}
    screen("SECURE LOGIN",name,"DERIVING PASSWORD PROOF","PASSWORD STAYS ON THIS CONSOLE","","PLEASE WAIT");
    char proof[65];if(!r51_proof_hex(password,salt,rounds,"LOGIN",pid,nonce,proof)){screen("SECURE AUTH ERROR","COULD NOT DERIVE PASSWORD PROOF","","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char finish[256],resp[256]={0};_snprintf(finish,sizeof(finish)-1,"MKDIR2|AUTH_FINISH|LOGIN|%s|%s|%s",pid,nonce,proof);finish[sizeof(finish)-1]=0;
    if(!r48_wait_prefix(finish,"MKDIR2|ACCOUNT_OK|",resp,sizeof(resp),5000U)){if(strstr(resp,"BAD_ACCOUNT_PASSWORD"))screen("LOGIN FAILED","WRONG PASSWORD",name,"","","A/B BACK"); else screen("LOGIN FAILED","MARIO KART HUB DID NOT ACCEPT LOGIN","CHECK MARIO KART HUB / NETWORK","","","A/B BACK"); r49_wait_a_or_b();return false;}
    if(!r49_store_ok(resp,password)){screen("LOGIN ERROR","HUB LOGIN SUCCEEDED","BUT CREDENTIALS COULD NOT BE SAVED","","","A/B BACK");;r49_wait_a_or_b();return false;}
    r47_consume_buttons();return true;
}
static bool r49_login_other(){char name[20]="",pass[20]="";if(!r48_keyboard("LOGIN USERNAME",name,18,false,false))return false;if(!r48_keyboard("LOGIN PASSWORD",pass,19,false,false))return false;return r49_login_name_password(name,pass);}

static bool r48_create_account(){
    if(r48_account_count>=4){screen("ACCOUNTS","THIS CONSOLE ALREADY SAVED 4 ACCOUNTS","HUB LIMITS 4 ACCOUNTS PER CONSOLE","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char name[20]="",pass[20]="",confirm[20]="";if(!r48_keyboard("CREATE USERNAME",name,18,false,false))return false;
    for(;;){pass[0]=confirm[0]=0;if(!r48_keyboard("CREATE ACCOUNT PASSWORD",pass,19,false,false))return false;if(!r48_keyboard("CONFIRM ACCOUNT PASSWORD",confirm,19,false,false))return false;if(!strcmp(pass,confirm))break;screen("PASSWORDS DO NOT MATCH","RE-ENTER THE ACCOUNT PASSWORD","","","","A/B RETRY");;r49_wait_a_or_b();}
    if(!r71_register_console())return false;
    const char *request_nonce=r49_nonce_for(name,pass);char begin[160],challenge[320]={0};_snprintf(begin,sizeof(begin)-1,"MKDIR2|ACCOUNT_CREATE_BEGIN|%s|%s",name,request_nonce);begin[sizeof(begin)-1]=0;screen("CREATING ACCOUNT",name,"REQUESTING SECURE ACCOUNT SALT","PASSWORD WILL NOT BE SENT","","PLEASE WAIT");
    if(!r48_wait_prefix(begin,"MKDIR2|ACCOUNT_CREATE_CHALLENGE|",challenge,sizeof(challenge),5000U)){if(!strncmp(challenge,"MKDIR2|ACCOUNT_LIMIT|",21))screen("ACCOUNT LIMIT","You reached the account limit for your console","ASK HUB ADMIN FOR HELP","","","A/B BACK"); else if(strstr(challenge,"NAME_TAKEN")){screen("USERNAME ALREADY EXISTS",name,"USE LOGIN OR CHOOSE ANOTHER NAME","","","A/B BACK");;r49_nonce_done();}else screen("ACCOUNT CREATE FAILED","HUB DID NOT CONFIRM SECURE CREATE","CHECK MARIO KART HUB / NETWORK","","","A/B BACK"); r49_wait_a_or_b();return false;}
    char server_name[20]={0},salt[40]={0},nonce[40]={0},echo[32]={0};unsigned rounds=0;
    if(sscanf(challenge,"MKDIR2|ACCOUNT_CREATE_CHALLENGE|%19[^|]|%39[^|]|%u|%39[^|]|%31s",server_name,salt,&rounds,nonce,echo)!=5){screen("ACCOUNT CREATE ERROR","BAD HUB CHALLENGE","","","","A/B BACK");;r49_wait_a_or_b();return false;}
    screen("CREATING ACCOUNT",name,"DERIVING SALTED PASSWORD VERIFIER","PASSWORD STAYS ON THIS CONSOLE","","PLEASE WAIT");
    char verifier[65];if(!r51_verifier_hex(pass,salt,rounds,verifier)){screen("SECURE AUTH ERROR","COULD NOT DERIVE PASSWORD PROOF","","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char finish[360],resp[256]={0};_snprintf(finish,sizeof(finish)-1,"MKDIR2|ACCOUNT_CREATE_FINISH|%s|%s|%u|%s|%s|%s",server_name,salt,rounds,nonce,verifier,request_nonce);finish[sizeof(finish)-1]=0;
    if(!r48_wait_prefix(finish,"MKDIR2|ACCOUNT_OK|",resp,sizeof(resp),5000U)){if(!strncmp(resp,"MKDIR2|ACCOUNT_LIMIT|",21))screen("ACCOUNT LIMIT","You reached the account limit for your console","ASK HUB ADMIN FOR HELP","","","A/B BACK"); else if(strstr(resp,"NAME_TAKEN"))screen("USERNAME ALREADY EXISTS",name,"USE LOGIN OR CHOOSE ANOTHER NAME","","","A/B BACK"); else screen("ACCOUNT CREATE FAILED","HUB DID NOT CONFIRM SECURE CREATE","CHECK MARIO KART HUB / NETWORK","","","A/B BACK"); r49_wait_a_or_b();return false;}
    if(!r49_store_ok(resp,pass)){screen("ACCOUNT RESPONSE ERROR","ACCOUNT WAS CREATED BUT COULD NOT BE SAVED","ASK HUB ADMIN BEFORE RETRYING","","","A/B BACK");;r49_wait_a_or_b();return false;}r49_nonce_done();screen("ACCOUNT CREATED",name,"SAVED TO THIS CONSOLE","PASSWORD ITSELF WAS NOT SENT","","A CONTINUE");;r49_wait_a_or_b();return true;
}
static void r50_remove_local_account(int idx){
    if(idx<0||idx>=r48_account_count)return;char deleted[20];strncpy(deleted,r48_accounts[idx].name,sizeof(deleted)-1);deleted[sizeof(deleted)-1]=0;
    for(int i=idx;i+1<r48_account_count;++i)r48_accounts[i]=r48_accounts[i+1];if(r48_account_count>0)--r48_account_count;if(r48_account_count<0)r48_account_count=0;
    if(r48_account_index>=r48_account_count)r48_account_index=r48_account_count?0:0;r48_save_accounts();r484_load_prefs();if(!strcmp(r484_saved_user,deleted)){r484_saved_user[0]=0;r484_save_prefs();}
}
static bool r50_delete_current_account(){
    if(r48_account_index<0||r48_account_index>=r48_account_count)return false;R48Account360 a=r48_accounts[r48_account_index];
    for(;;){screen("DELETE ACCOUNT?",a.name,"THIS DELETES HUB + LOCAL ACCOUNT","A CONTINUE  B CANCEL","","PASSWORD REQUIRED TO CONFIRM"); DWORD q=pressed(false); if(q&XINPUT_GAMEPAD_B){r47_consume_buttons(); return false;}if(q&XINPUT_GAMEPAD_A)break;Sleep(16);}r47_consume_buttons();
    char pass[20]="";if(!r48_keyboard("ENTER ACCOUNT PASSWORD TO DELETE",pass,sizeof(pass),false,false))return false;
    char begin[180],challenge[320]={0};_snprintf(begin,sizeof(begin)-1,"MKDIR2|AUTH_BEGIN|DELETE|%s|%s",a.id,a.secret);begin[sizeof(begin)-1]=0;screen("DELETING ACCOUNT",a.name,"REQUESTING ONE-TIME CHALLENGE","PASSWORD WILL NOT BE SENT","","PLEASE WAIT");
    if(!r48_wait_prefix(begin,"MKDIR2|AUTH_CHALLENGE|",challenge,sizeof(challenge),5000U)){screen("DELETE FAILED","HUB DID NOT CONFIRM SECURE DELETE","LOCAL ACCOUNT WAS KEPT","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char purpose[12]={0},pid[20]={0},server_name[20]={0},salt[40]={0},nonce[40]={0};unsigned rounds=0;
    if(sscanf(challenge,"MKDIR2|AUTH_CHALLENGE|%11[^|]|%19[^|]|%19[^|]|%39[^|]|%u|%39s",purpose,pid,server_name,salt,&rounds,nonce)!=6||strcmp(purpose,"DELETE")){screen("DELETE FAILED","HUB DID NOT CONFIRM SECURE DELETE","LOCAL ACCOUNT WAS KEPT","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char proof[65];if(!r51_proof_hex(pass,salt,rounds,"DELETE",pid,nonce,proof)){screen("SECURE AUTH ERROR","COULD NOT DERIVE PASSWORD PROOF","","","","A/B BACK");;r49_wait_a_or_b();return false;}
    char finish[256],resp[256]={0};_snprintf(finish,sizeof(finish)-1,"MKDIR2|AUTH_FINISH|DELETE|%s|%s|%s",pid,nonce,proof);finish[sizeof(finish)-1]=0;
    if(!r48_wait_prefix(finish,"MKDIR2|ACCOUNT_DELETED|",resp,sizeof(resp),5000U)){if(strstr(resp,"BAD_ACCOUNT_PASSWORD"))screen("DELETE FAILED","WRONG ACCOUNT PASSWORD",a.name,"","","A/B BACK"); else screen("DELETE FAILED","HUB DID NOT CONFIRM SECURE DELETE","LOCAL ACCOUNT WAS KEPT","","","A/B BACK"); r49_wait_a_or_b();return false;}
    int idx=r48_account_index;r50_remove_local_account(idx);screen("ACCOUNT DELETED",a.name,"REMOVED FROM MARIO KART HUB","REMOVED FROM THIS CONSOLE","","A CONTINUE");;r49_wait_a_or_b();return true;
}
static bool r50_account_actions(){
    R48Account360 *me=(r48_account_index>=0&&r48_account_index<r48_account_count)?&r48_accounts[r48_account_index]:0;if(!me)return false;int row=0;r47_consume_buttons();for(;;){char a[88],b[88],c[88],d[88];_snprintf(a,87,"SIGNED IN: %s",me->name);_snprintf(b,87,"%c CONTINUE TO PUBLIC MATCH",row==0?'>':' ');_snprintf(c,87,"%c BACK TO ACCOUNT LIST",row==1?'>':' ');d[0]=0;screen("ACCOUNT",a,b,c,d,"A SELECT   UP/DOWN   B BACK","DELETE: HOLD X+Y 3 SEC ON ACCOUNT LIST");DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%2;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+1)%2;if(p&XINPUT_GAMEPAD_A){if(row==0){r47_consume_buttons();return true;}return false;}Sleep(16);}
}
static bool r48_account_login(int idx){
    if(idx<0||idx>=r48_account_count)return false;r48_account_index=idx;if(!r48_accounts[idx].password[0]){screen("SAVED PASSWORD MISSING",r48_accounts[idx].name,"USE LOGIN TO ANOTHER ACCOUNT","TO SAVE ITS PASSWORD AGAIN","","A/B BACK");r49_wait_a_or_b();return false;}
    /* R50 privacy: use saved password internally; never render it on screen. */
    if(!r49_login_name_password(r48_accounts[idx].name,r48_accounts[idx].password))return false;r47_consume_buttons();return true;
}
static bool r48_choose_account(){
    r48_load_accounts();r484_load_prefs();int row=0;for(int i=0;i<r48_account_count;++i)if(r484_saved_user[0]&&!strcmp(r48_accounts[i].name,r484_saved_user)){row=i;break;}r47_consume_buttons();
    for(;;){
        if(row<r48_account_count && r54_360_delete_hold()){r48_account_index=row;r47_consume_buttons();r50_delete_current_account();r47_consume_buttons();continue;}int items=r48_account_count+3;if(row>=items)row=0;char l[4][88];for(int i=0;i<4;++i)l[i][0]=0;int first=row-1;if(first<0)first=0;if(first>items-4)first=items-4;if(first<0)first=0;
        for(int j=0;j<4;++j){int i=first+j;if(i>=items)continue;if(i<r48_account_count)_snprintf(l[j],87,"%c %s",i==row?'>':' ',r48_accounts[i].name);else if(i==r48_account_count)_snprintf(l[j],87,"%c LOGIN TO ANOTHER ACCOUNT",i==row?'>':' ');else if(i==r48_account_count+1)_snprintf(l[j],87,"%c + %s",i==row?'>':' ',r48_account_count?"CREATE ANOTHER ACCOUNT":"CREATE ACCOUNT");else _snprintf(l[j],87,"%c BACK",i==row?'>':' ');l[j][87]=0;}
        screen("PUBLIC MATCH ACCOUNT",l[0],l[1],l[2],l[3],"A SELECT   UP/DOWN   B BACK","HOLD X+Y 3 SEC ON SAVED ACCOUNT TO DELETE");DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%items;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+items-1)%items;
        if(p&XINPUT_GAMEPAD_A){if(row<r48_account_count){if(r48_account_login(row)){r47_consume_buttons();return true;}r47_consume_buttons();}else if(row==r48_account_count){if(r49_login_other()){r47_consume_buttons();return true;}r47_consume_buttons();}else if(row==r48_account_count+1){int before=r48_account_count;if(r48_create_account()){row=(r48_account_count>before)?r48_account_count-1:r48_account_count;continue;}r47_consume_buttons();}else return false;}Sleep(16);
    }
}
static R48Account360 *r48_me(){return (r48_account_index>=0&&r48_account_index<r48_account_count)?&r48_accounts[r48_account_index]:0;}
#define R61_WORLD_SUPPRESS r59_world_overlay_suppress
static bool r70_social_packet(const char *b);
#include "r61_social_shared.inl"
static void r62_lobby_service();
static void r61_modal_service(){if(r62_modal_lobby)r62_lobby_service();}
#include "r62_game_stats.inl"
#define R70_ROOM_ID r48_room_id
#define R70_LOBBY_RUNNING (!active && !failed)
#include "r70_room_social.inl"
#include "r73_solo_online.inl"
#undef R70_ROOM_ID
#undef R70_LOBBY_RUNNING
/* R61 native 1280x720 virtual-space social panel; net_text scales to output. */
static void r61_rect(IDirect3DDevice9 *dev,DWORD sw,DWORD sh,int x0,int y0,int x1,int y1,DWORD color){
    const bool wide=x360_video_widescreen()!=0;const int vy=wide?0:90,vh=wide?720:540;
    D3DRECT r={(LONG)(x0*sw/1280),(LONG)((vy+y0*vh/720)*sh/720),
               (LONG)(x1*sw/1280),(LONG)((vy+y1*vh/720)*sh/720)};
    if(r.x2>r.x1&&r.y2>r.y1)dev->Clear(1,&r,D3DCLEAR_TARGET,color,1,0);
}
static void r61_menu_draw(int left_row,bool focus_right,int right_row){
    IDirect3DDevice9 *dev=x360_d3d_device();if(!dev)return;
    const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();
    D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);
    dev->Clear(0,0,D3DCLEAR_TARGET,0xFF101C2A,1,0);
    net_text(dev,90,42,"MARIO KART HUB / ONLINE",4,0xFFFFD050);
    
    net_text(dev,91,143,"(C) 2026 SIRDANKZ / MPL-2.0 FOR ORIGINAL CODE",2,0xFF90D0FF);
    net_text(dev,91,169,"NINTENDO / TEAM RESURGENT / UPSTREAM RIGHTS UNCHANGED",2,0xFF8AA7C0);
    char online[32];R61_SNPRINTF(online,sizeof(online)-1,"PLAYERS ONLINE: %d",r57_online_count>=0?r57_online_count:0);online[sizeof(online)-1]=0;
    net_text(dev,894,116,online,2,0xFF90D0FF);
    char gcount[36];R61_SNPRINTF(gcount,sizeof(gcount)-1,"PLAYERS IN GAME: %d",r62_game_total>=0?r62_game_total:0);gcount[sizeof(gcount)-1]=0;net_text(dev,894,150,gcount,2,0xFF90D0FF);
    r61_rect(dev,sw,sh,79,205,716,623,0xFF182B3F);r61_rect(dev,sw,sh,732,205,1202,623,0xFF182B3F);
    r61_rect(dev,sw,sh,708,215,711,612,0xFF34506B);
    net_text(dev,104,225,"PLAY",3,0xFFFFD050);net_text(dev,758,225,"ONLINE PLAYERS",3,0xFFFFD050);
    const char *items[6]={"CREATE PUBLIC ROOM","BROWSE PUBLIC ROOMS","SOLO ONLINE","REGION","LEADERBOARDS","SIGN OUT / BACK"};
    for(int i=0;i<6;++i){
        char line[90];if(i==3)R61_SNPRINTF(line,sizeof(line)-1,"REGION: %s",r484_region);else R61_SNPRINTF(line,sizeof(line)-1,"%s",items[i]);line[sizeof(line)-1]=0;
        if(i==left_row&&!focus_right)r61_rect(dev,sw,sh,96,266+i*55,694,316+i*55,0xFF334B64);
        char label[100];R61_SNPRINTF(label,sizeof(label)-1,"%c %s",i==left_row&&!focus_right?'>':' ',line);label[sizeof(label)-1]=0;
        net_text(dev,112,279+i*55,label,3,i==left_row&&!focus_right?0xFFFFD050:0xFFE4F1FA);
    }
    if(r61_total<=0)net_text(dev,762,306,"NO OTHER PLAYERS ONLINE",2,0xFFFFFFFF);
    else for(int i=0;i<5;++i){if(!r61_people[i].found)continue;
        const R61Person &u=r61_people[i];int y=281+i*61;
        if(focus_right&&right_row==i)r61_rect(dev,sw,sh,746,y,1188,y+55,0xFF334B64);
        char line[64];R61_SNPRINTF(line,sizeof(line)-1,"%c %.16s",focus_right&&i==right_row?'>':' ',u.name);line[sizeof(line)-1]=0;
        net_text(dev,760,y+5,line,3,focus_right&&i==right_row?0xFFFFD050:0xFFFFFFFF);
        char detail[48];R61_SNPRINTF(detail,sizeof(detail)-1,"%s / %s%s",u.platform,u.region,u.playing?" IN GAME":"");detail[sizeof(detail)-1]=0;detail[31]=0;
        net_text(dev,776,y+34,detail,2,0xFF90D0FF);
    }
    char page[56];R61_SNPRINTF(page,sizeof(page)-1,"%d-%d OF %d",r61_total?r61_offset+1:0,r61_offset+r61_count_people(),r61_total);page[sizeof(page)-1]=0;net_text(dev,758,590,page,2,0xFF9EB4C9);
    net_text(dev,90,630,focus_right?"A PROFILE  LEFT MENU":"A SELECT  SOLO TT BESTS / GP STATS",3,0xFFFFD050);
    net_text(dev,90,664,"RT STATS  Y PROFILE  X MAILBOX  RS CHAT  B BACK",2,0xFFB8CAD9);
    net_text(dev,820,630,"HOST: PORT FORWARD UDP 6464",2,0xFFFF6868);
    net_text(dev,820,664,"FOR BEST CONNECTION",2,0xFFFF6868);
    dev->Present(0,0,0,0);
}
static void r57_region_menu(){static const char *regions[]={"UNSET","US-W","US-C","US-E","CANADA","LATAM","EUROPE","ASIA","OCEANIA","OTHER"};int row=0;for(int i=0;i<10;++i)if(!strcmp(r484_region,regions[i]))row=i;r47_consume_buttons();for(;;){r57_presence_tick(true);char a[64],b[64],c[64],d[64];int first=(row/4)*4;const char *l[4]={"","","",""};for(int j=0;j<4;++j)if(first+j<10)l[j]=regions[first+j];_snprintf(a,63,"%c %s",row==first?'>':' ',l[0]);_snprintf(b,63,"%c %s",row==first+1?'>':' ',l[1]);_snprintf(c,63,"%c %s",row==first+2?'>':' ',l[2]);_snprintf(d,63,"%c %s",row==first+3?'>':' ',l[3]);screen("SELECT REGION",a,b,c,d,"A SAVE  UP/DOWN  B BACK");DWORD q=pressed(false);if(q&XINPUT_GAMEPAD_RIGHT_THUMB){r59_world_chat_view();continue;}if(q&XINPUT_GAMEPAD_B)return;if(q&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%10;if(q&XINPUT_GAMEPAD_DPAD_UP)row=(row+9)%10;if(q&XINPUT_GAMEPAD_A){strncpy(r484_region,regions[row],sizeof(r484_region)-1);r484_region[sizeof(r484_region)-1]=0;r484_save_prefs();r57_last_presence=0;r59_world_last_poll=0;r47_consume_buttons();return;}Sleep(16);}}
static bool r48_room_setup(){
    R48Account360 *me=r48_me();if(!me)return false;r484_load_prefs();r484_remember_user(me->name);if(r484_saved_room[0]){strncpy(r48_room_name,r484_saved_room,sizeof(r48_room_name)-1);r48_room_name[sizeof(r48_room_name)-1]=0;}else{_snprintf(r48_room_name,sizeof(r48_room_name)-1,"%s ROOM",me->name);r48_room_name[sizeof(r48_room_name)-1]=0;}strncpy(r48_room_password,r484_saved_password,sizeof(r48_room_password)-1);r48_room_password[sizeof(r48_room_password)-1]=0;int row=0;r47_consume_buttons();
    for(;;){r57_presence_tick(true);char a[80],b[80],c[80],d[80];_snprintf(a,79,"%c ROOM NAME: %s",row==0?'>':' ',r48_room_name);if(r48_room_password[0])_snprintf(b,79,"%c ROOM PASSWORD: %s",row==1?'>':' ',r48_room_password);else _snprintf(b,79,"%c ROOM PASSWORD: NONE (OPTIONAL)",row==1?'>':' ');_snprintf(c,79,"%c START HOSTING",row==2?'>':' ');_snprintf(d,79,"%c BACK",row==3?'>':' ');a[79]=b[79]=c[79]=d[79]=0;screen("PUBLIC ROOM SETUP",a,b,c,d,"A SELECT  Y 30/60FPS  X CLEAR  B BACK",r69_requested_60?"60FPS: MENU DELAY HIGHER; RACE TARGET 60":"30FPS V10 - AUTO NAT / RELAY");DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_Y){r69_requested_60=!r69_requested_60;mknet::set_rate60(r69_requested_60);r47_consume_buttons();continue;}if(p&XINPUT_GAMEPAD_RIGHT_THUMB){r59_world_chat_view();continue;}if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%4;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+3)%4;if(row==1&&(p&XINPUT_GAMEPAD_X)){r48_room_password[0]=r484_saved_password[0]=0;r484_save_prefs();}
        if(p&XINPUT_GAMEPAD_A){if(row==0){char tmp[32];strncpy(tmp,r48_room_name,sizeof(tmp)-1);tmp[sizeof(tmp)-1]=0;if(r48_keyboard("ROOM NAME",tmp,sizeof(tmp),false,false)&&tmp[0]){strncpy(r48_room_name,tmp,sizeof(r48_room_name)-1);r48_room_name[sizeof(r48_room_name)-1]=0;strncpy(r484_saved_room,r48_room_name,sizeof(r484_saved_room)-1);r484_saved_room[sizeof(r484_saved_room)-1]=0;r484_save_prefs();}}else if(row==1){char tmp[20];strncpy(tmp,r48_room_password,sizeof(tmp)-1);tmp[sizeof(tmp)-1]=0;if(r48_keyboard("OPTIONAL ROOM PASSWORD",tmp,sizeof(tmp),false,false)){strncpy(r48_room_password,tmp,sizeof(r48_room_password)-1);r48_room_password[sizeof(r48_room_password)-1]=0;strncpy(r484_saved_password,r48_room_password,sizeof(r484_saved_password)-1);r484_saved_password[sizeof(r484_saved_password)-1]=0;r484_save_prefs();}}else if(row==2){strncpy(r484_saved_room,r48_room_name,sizeof(r484_saved_room)-1);strncpy(r484_saved_password,r48_room_password,sizeof(r484_saved_password)-1);r484_saved_room[sizeof(r484_saved_room)-1]=0;r484_saved_password[sizeof(r484_saved_password)-1]=0;r484_save_prefs();
            if(r69_requested_60){char tagged[32];_snprintf(tagged,sizeof(tagged)-1,"[60] %.18s",r48_room_name);tagged[sizeof(tagged)-1]=0;strcpy(r48_room_name,tagged);}
            mknet::set_rate60(r69_requested_60);return true;}else return false;}Sleep(16);}
}
static bool r48_register_room(){R48Account360 *me=r48_me();if(!me)return false;char msg[360],resp[256];_snprintf(msg,sizeof(msg)-1,"MKDIR2|REGISTER|%s|%s|%s|%s|6464|%u|4|X360|%s|%s|%s",me->id,me->secret,r48_room_name,r48_room_password,local_count,r69_requested_60?"11":"10",r69_requested_60?"BE610928":"BE100927",r484_region);msg[sizeof(msg)-1]=0;if(!r48_wait_prefix(msg,"MKDIR2|REGOK|",resp,sizeof(resp)))return false;if(sscanf(resp,"MKDIR2|REGOK|%19[^|]|%39s",r48_room_id,r48_room_token)!=2)return false;r48_room_registered=true;r48_last_heartbeat=r48_last_poll=0;r48_chat_seq=0;r50_chat_reset(r48_room_name,me->name);return true;}
static void r48_heartbeat(){if(!r48_room_registered)return;DWORD now=GetTickCount();if(r48_last_heartbeat&&now-r48_last_heartbeat<4000U)return;char m[180];_snprintf(m,sizeof(m)-1,"MKDIR2|HEARTBEAT|%s|%s|%u",r48_room_id,r48_room_token,lobby_slots());m[sizeof(m)-1]=0;bool first=(r48_last_heartbeat==0);int sr=r48_send(m);if(sr==SOCKET_ERROR)net_log("R56_ROOM: directory heartbeat send failed wsa=%d\n",WSAGetLastError());else if(first)net_log("R56_ROOM: public lobby heartbeat active\n");r48_last_heartbeat=now;}
static void r48_unregister_room(){if(r48_room_registered){char m[180];_snprintf(m,sizeof(m)-1,"MKDIR2|UNREGISTER|%s|%s",r48_room_id,r48_room_token);m[sizeof(m)-1]=0;r48_send(m);}r48_room_registered=false;r48_room_id[0]=r48_room_token[0]=0;}
static void r48_leave_room(){R48Account360 *me=r48_me();if(!me||!r48_room_id[0])return;char m[200];_snprintf(m,sizeof(m)-1,"MKDIR2|ROOM_LEAVE|%s|%s|%s",r48_room_id,me->id,me->secret);m[sizeof(m)-1]=0;for(int i=0;i<3;++i)r48_send(m);}
static void r48_poll_chat(){R48Account360 *me=r48_me();if(!me||!r48_room_id[0]||r48_dir_sock==INVALID_SOCKET)return;DWORD now=GetTickCount();if(r582_chat_resume&&now<r582_chat_resume)return;if(r581_chat_pending_ready&&r581_chat_pending[0]){char cm[360];_snprintf(cm,sizeof(cm)-1,"MKDIR2|CHAT_SEND|%s|%s|%s|%s",r48_room_id,me->id,me->secret,r581_chat_pending);cm[sizeof(cm)-1]=0;if(r48_send(cm)!=SOCKET_ERROR){r581_chat_pending_ready=false;r581_chat_pending[0]=0;r48_last_poll=0;r582_chat_resume=now+100U;net_log("R58.2_CHAT: queued message sent on isolated lobby frame\n");}return;}if(!r48_last_poll||now-r48_last_poll>=450U){char m[220];_snprintf(m,sizeof(m)-1,"MKDIR2|ROOM_POLL|%s|%s|%s|%u",r48_room_id,me->id,me->secret,r48_chat_seq);m[sizeof(m)-1]=0;r48_send(m);r48_last_poll=now;}
    for(;;){char b[512];sockaddr_in from;int flen=sizeof(from);int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);if(n<=0)break;b[n]=0;unsigned seq=0,sys=0;char who[20]={0},text[80]={0};if(r59_world_packet(b)){}else if(r61_social_packet(b)){}else if(!strncmp(b,"MKDIR2|STATUS|",14)){r57_parse_status(b);}else if(!strncmp(b,"MKDIR2|STATE|",13)){r57_parse_state(b);}else if(sscanf(b,"MKDIR2|CHAT|%u|%19[^|]|%79[^|]|%u",&seq,who,text,&sys)==4){if(seq>r48_chat_seq){r48_chat_seq=seq;r48_set_chat(who,text);}}else{unsigned end=0;if(sscanf(b,"MKDIR2|POLLEND|%u",&end)==1&&end>r48_chat_seq)r48_chat_seq=end;}}
}
static void r62_lobby_service(){if(!r62_modal_lobby)return;pump();if(r47_public_mode){if(hosting)r48_heartbeat();r57_presence_tick(false);r48_poll_chat();}}
static void r48_chat_compose(){R48Account360 *me=r48_me();if(!me||!r48_room_id[0])return;char text[56]="";if(!r48_keyboard("LOBBY CHAT",text,53,false,true)||!text[0])return;strncpy(r581_chat_pending,text,sizeof(r581_chat_pending)-1);r581_chat_pending[sizeof(r581_chat_pending)-1]=0;r581_chat_pending_ready=true;r582_chat_resume=GetTickCount()+150U;r47_consume_buttons();}
static void r59_world_compose(bool lobby_pump){char text[56]="";r59_world_keyboard=true;bool ok=r48_keyboard("WORLD CHAT",text,53,false,lobby_pump);r59_world_keyboard=false;if(!ok||!text[0]){r47_consume_buttons();return;}strncpy(r59_world_pending,text,sizeof(r59_world_pending)-1);r59_world_pending[sizeof(r59_world_pending)-1]=0;r59_world_pending_ready=true;r59_world_resume=GetTickCount()+150U;r47_consume_buttons();}
static void r59_world_chat_view(){bool old=r59_world_overlay_suppress;r59_world_overlay_suppress=true;r47_consume_buttons();for(;;){r57_presence_tick(true);IDirect3DDevice9 *dev=x360_d3d_device();if(dev){const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);int x=(int)(sw*.07f),mw=(int)(sw*.86f),ss=sw<900?1:2;net_text(dev,x,(int)(sh*.055f),"WORLD CHAT",sw<900?3:5,0xFFFFD050);net_text(dev,x,(int)(sh*.125f),"MARIO KART HUB - MADE BY SIRDANKZ",ss,0xFF90D0FF);bool any=false;int row=0;for(int i=0;i<6;++i){if(!r59_world_lines[i][0])continue;const char *w=r59_world_lines[i];net_text(dev,x,(int)(sh*(.22f+row*.085f)),w,r54_360_fit(w,sw<900?1:2,1,mw),0xFFFFFFFF);any=true;++row;}if(!any)net_text(dev,x,(int)(sh*.22f),"NO WORLD MESSAGES YET",ss,0xFFFFFFFF);net_text(dev,x,(int)(sh*.80f),"X SEND MESSAGE",sw<900?2:3,0xFF90D0FF);net_text(dev,x,(int)(sh*.88f),"RS / B CLOSE",ss,0xFFAAAAAA);dev->Present(0,0,0,0);}DWORD q=pressed(false);if(q&XINPUT_GAMEPAD_X){r59_world_compose(false);r47_consume_buttons();continue;}if((q&XINPUT_GAMEPAD_B)||(q&XINPUT_GAMEPAD_RIGHT_THUMB))break;Sleep(16);}r59_world_overlay_suppress=old;r47_consume_buttons();}
static int r48_fetch_rooms(R48Room360 rooms[8]){const char *q=r69_requested_60?"MKDIR2|LIST|11|BE610928":"MKDIR2|LIST|10|BE100927";r48_drain();int count=0;DWORD begin=GetTickCount(),last=0;bool ended=false;while(GetTickCount()-begin<1800U&&!ended){DWORD now=GetTickCount();if(!last||now-last>=400U){r48_send(q);last=now;}for(;;){char b[400];sockaddr_in from;int flen=sizeof(from);int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);if(n<=0)break;b[n]=0;if(r59_world_packet(b))continue;if(r61_social_packet(b))continue;if(!strncmp(b,"MKDIR2|STATUS|",14)){r57_parse_status(b);continue;}if(!strncmp(b,"MKDIR2|LISTEND",14)){ended=true;break;}if(!strncmp(b,"MKDIR2|ROOM|",12)&&count<8){R48Room360 r;memset(&r,0,sizeof(r));if(sscanf(b,"MKDIR2|ROOM|%19[^|]|%31[^|]|%u|%u|%15[^|]|%u|%19[^|]|%11[^\r\n]",r.id,r.name,&r.players,&r.max_players,r.platform,&r.locked,r.owner,r.region)==8){if(!strcmp(r.platform,"X360")){r.rate60=r69_requested_60?1U:0U;rooms[count++]=r;}}}}Sleep(10);}return count;}
static bool r48_join_room(const R48Room360 &r,sockaddr_in &target){R48Account360 *me=r48_me();if(!me)return false;r484_load_prefs();char pass[20];strncpy(pass,r484_saved_password,sizeof(pass)-1);pass[sizeof(pass)-1]=0;if(r.locked){if(!r48_keyboard("ROOM PASSWORD",pass,sizeof(pass),false,false))return false;}char msg[320],resp[320];_snprintf(msg,sizeof(msg)-1,"MKDIR2|JOIN_ROOM|%s|%s|%s|%s|%u|%s",r.id,me->id,me->secret,pass,join_local_count,r484_region);msg[sizeof(msg)-1]=0;if(!r48_wait_prefix(msg,"MKDIR2|JOIN|",resp,sizeof(resp))){if(strstr(resp,"BAD_PASSWORD")){screen("WRONG PASSWORD","THE ROOM PASSWORD WAS NOT ACCEPTED","","","","B BACK");while(!(pressed(false)&XINPUT_GAMEPAD_B))Sleep(16);}else if(strstr(resp,"ROOM_FULL")){screen("ROOM FULL","NOT ENOUGH RACER SLOTS FOR THIS CONSOLE","","","","B BACK");while(!(pressed(false)&XINPUT_GAMEPAD_B))Sleep(16);}return false;}char lid[20],ip[32];unsigned port=0;if(sscanf(resp,"MKDIR2|JOIN|%19[^|]|%31[^|]|%u",lid,ip,&port)!=3)return false;strncpy(r48_room_id,lid,sizeof(r48_room_id)-1);r48_room_id[sizeof(r48_room_id)-1]=0;r48_chat_seq=0;r48_last_poll=0;r50_chat_reset(r.name,me->name);
    r69_requested_60=r.rate60!=0;mknet::set_rate60(r69_requested_60);
    memset(&target,0,sizeof(target));target.sin_family=AF_INET;target.sin_addr.s_addr=inet_addr(ip);target.sin_port=htons((u_short)port);if(target.sin_addr.s_addr!=INADDR_NONE){if(pass[0]){strncpy(r484_saved_password,pass,sizeof(r484_saved_password)-1);r484_saved_password[sizeof(r484_saved_password)-1]=0;r484_save_prefs();}return true;}return false;}
static bool r48_browse_rooms(sockaddr_in &target){int selected=0;for(;;){r57_presence_tick(true);R48Room360 rooms[8];memset(rooms,0,sizeof(rooms));int count=r48_fetch_rooms(rooms);if(count==0){for(;;){r57_presence_tick(true);screen("PUBLIC ROOMS","NO COMPATIBLE X360 ROOMS","Y TO SWITCH 30/60 FPS","","","Y RATE    A REFRESH    B BACK");DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_Y){r69_requested_60=!r69_requested_60;mknet::set_rate60(r69_requested_60);r47_consume_buttons();break;}if(p&XINPUT_GAMEPAD_RIGHT_THUMB){r59_world_chat_view();continue;}if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_A)break;Sleep(16);}continue;}if(selected>=count)selected=count-1;for(;;){r57_presence_tick(true);char a[96]="",b[96]="",c[96]="",d[96];int first=selected-1;if(first<0)first=0;if(first>count-3)first=count-3;if(first<0)first=0;char *line[3]={a,b,c};for(int j=0;j<3;++j){int i=first+j;if(i>=count)continue;_snprintf(line[j],95,"%c %s [%s] %s %u/%u",i==selected?'>':' ',rooms[i].locked?"[LOCK]":"",rooms[i].region,rooms[i].name,rooms[i].players,rooms[i].max_players);line[j][95]=0;}_snprintf(d,sizeof(d)-1,"HOST: %s  REGION: %s",rooms[selected].owner,rooms[selected].region);d[sizeof(d)-1]=0;screen("PUBLIC ROOMS",a,b,c,d,"A JOIN  Y RATE  X REFRESH  B BACK",r69_requested_60?"60FPS V11 - X360 HOST, OG MAY JOIN":"30FPS V10 - X360 HOST, OG MAY JOIN");DWORD p=pressed(false);if(p&XINPUT_GAMEPAD_Y){r69_requested_60=!r69_requested_60;mknet::set_rate60(r69_requested_60);r47_consume_buttons();break;}if(p&XINPUT_GAMEPAD_RIGHT_THUMB){r59_world_chat_view();continue;}if(p&XINPUT_GAMEPAD_B)return false;if(p&XINPUT_GAMEPAD_X)break;if(p&XINPUT_GAMEPAD_DPAD_UP)selected=(selected+count-1)%count;if(p&XINPUT_GAMEPAD_DPAD_DOWN)selected=(selected+1)%count;if(p&XINPUT_GAMEPAD_A){if(r48_join_room(rooms[selected],target)){r47_consume_buttons();return true;}}Sleep(16);}}}
static bool lobby(bool host){
    mknet::set_rate60(r69_requested_60 && mknet::lobby_capacity()==4);
    if(host){if(!host_players_menu())return false;}
    else{if(!join_players_menu())return false;}
    if(!open_network(host))return false;failed=false;
    if(host){
        local_count=host_local_count;
        net_log("SPLIT_DIAG: host post-open restore local_count=%u; first remote slot=P%u\n",local_count,local_count+1);
    }
    {
        XINPUT_STATE c2;memset(&c2,0,sizeof(c2));
        const bool c2_connected=XInputGetState(1,&c2)==ERROR_SUCCESS;
        net_log("SPLIT_DIAG: lobby enter role=%s controller2=%s local_count=%u host_local_count=%u join_local_count=%u lobby_slots=%u\n",
                host?"HOST":"JOIN",c2_connected?"CONNECTED":"MISSING",
                local_count,host_local_count,join_local_count,lobby_slots());
    }
    if(!wait_for_xnet_route()){screen("NETWORK ERROR","NO XNET INTERNET ROUTE","CHECK NETWORK / LOG","B BACK","");Sleep(1500);close_network();return false;}
    strcpy(local_address,"LOCAL ADDRESS UNAVAILABLE");XNADDR addr;memset(&addr,0,sizeof(addr));XNetGetTitleXnAddr(&addr);if(addr.ina.s_addr)address_text(local_address,ntohl(addr.ina.s_addr),6464);
    if(r47_public_mode){if(!r48_dir_open()){screen("PUBLIC MATCH","MARIO KART HUB CONNECTION FAILED","CHECK NETWORK","","","B BACK");Sleep(1200);close_network();return false;}if(!r57_signed_in&&!r48_choose_account()){r48_dir_close();close_network();return false;}r57_signed_in=true;r57_presence_active=true;r484_load_prefs();r57_presence_tick(true);if(host&&!r48_room_setup()){r48_dir_close();close_network();return false;}}

    if(host){
        r42_public_ip_visible=true;bool mapped=false;
        if(r47_public_mode){strcpy(public_address,"AUTO NAT / RELAY");screen("PUBLIC MATCH","PREPARING PUBLIC ROOM","AUTOMATIC NAT TRAVERSAL","NO PORT FORWARD REQUIRED","");net_log("R58_NAT: Public Match uses Mario Kart Hub rendezvous\n");}
        else{strcpy(public_address,"DISCOVERING PUBLIC ADDRESS");public_ip(false);screen(mknet::lobby_capacity()==8?"HOST 4-8 PLAYER GAME":"HOST 2-4 PLAYER GAME",public_address,local_address,"SETTING UP UDP 6464 AUTOMATICALLY","");mapped=map_router(public_address);}
        net_log("MK64NET4: host ready public=%s upnp=%s\n",public_address,mapped?"OK":"NO");
        uint32_t partyIp=0;uint16_t partyPort=0;
        if(!r47_public_mode&&mknet::parse_endpoint(public_address,partyIp,partyPort)){
            HRESULT phr=x360_party_publish_host((unsigned int)partyIp,(unsigned short)partyPort,(unsigned int)mknet::get32(session));
            net_log("MK64NET4: Party host advertisement initial hr=0x%08X endpoint=%s\n",(unsigned)phr,public_address);
        }else if(!r47_public_mode){net_log("MK64NET4: Party host advertisement skipped - public endpoint unavailable\n");}
        if(r47_public_mode && !r48_register_room()){r48_dir_close();close_network();return false;}
        if(r47_public_mode&&!r58_nat_host(r48_room_id,r48_room_token)){net_log("R58.2_NAT: host transport init failed\n");r48_unregister_room();r48_dir_close();close_network();return false;}
    }else{
        if(r47_public_mode){if(!r48_browse_rooms(join_target)){r48_dir_close();close_network();return false;}R48Account360 *natme=r48_me();if(!natme||!r58_nat_join(r48_room_id,natme->id,natme->secret)){r48_leave_room();r48_dir_close();close_network();return false;}}
        else if(!host_ip_editor(join_target)){close_network();return false;}
        host_peer=join_target;char target[80];peer_text(target,join_target);net_log("MK64NET4: join target %s\n",target);
    }

    DWORD begin=GetTickCount();last_received=begin;DWORD last_hello=0;DWORD last_start_send=0;DWORD last_party_publish=0;
    while(!active&&!failed){
        pump();DWORD p=pressed();if(p&XINPUT_GAMEPAD_B){if(r47_public_mode){if(hosting)r48_unregister_room();else r48_leave_room();r48_dir_close();}close_network();return false;}DWORD now=GetTickCount();mesh_send_probes(now);
        if(r47_public_mode){if(hosting)r48_heartbeat();r57_presence_tick(false);r48_poll_chat();}
        if(r47_public_mode&&(p&kUiRightTriggerStats)){r62_modal_lobby=true;r61_profile_view(r48_me()->id,true);r62_modal_lobby=false;r47_consume_buttons();continue;}
        if(r47_public_mode&&(p&XINPUT_GAMEPAD_DPAD_RIGHT)){r62_modal_lobby=true;r70_room_view();r62_modal_lobby=false;r47_consume_buttons();continue;}
        if(r47_public_mode&&(p&XINPUT_GAMEPAD_Y)){r62_modal_lobby=true;r61_mailbox_view();r62_modal_lobby=false;r47_consume_buttons();continue;}
        if(r47_public_mode&&(p&XINPUT_GAMEPAD_RIGHT_THUMB)){r59_world_lobby_view=!r59_world_lobby_view;r47_consume_buttons();Sleep(32);continue;}
        if(hosting && !r47_public_mode && (p&XINPUT_GAMEPAD_Y)){
            r42_public_ip_visible=!r42_public_ip_visible;
            x360_menu_consume_current_buttons();
        }
        if(!hosting&&!host_session_known&&now-last_hello>=250){
            uint8_t hello[2]={uint8_t(join_local_count),uint8_t(mknet::PLATFORM_XBOX360)};int sent=send_packet_to(join_target,mknet::HELLO,nonce,hello,sizeof(hello));if(sent==SOCKET_ERROR){++net_tx_punch_fail;if(net_tx_punch_fail<=4||(net_tx_punch_fail%40)==0)net_log("MK64NET4: HELLO send fail count=%u wsa=%d\n",net_tx_punch_fail,WSAGetLastError());}else{++net_tx_punch;if(net_tx_punch<=2)net_log("MK64NET4: HELLO send OK count=%u\n",net_tx_punch);}last_hello=now;
        }

        if(hosting){
            prune_prestart_timeouts(now);int n=peer_count();
            if(now-last_party_publish>=2000){
                uint32_t pip=0;uint16_t pport=0;
                if(mknet::parse_endpoint(public_address,pip,pport))
                    x360_party_publish_host((unsigned int)pip,(unsigned short)pport,(unsigned int)mknet::get32(session));
                last_party_publish=now;
            }
            if(!start_sent&&(p&XINPUT_GAMEPAD_X)){
                            if(r47_public_mode){if(r59_world_lobby_view)r59_world_compose(true);else r48_chat_compose();r47_consume_buttons();Sleep(32);continue;}
                            else{uint32_t pip=0;uint16_t pport=0;if(mknet::parse_endpoint(public_address,pip,pport))x360_party_publish_host((unsigned int)pip,(unsigned short)pport,(unsigned int)mknet::get32(session));DWORD pir=x360_party_open_social_ui();net_log("MK64NET4: Party social UI result=0x%08X\n",(unsigned)pir);r47_consume_buttons();}
                        }
            if(!start_sent){for(int i=0;i<n;++i)if(now-peers[i].last_offer>=250)send_offer(i);if(n>=2&&all_ready()&&(!mesh_last_announce||now-mesh_last_announce>=500U)){for(int i=0;i<n;++i)send_mesh_info_to(i);mesh_last_announce=now;}}
            if(all_ready()&&r55_latency_ready()&&lobby_slots()>=(mknet::lobby_capacity()==8?4U:2U)&&!start_sent&&(p&XINPUT_GAMEPAD_A)){
                unsigned worst=0,second=0;
                for(int i=0;i<n;++i){unsigned b=peers[i].latency.budget();if(b>=worst){second=worst;worst=b;}else if(b>second)second=b;}
                player_count=lobby_slots();session_split=(local_count==2);
                net_log("SPLIT_DIAG: host start pre-map local_count=%u lobby_slots=%u peer_count=%d\n",
                        local_count,lobby_slots(),n);
                for(int i=0;i<n;++i)if(peers[i].local_count==2)session_split=true;
                crossplay=false;for(int i=0;i<n;++i)if(peers[i].platform==mknet::PLATFORM_OG_XBOX)crossplay=true;
                net_log("MK64NET10: session platform mode crossplay=%u (Xbox 360 authoritative host)\n",crossplay?1U:0U);
                if(crossplay)net_log("MK64NET19: retained STATE_SYNC recovery enabled\n");
                /* 2P keeps the proven direct-host formula. 3P/4P size the
                 * buffer for the longest early-relayed guest-to-guest path. */
                for(int i=0;i<n;++i)net_log("R55_LATENCY: START P%u samples=%u mean=%u var=%u budget=%u\n",peers[i].slot+1,peers[i].latency.samples,peers[i].latency.mean,peers[i].latency.variation,peers[i].latency.budget());
                unsigned relay_delay=(player_count==2)?mknet::input_delay_2p(worst):mknet::input_delay_early_relay(worst,second);bool mesh_ready=false;chosen_delay=relay_delay;if(player_count>2)mesh_ready=r57_mesh_delay(chosen_delay,relay_delay);
                net_log("R57_LATENCY: CHOSEN delay=%u players=%u worst_budget=%u second_budget=%u relay=%u mesh=%s\n",chosen_delay,player_count,worst,second,relay_delay,mesh_ready?"READY":"FALLBACK");
                local_slot=0;stream.reset(chosen_delay,player_count,0,local_count);boot.reset(player_count);bool host_local_boot_ok=true;if(local_count>1)host_local_boot_ok=boot.ready_span(1,local_count-1);start_sent=true;last_start_send=0;
                net_log("SPLIT_DIAG: host boot local span P1-P%u ready=%s\n",local_count,host_local_boot_ok?"YES":"NO");
                net_log("SPLIT_DIAG: host stream reset local_slot=%u local_count=%u player_count=%u session_split=%u\n",
                        local_slot,local_count,player_count,session_split?1U:0U);
                for(int i=0;i<n;++i)peers[i].acked=false;net_log("MK64NET4: starting %u players delay=%u budgetRTT=%u secondRTT=%u mode=%s\n",player_count,chosen_delay,worst,second,player_count==2?"2P-EARLY":"EARLY-RELAY");
            }
            if(start_sent&&now-last_start_send>=100){for(int i=0;i<n;++i)if(!peers[i].acked)send_start(i);last_start_send=now;}
            if(start_sent&&all_acked()){active=true;net_log("MK64NET4: all START ACKs received; host active\n");break;}

            char status[80],diag[80];
            if(start_sent)_snprintf(status,sizeof(status)-1,"STARTING %uP - ACK %d/%d",player_count,ack_count(),n);
            else if(n==0)_snprintf(status,sizeof(status)-1,"WAITING FOR PLAYERS (%u/%u)",local_count,mknet::lobby_capacity());
            else if(all_ready()&&r55_latency_ready()&&lobby_slots()>=(mknet::lobby_capacity()==8?4U:2U))_snprintf(status,sizeof(status)-1,"PLAYERS %d/%u READY - A START",lobby_slots(),mknet::lobby_capacity());
            else if(all_ready()&&!r55_latency_ready()){unsigned min_samples=0xFFFFFFFFU;for(int i=0;i<n;++i)if(peers[i].latency.samples<min_samples)min_samples=peers[i].latency.samples;if(min_samples>3U)min_samples=3U;_snprintf(status,sizeof(status)-1,"MEASURING NETWORK %u/3",min_samples);}
            else _snprintf(status,sizeof(status)-1,"PLAYERS %d/%u - READY %d/%d",lobby_slots(),mknet::lobby_capacity(),ready_count(),n);
            status[sizeof(status)-1]=0;
            _snprintf(diag,sizeof(diag)-1,"RX=%u V=%u H=%u R=%u",net_rx_total,net_rx_valid,net_rx_hello,net_rx_ready);diag[sizeof(diag)-1]=0;
            {
                            if(r47_public_mode)r50_public_lobby_screen("PUBLIC ROOM - HOST",status,true);
                            else{const bool partyReady=x360_party_is_active()!=0;const char *partyLine=partyReady?"X: OPEN FRIENDS - SELECT FRIEND - INVITE TO PARTY":"X: START/OPEN XBOX LIVE PARTY";const char *friendLine=partyReady?"FRIEND: JOIN PARTY, RUN MK360, CHOOSE JOIN, PRESS X":"AFTER STARTING PARTY: RETURN HERE AND PRESS X AGAIN";screen(mknet::lobby_capacity()==8?"HOST 4-8 PLAYER GAME":"HOST 2-4 PLAYER GAME",public_address,status,partyLine,friendLine);}
                        }
            if(start_sent){for(int i=0;i<n;++i)if(now-peers[i].last_received>30000){net_log("MK64NET4: P%u start timeout\n",peers[i].slot+1);failed=true;}}
        }else{
            if(r47_public_mode&&(p&XINPUT_GAMEPAD_X)){if(r59_world_lobby_view)r59_world_compose(true);else r48_chat_compose();r47_consume_buttons();Sleep(32);continue;}
            char status[80],diag[80];if(host_session_known)_snprintf(status,sizeof(status)-1,"ASSIGNED P%u - %u LOCAL PLAYER(S) - WAITING",assigned_slot+1,join_local_count);else _snprintf(status,sizeof(status)-1,"CONNECTING TO HOST");status[sizeof(status)-1]=0;
            _snprintf(diag,sizeof(diag)-1,"RX=%u V=%u OFFER=%u TX=%u F=%u",net_rx_total,net_rx_valid,net_rx_offer,net_tx_punch,net_tx_punch_fail);diag[sizeof(diag)-1]=0;
            if(r47_public_mode)r50_public_lobby_screen("PUBLIC ROOM - JOIN",status,false);else screen(mknet::lobby_capacity()==8?"JOIN 4-8 PLAYER GAME":"JOIN 2-4 PLAYER GAME",status,local_address,diag,"ALL GUESTS ENTER SAME HOST IP");
            if(now-last_received>30000){net_log("MK64NET4: join timeout after 30s\n");failed=true;}
        }
        Sleep(16); /* R59.1: menu refresh does not need a ~100 Hz spin. */
    }
    if(active){if(r47_public_mode){r62_begin_game();if(hosting)r48_unregister_room();/* Guest stays in roster until gameplay closes. Keep directory UDP socket for stats and presence. */}crossplay_release_gate();return true;}
    if(crossplay_host_required){for(;;){screen("XBOX 360 MUST HOST","OG XBOX CANNOT HOST CROSSPLAY","CHOOSE HOST ON XBOX 360","JOIN FROM OG XBOX","A RETURN");if(pressed()&XINPUT_GAMEPAD_A)break;Sleep(16);}}
    if(join_full){for(;;){screen("NOT ENOUGH RACER SLOTS","THE HOST CANNOT FIT THIS CONSOLE","TWO LOCAL PLAYERS NEED TWO FREE SLOTS","A RETURN","");if(pressed()&XINPUT_GAMEPAD_A)break;Sleep(16);}}
    if(r47_public_mode){if(hosting)r48_unregister_room();else r48_leave_room();r48_dir_close();}
    close_network();return false;
}

static bool r57_public_account_gate(){if(!initialized){if(!open_network(false))return false;close_network();}if(!r48_dir_open())return false;r484_load_prefs();if(!r57_signed_in){if(!r48_choose_account()){r48_dir_close();return false;}r57_signed_in=true;r59_world_reset();}r57_presence_active=true;r59_world_ui_active=true;r57_last_presence=0;r59_world_last_poll=0;r57_presence_tick(true);return true;}

static int r47_public_match_menu(void){
    if(!r57_public_account_gate())return false;
    int row=0,person=0;bool right=false;r61_offset=0;r61_last_list=0;r47_consume_buttons();
    for(;;){
        if(!r48_dir_open()){r57_signed_in=false;r57_presence_active=false;r59_world_ui_active=false;return false;}
        r57_presence_tick(true);r61_online_tick(false);r61_menu_draw(row,right,person);
        DWORD q=pressed(false);
        if(q&kUiRightTriggerStats){r61_profile_view(r48_me()->id,true);r61_last_list=0;continue;}
        if(q&XINPUT_GAMEPAD_Y){r61_profile_view(r48_me()->id);r61_last_list=0;continue;}
        if(q&XINPUT_GAMEPAD_X){r61_mailbox_view();r61_last_list=0;continue;}
        if(q&XINPUT_GAMEPAD_RIGHT_THUMB){r59_world_chat_view();r61_last_list=0;continue;}
        if((q&XINPUT_GAMEPAD_DPAD_RIGHT)&&r61_count_people()){right=true;person=0;r47_consume_buttons();continue;}
        if((q&XINPUT_GAMEPAD_DPAD_LEFT)&&right){right=false;r47_consume_buttons();continue;}
        if(q&XINPUT_GAMEPAD_B){if(right){right=false;r47_consume_buttons();continue;}row=5;}
        if(right){
            int n=r61_count_people();
            if((q&XINPUT_GAMEPAD_DPAD_DOWN)&&n){if(person+1<n)++person;else if(r61_offset+5<r61_total){r61_offset+=5;person=0;r61_online_tick(true);}else{r61_offset=0;person=0;r61_online_tick(true);}}
            if((q&XINPUT_GAMEPAD_DPAD_UP)&&n){if(person>0)--person;else if(r61_offset>0){r61_offset-=5;person=4;r61_online_tick(true);}else person=n-1;}
            if((q&XINPUT_GAMEPAD_A)&&person<n&&r61_people[person].found){char id[20];r61_copy(id,sizeof(id),r61_people[person].id);r61_profile_view(id);r61_online_tick(true);r47_consume_buttons();}
        }else{
            if(q&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%6;
            if(q&XINPUT_GAMEPAD_DPAD_UP)row=(row+5)%6;
            if(q&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B)){
                if(row==5){r57_signed_in=false;r57_presence_active=false;r59_world_ui_active=false;r57_online_count=-1;r48_dir_close();r47_consume_buttons();return false;}
                if(row==2){if(r73_solo_begin())return 2;r47_consume_buttons();continue;}
                if(row==3){r57_region_menu();r47_consume_buttons();continue;}
                if(row==4){r70_top_view();r47_consume_buttons();continue;}
                mknet::lobby_capacity()=4;r47_public_mode=true;bool ok=lobby(row==0);r47_public_mode=false;
                if(ok)return true;r48_dir_open();r57_presence_active=true;r59_world_ui_active=true;r57_last_presence=0;r59_world_last_poll=0;r61_last_list=0;r47_consume_buttons();
            }
        }
        Sleep(16);
    }
}
static void mk_stress_desc(char *dst,int size){if(!mk_stress_enabled())_snprintf(dst,size-1,"NET STRESS: OFF");else _snprintf(dst,size-1,"NET STRESS: %uMS J+-%u L%u%%",mk_stress_rtt_ms(),mk_stress_jitter_ms(),mk_stress_loss_pct());dst[size-1]=0;}
static void mk_stress_menu(void){
    int row=0;x360_menu_consume_current_buttons();
    for(;;){
        char a[80],b[80],c[80],d[80];
        if(mk_stress_rtt_ms())_snprintf(a,sizeof(a)-1,"%c SIMULATED RTT: %u MS",row==0?'>':' ',mk_stress_rtt_ms());else _snprintf(a,sizeof(a)-1,"%c SIMULATED RTT: OFF",row==0?'>':' ');a[sizeof(a)-1]=0;
        _snprintf(b,sizeof(b)-1,"%c RTT JITTER: +-%u MS",row==1?'>':' ',mk_stress_jitter_ms());b[sizeof(b)-1]=0;
        _snprintf(c,sizeof(c)-1,"%c PACKET LOSS: %u%%",row==2?'>':' ',mk_stress_loss_pct());c[sizeof(c)-1]=0;
        _snprintf(d,sizeof(d)-1,"SET SAME PROFILE ON ALL CONSOLES");d[sizeof(d)-1]=0;screen("NET STRESS TEST",a,b,c,d);
        DWORD p=pressed();if(p&XINPUT_GAMEPAD_B){x360_menu_consume_current_buttons();return;}if(p&XINPUT_GAMEPAD_A){x360_menu_consume_current_buttons();return;}if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+2)%3;if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%3;
        if(p&XINPUT_GAMEPAD_DPAD_RIGHT){if(row==0)mk_stress_rtt_index=(mk_stress_rtt_index+1)%(int)(sizeof(mk_stress_rtt_opts)/sizeof(mk_stress_rtt_opts[0]));else if(row==1)mk_stress_jitter_index=(mk_stress_jitter_index+1)%(int)(sizeof(mk_stress_jitter_opts)/sizeof(mk_stress_jitter_opts[0]));else mk_stress_loss_index=(mk_stress_loss_index+1)%(int)(sizeof(mk_stress_loss_opts)/sizeof(mk_stress_loss_opts[0]));mk_stress_clear_queue();}
        if(p&XINPUT_GAMEPAD_DPAD_LEFT){if(row==0){int n=(int)(sizeof(mk_stress_rtt_opts)/sizeof(mk_stress_rtt_opts[0]));mk_stress_rtt_index=(mk_stress_rtt_index+n-1)%n;}else if(row==1){int n=(int)(sizeof(mk_stress_jitter_opts)/sizeof(mk_stress_jitter_opts[0]));mk_stress_jitter_index=(mk_stress_jitter_index+n-1)%n;}else{int n=(int)(sizeof(mk_stress_loss_opts)/sizeof(mk_stress_loss_opts[0]));mk_stress_loss_index=(mk_stress_loss_index+n-1)%n;}mk_stress_clear_queue();}
        Sleep(16);
    }
}


static void controls_menu(){
    int player=0,row=0;bool dirty=false;
    x360_controls_load();
    for(;;){
        char title[64],lines[4][96];_snprintf(title,sizeof(title),"CONTROLLER %d - REBINDING",player+1);
        const int page=(row/4)*4;gR73ControlScrollRow=row;
        for(int i=0;i<4;++i){int item=page+i;
            if(item<14)_snprintf(lines[i],sizeof(lines[i]),"%c %s: %s",item==row?'>':' ',x360_control_action(item),x360_control_binding(player,item));
            else if(item==14)_snprintf(lines[i],sizeof(lines[i]),"%c STEERING: %s STICK",item==row?'>':' ',x360_control_stick(player,0)?"RIGHT":"LEFT");
            else if(item==15)_snprintf(lines[i],sizeof(lines[i]),"%c DEAD ZONE: %d PERCENT",item==row?'>':' ',x360_control_deadzone(player,0));
            else if(item==16)_snprintf(lines[i],sizeof(lines[i]),"%c STEERING SENSITIVITY: %d PERCENT",item==row?'>':' ',x360_control_sensitivity(player,0));
            else if(item==17)_snprintf(lines[i],sizeof(lines[i]),"%c IN-GAME MUSIC: %s",item==row?'>':' ',x360_music_enabled()?"ON":"OFF");
            else lines[i][0]=0;
            lines[i][sizeof(lines[i])-1]=0;
        }
        screen(title,lines[0],lines[1],lines[2],lines[3],"A SELECT   X CLEAR   Y DEFAULTS   UP/DOWN SCROLL","LB/RB CONTROLLER   LEFT/RIGHT ADJUST   B BACK");
        DWORD p=pressed(false);
        if(p&XINPUT_GAMEPAD_B){gR73ControlScrollRow=-1;if(dirty){int saved=x360_controls_save();screen(saved?"CONTROLS SAVED":"CONTROLS ACTIVE",saved?"READY FOR NEXT LAUNCH":"COULD NOT SAVE TO GAME FOLDER","",saved?"":"USING THESE SETTINGS FOR THIS LAUNCH","","","B BACK");Sleep(650);}return;}
        if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%18;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+17)%18;
        if(p&XINPUT_GAMEPAD_LEFT_SHOULDER)player=(player+3)%4;if(p&XINPUT_GAMEPAD_RIGHT_SHOULDER)player=(player+1)%4;
        if(p&XINPUT_GAMEPAD_Y){x360_control_defaults(player);dirty=true;}
        if(row<14&&(p&XINPUT_GAMEPAD_X)){x360_control_bind(player,row,255);dirty=true;}
        if(row==14&&(p&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT))){x360_control_stick(player,1);dirty=true;}
        if(row==15&&(p&(XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT))){x360_control_deadzone(player,(p&XINPUT_GAMEPAD_DPAD_LEFT)?-1:1);dirty=true;}
        if(row==16&&(p&(XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT))){x360_control_sensitivity(player,(p&XINPUT_GAMEPAD_DPAD_LEFT)?-5:5);dirty=true;}
        if(row==17&&(p&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT)))x360_music_set_enabled(!x360_music_enabled());
        if(row<14&&(p&XINPUT_GAMEPAD_A)){
            bool released=false;
            for(;;){screen("PRESS A CONTROL",x360_control_action(row),"BUTTON, TRIGGER OR STICK DIRECTION","BACK CANCELS THIS BINDING","","RELEASE THE CURRENT CONTROL FIRST","MENU NAVIGATION ALWAYS USES DEFAULT BUTTONS");
                unsigned down=x360_controls_down();if(!down)released=true;
                if(released&&down){if(down&(1U<<8))break;for(int b=0;b<24;++b)if(down&(1U<<b)){x360_control_bind(player,row,b);dirty=true;break;}break;}
                Sleep(16);
            }
            // Consume the capture press; it must not also clear/reset another row.
            XINPUT_STATE state;memset(&state,0,sizeof(state));XInputGetState(0,&state);prev_buttons=state.Gamepad.wButtons;sUiRightTriggerPrev=(state.Gamepad.bRightTrigger>0x40);
        }
        Sleep(16);
    }
}
/* R75 PUBLIC BUILD: logging-menu UI omitted. */
static void options_menu(){
    int row=0;bool save_failed=false;x360_menu_consume_current_buttons();
    const int row_count=3;
    for(;;){
        char aspect[64],controls[80],third[80],fourth[80];
        _snprintf(aspect,sizeof(aspect)-1,"%c ASPECT: %s",row==0?'>':' ',display_wide?"16:9":"4:3");aspect[sizeof(aspect)-1]=0;
        _snprintf(controls,sizeof(controls)-1,"%c CONTROLLER REBINDING",row==1?'>':' ');controls[sizeof(controls)-1]=0;
        _snprintf(third,sizeof(third)-1,"%c IN-GAME MUSIC: %s",row==2?'>':' ',x360_music_enabled()?"ON":"OFF");third[sizeof(third)-1]=0;
        fourth[0]=0;
        screen("OPTIONS",aspect,controls,third,fourth,"A SELECT    LEFT/RIGHT CHANGE    B BACK",
            save_failed?"MUSIC CHANGED; COULD NOT SAVE TO DISK":"DPAD LEFT/RIGHT ALSO STEERS IN GAME");
        DWORD p=pressed(false);
        if(p&XINPUT_GAMEPAD_B){x360_menu_consume_current_buttons();return;}
        if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+row_count-1)%row_count;
        if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%row_count;
        if(row==0&&(p&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT)))display_wide=!display_wide;
        if(row==1&&(p&XINPUT_GAMEPAD_A)){controls_menu();x360_menu_consume_current_buttons();}
        if(row==2&&(p&(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT)))save_failed=!x360_music_set_enabled(!x360_music_enabled());
        Sleep(16);
    }
}

static bool input_test_active=false;
static uint32_t input_test_hash=2166136261U;
static Race8Lobby raceLobby;

static int r485_direct_play_menu(){int row=0;r47_consume_buttons();for(;;){char a[80],b[80],c[80],d[80];_snprintf(a,79,"%c HOST 2-4 PLAYER GAME",row==0?'>':' ');_snprintf(b,79,"%c JOIN 2-4 PLAYER GAME",row==1?'>':' ');_snprintf(c,79,"%c HOST 4-8 PLAYER GAME",row==2?'>':' ');_snprintf(d,79,"%c JOIN 4-8 PLAYER GAME",row==3?'>':' ');a[79]=b[79]=c[79]=d[79]=0;screen("DIRECT PLAY",a,b,c,d,"A SELECT  Y 30/60FPS  B BACK",r69_requested_60?"60FPS: MENUS MORE DELAY; RACE TARGET 60":"30FPS V10 - OG CROSSPLAY AVAILABLE");DWORD p=pressed();if(p&XINPUT_GAMEPAD_Y){r69_requested_60=!r69_requested_60;mknet::set_rate60(r69_requested_60);r47_consume_buttons();continue;}if(p&XINPUT_GAMEPAD_B){r47_consume_buttons();return -1;}if(p&XINPUT_GAMEPAD_DPAD_DOWN)row=(row+1)%4;if(p&XINPUT_GAMEPAD_DPAD_UP)row=(row+3)%4;if(p&XINPUT_GAMEPAD_A){r47_consume_buttons();return row;}Sleep(16);}}

extern "C" int x360_net_boot_menu(void){
#if !MK64_ENABLE_LOGGER_OPTIONS
    x360_set_logging(0);net_set_logging(false);x360_party_set_logging(0);
#endif
    x360_controls_load();r69_requested_60=false;mknet::set_rate60(false);int selection=0;
    for(;;){IDirect3DDevice9 *dev=x360_d3d_device();if(dev){const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);net_text(dev,72,54,"MARIO KART 64 - ONLINE",5,0xFFFFD050);const char *labels[]={"OFFLINE","DIRECT PLAY","PUBLIC MATCH - UNRANKED","OPTIONS / CONTROLS"};for(int row=0;row<4;++row){char line[96];_snprintf(line,sizeof(line),"%c %s",row==selection?'>':' ',labels[row]);net_text(dev,72,190+row*90,line,3,row==selection?0xFFFFD050:0xFFFFFFFF);}char r55diag[96];_snprintf(r55diag,sizeof(r55diag)-1,"A SELECT  UP/DOWN");r55diag[sizeof(r55diag)-1]=0;net_text(dev,72,600,r55diag,3,0xFFAAAAAA);net_text(dev,72,646,"DIRECT PLAY KEEPS 2-4 AND 4-8 MODES",3,0xFFAAAAAA);dev->Present(0,0,0,0);}
        DWORD p=pressed();if(p&XINPUT_GAMEPAD_DPAD_DOWN)selection=(selection+1)%4;if(p&XINPUT_GAMEPAD_DPAD_UP)selection=(selection+3)%4;if(!(p&XINPUT_GAMEPAD_A)){Sleep(16);continue;}
        if(selection==0){close_network();return 0;}
        if(selection==3){options_menu();continue;}
        if(selection==2){int public_result=r47_public_match_menu();if(public_result==2)return 0;if(public_result==1){if(x360_net8_active()){race8_lobby_init(&raceLobby,(int)player_count);x360_net8_configure();}return 1;}continue;}
        int d=r485_direct_play_menu();if(d<0)continue;mknet::lobby_capacity()=(d>=2)?8:4;if(d>=2)r69_requested_60=false;mknet::set_rate60(r69_requested_60);bool host=(d==0||d==2);if(lobby(host)){if(x360_net8_active()){race8_lobby_init(&raceLobby,(int)player_count);x360_net8_configure();}return 1;}while(true){screen("CONNECTION NOT ESTABLISHED","NO GAME WAS STARTED","CHECK HOST IP / UDP 6464","","","A RETURN TO MENU");if(pressed()&XINPUT_GAMEPAD_A)break;Sleep(16);}r47_consume_buttons();
    }
}

extern "C" int x360_net_return_requested(void){return return_to_premenu?1:0;}
extern "C" void x360_net_clear_return_request(void){return_to_premenu=false;}
extern "C" void x360_net_full_restart(void){r73_solo_exit();close_network();XLaunchNewImage("game:\\MK64.xex",0);XLaunchNewImage("game:\\default.xex",0);XLaunchNewImage(0,0);}
extern "C" void x360_net_request_return(void){
    return_to_premenu=true;
    close_network(); /* sends GOODBYE immediately before closing */
}

extern "C" int x360_net8_active(void){return active&&(mknet::lobby_capacity()==8||session_split);}
extern "C" int x360_net_extended_lobby(void){return active&&mknet::lobby_capacity()==8;}
extern "C" int x360_net_active(void){return active?1:0;}
extern "C" int x360_net_60fps_session(void){return (active&&mknet::rate60()&&mknet::lobby_capacity()==4)?1:0;}
extern "C" int x360_net_crossplay(void){return active&&crossplay?1:0;}
extern "C" int x360_net_player_count(void){return active?(int)player_count:1;}
extern "C" int x360_net_local_slot(void){return active?(int)local_slot:0;}
extern "C" int x360_net_local_count(void){return active?(int)local_count:1;}
extern "C" int x360_net_is_local(int slot){return active&&slot>=(int)local_slot&&slot<(int)(local_slot+local_count);}
extern "C" unsigned int x360_net_frame(void){return stream.frame;}
struct NetPadCompat {unsigned short button;signed char stick_x,stick_y;unsigned char err_no;};

static void send_crossplay_state_snapshot(){
    if(!active||!hosting||!crossplay)return;
    uint8_t payload[4+mknet::CROSS_STATE_BYTES];
    if(menu_sync && !crossplay_states.find(stream.frame)){
        int n=x360_crossplay_state_pack(payload+4,mknet::CROSS_STATE_BYTES);
        if(n!=mknet::CROSS_STATE_BYTES)return;
        crossplay_states.save(stream.frame,payload+4);
    }
    int pc=peer_count();
    for(int i=0;i<pc;++i){
        if(peers[i].platform!=mknet::PLATFORM_OG_XBOX)continue;
        uint32_t frames[3];
        unsigned count=crossplay_states.replay_frames(stream.peer_frame[peers[i].slot],frames,peers[i].slot);
        for(unsigned j=0;j<count;++j){
            mknet::put32(payload,frames[j]);
            memcpy(payload+4,crossplay_states.find(frames[j]),mknet::CROSS_STATE_BYTES);
            int sr=send_peer_message(i,mknet::STATE_SYNC,payload,sizeof(payload));
            if(sr==SOCKET_ERROR)++net_tx_input_fail;
        }
    }
}

static bool menu_frame_window_ready(){
    if(!menu_sync)return true;
    if(!crossplay)return true;
    /* During crossplay menus/fades/countdown the Xbox 360 host is the commit
     * authority. Host already requires every player's delayed input before
     * stream.consume(). Guests only consume frames seen in a complete host
     * FRAMESET, preventing semantic screens from passing each other. */
    if(hosting)return true;
    return have_host_commit && stream.frame<=host_commit_frame;
}



/* MK64_ASTRA_TRACE_R17: synchronized input/hash history dumped only on fault. */
extern "C" void mk64_astra_diag_dump(void);extern "C" void mk64_astra_rng_dump(void);
static void astra_r17_dump_net_history(void){uint32_t cur=stream.frame,first=cur>24U?cur-24U:0U;net_log("ASTRA_NET_BEGIN SIDE=360 FIRST=%u LAST=%u\n",(unsigned)first,(unsigned)cur);for(uint32_t f=first;f<=cur;++f){const mknet::HashSlot &lh=stream.hashes[f%mknet::HISTORY];for(unsigned s=0;s<player_count&&s<4;++s){const mknet::InputSlot &in=stream.inputs[s][f%mknet::HISTORY];const mknet::HashSlot &ph=stream.peer_hashes[s][f%mknet::HISTORY];if(in.present&&in.frame==f)net_log("ASTRA_NET SIDE=360 F=%u P=%u B=%04X X=%d Y=%d LH=%08X LHP=%u PH=%08X PHP=%u\n",(unsigned)f,s+1,in.pad.buttons,(int)in.pad.x,(int)in.pad.y,(unsigned)((lh.present&&lh.frame==f)?lh.value:0),(lh.present&&lh.frame==f)?1U:0U,(unsigned)((ph.present&&ph.frame==f)?ph.value:0),(ph.present&&ph.frame==f)?1U:0U);}}net_log("ASTRA_NET_END SIDE=360\n");}

/* MK64_CROSSPLAY_R25_ASTRA_GOLD_TRACE */

extern "C" unsigned int x360_crossplay_r25_count(void);
extern "C" int x360_crossplay_r25_get(unsigned int index,unsigned int *out,int outCount);
extern "C" unsigned int mk64_r27_speed_probe(void);
extern "C" unsigned int mk64_crossplay_r27_count(void);
extern "C" int mk64_crossplay_r27_get(unsigned int index, unsigned int *out, int outCount);
static void r25_astra_dump(uint32_t mismatch){
    if(!crossplay_diagnostics_enabled)return;
    unsigned int n=x360_crossplay_r25_count(),w[88];const char *path="game:\\mk64-astra-r25.log";
    HANDLE f=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(f==INVALID_HANDLE_VALUE){path="mk64-astra-r25.log";f=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);}
    net_log("R25_ASTRA_GOLD mismatch=%u records=%u file=%s ok=%u\n",mismatch,n,path,f!=INVALID_HANDLE_VALUE?1U:0U);if(f==INVALID_HANDLE_VALUE)return;
    char line[1536];DWORD wrote=0;int m=_snprintf(line,sizeof(line)-1,"R25_HEADER SIDE=360 mismatch=%u records=%u words=88 demo_filtered=1 phases=1,10-21,30-32\r\n",mismatch,n);line[sizeof(line)-1]=0;if(m>0)WriteFile(f,line,(DWORD)strlen(line),&wrote,NULL);
    for(unsigned int k=0;k<n;++k){
        if(!x360_crossplay_r25_get(k,w,88))continue;
        m=_snprintf(line,sizeof(line)-1,"R25_PHASE SIDE=360 I=%u F=%u PH=%u TK=%08X GT=%u RS=%u SEED=%04X CT=%08X VT=%08X DEMO=%u TICKS=%u PC=%u SM=%u MODE=%u KIN=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X LOG=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X\r\n",
          k,w[0],w[1],w[2],w[3],w[4],w[5]&0xFFFFU,w[6],w[7],w[8],w[9],w[10],(w[11]>>16)&0xFFFFU,w[11]&0xFFFFU,w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21],w[22],w[23],w[24],w[25],w[26],w[27]);line[sizeof(line)-1]=0;if(m>0){if(m>(int)sizeof(line)-1)m=(int)sizeof(line)-1;WriteFile(f,line,(DWORD)m,&wrote,NULL);}
        for(int p=0;p<2;++p){unsigned int *d=&w[28+p*30];
          m=_snprintf(line,sizeof(line)-1,"R25_P SIDE=360 I=%u F=%u PH=%u TK=%08X P=%d TYPE=%04X LAP=%d RANK=%d PATH=%d ITEM=%d EFF=%08X TRIG=%08X PROP=%04X POS=%08X,%08X,%08X VEL=%08X,%08X,%08X SPD=%08X CUR=%08X PREV=%08X OLD=%08X,%08X,%08X ROTY=%04X SLOPE=%04X U98=%08X U8C=%08X BBOX=%08X SURF2=%08X ORI=%08X,%08X,%08X U90=%08X RSV=%08X BTN=%04X X=%d Y=%d BP=%04X BD=%04X\r\n",
           k,w[0],w[1],w[2],p+1,d[0]&0xFFFFU,(int)(short)(d[1]>>16),(int)(short)(d[1]&0xFFFFU),(int)(short)(d[2]>>16),(int)(short)(d[2]&0xFFFFU),d[3],d[4],d[5]&0xFFFFU,d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15],d[16],d[17],(d[18]>>16)&0xFFFFU,d[18]&0xFFFFU,d[19],d[20],d[21],d[22],d[23],d[24],d[25],d[26],d[27],(d[28]>>16)&0xFFFFU,(int)(signed char)((d[28]>>8)&0xFFU),(int)(signed char)(d[28]&0xFFU),(d[29]>>16)&0xFFFFU,d[29]&0xFFFFU);line[sizeof(line)-1]=0;if(m>0){if(m>(int)sizeof(line)-1)m=(int)sizeof(line)-1;WriteFile(f,line,(DWORD)m,&wrote,NULL);}
        }
    }    m=_snprintf(line,sizeof(line)-1,"R31_HEADER SIDE=360 build=R31-COLLISION-CANON words=56 anchor=1024 recent=2048 exact_gp_hash=1 speed6=%08X\r\n",mk64_r27_speed_probe());
    if(m>0)WriteFile(f,line,(DWORD)m,&wrote,NULL);
    for (unsigned int k=0;k<mk64_crossplay_r27_count();++k) {
        unsigned int c[56]; if (!mk64_crossplay_r27_get(k,c,56)) continue;
        m=_snprintf(line,sizeof(line)-1,"R27_CPU SIDE=360 I=%u F=%u TK=%u ST=%u P=%u W=",k,c[0],c[1],c[2],c[3]+1);
        for (unsigned int j=0;j<56 && m>0 && m<1400;++j) m+=_snprintf(line+m,sizeof(line)-1-m,j?",%08X":"%08X",c[j]);
        if(m>0 && m<1400){line[m++]='\r';line[m++]='\n';WriteFile(f,line,(DWORD)m,&wrote,NULL);}
    }
    CloseHandle(f);
}

static void crossplay_fault_log(){
    if(!crossplay_diagnostics_enabled)return;
    static bool written=false;if(written)return;written=true;
    uint32_t mf=0xFFFFFFFFU,lh=0,ph=0;unsigned ms=0;
    for(unsigned back=0;back<16;++back){
        if(back>stream.frame)break;uint32_t f=stream.frame-back;
        const mknet::HashSlot &a=stream.hashes[f%mknet::HISTORY];if(!a.present||a.frame!=f)continue;
        for(unsigned slot=0;slot<player_count;++slot){
            if(slot>=local_slot&&slot<local_slot+local_count)continue;
            const mknet::HashSlot &b=stream.peer_hashes[slot][f%mknet::HISTORY];
            if(b.present&&b.frame==f&&a.value!=b.value){mf=f;lh=a.value;ph=b.value;ms=slot;break;}
        }
        if(mf!=0xFFFFFFFFU)break;
    }
    r25_astra_dump(mf);
    char text[4096];int used=0;
    used+=_snprintf(text+used,sizeof(text)-used-1,
        "CROSSPLAY_FAULT role=%s frame=%u menuSync=%u localSlot=%u localCount=%u players=%u latestLocal=%u latestComplete=%u\r\n",
        hosting?"HOST":"JOIN",(unsigned)stream.frame,menu_sync?1U:0U,local_slot,local_count,player_count,
        (unsigned)stream.latest_local,(unsigned)stream.latest_complete);
    if(mf!=0xFFFFFFFFU)used+=_snprintf(text+used,sizeof(text)-used-1,
        "CROSSPLAY_HASH_MISMATCH frame=%u local=%08X peerP%u=%08X\r\n",
        (unsigned)mf,(unsigned)lh,ms+1,(unsigned)ph);
    else used+=_snprintf(text+used,sizeof(text)-used-1,
        "CROSSPLAY_FAULT no recent hash mismatch found; possible conflicting input/history fault\r\n");
    for(unsigned slot=0;slot<player_count&&used<(int)sizeof(text)-80;++slot)
        used+=_snprintf(text+used,sizeof(text)-used-1,"CROSSPLAY_PEER_FRAME P%u=%u\r\n",slot+1,(unsigned)stream.peer_frame[slot]);
    {
        unsigned char st[mknet::CROSS_STATE_BYTES];
        int sn=x360_crossplay_state_pack(st,sizeof(st));
        static const char d[]="0123456789ABCDEF";
        if(used<(int)sizeof(text)-32)used+=_snprintf(text+used,sizeof(text)-used-1,"CROSSPLAY_STATE_HEX ");
    mkdiag_write_component_snapshot(); /* MK64_CROSSPLAY_COMPONENT_DIAG_R15_CALL */
        for(int i=0;i<sn&&used+2<(int)sizeof(text)-3;++i){text[used++]=d[st[i]>>4];text[used++]=d[st[i]&15];}
        if(used<(int)sizeof(text)-3){text[used++]='\r';text[used++]='\n';}
    }
    if(used<0)used=0;if(used>=(int)sizeof(text))used=(int)sizeof(text)-1;text[used]=0;
    OutputDebugStringA(text);x360_log(text);
    const char *faultPath="game:\\mk64-crossplay-fault.log";
    HANDLE f=CreateFileA(faultPath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    if(f==INVALID_HANDLE_VALUE){
        faultPath="mk64-crossplay-fault.log";
        f=CreateFileA(faultPath,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
    }
    bool faultFileOk=(f!=INVALID_HANDLE_VALUE);
    DWORD faultErr=faultFileOk?0:GetLastError();
    if(faultFileOk){DWORD wrote=0;WriteFile(f,text,(DWORD)strlen(text),&wrote,NULL);CloseHandle(f);}
    net_log("R10_FAULT_LOG: path=%s ok=%u err=%lu\n",faultPath,faultFileOk?1U:0U,(unsigned long)faultErr);

    astra_r17_dump_net_history();
    mk64_astra_diag_dump();
    mk64_astra_rng_dump();
}
extern "C" void x360_net_set_menu_sync(int enabled){menu_sync=enabled!=0;}
extern "C" int x360_net_menu_sync_active(void){return menu_sync?1:0;}

static bool gameplay_timeout(DWORD now){
    if(hosting){int n=peer_count();for(int i=0;i<n;++i){DWORD timeout=peers[i].gameplay_seen?15000:60000;if(now-peers[i].last_received>timeout){net_log("MK64NET4: P%u input timeout age=%u\n",peers[i].slot+1,(unsigned)(now-peers[i].last_received));return true;}}return false;}
    DWORD timeout=first_gameplay_input?15000:60000;return now-last_received>timeout;
}

extern "C" void x360_netplay_r62_observe(int state,int mode,int course,int cup,int winner){r62_observe_result(state,mode,course,cup,winner);}
extern "C" void x360_net_controllers(void *pads_,int count){
    if(!active||count<2)return;NetPadCompat *pads=(NetPadCompat*)pads_;
    r71_refresh_device();r62_game_tick();
    if(netplay_logging_enabled && split_diag_samples<20){
        XINPUT_STATE xs[4];DWORD xr[4];
        for(DWORD user=0;user<4;++user){
            memset(&xs[user],0,sizeof(xs[user]));
            xr[user]=XInputGetState(user,&xs[user]);
        }
        const int extra=(xr[1]==ERROR_SUCCESS)?1:
                        (xr[2]==ERROR_SUCCESS)?2:
                        (xr[3]==ERROR_SUCCESS)?3:-1;
        net_log("SPLIT_DIAG_V2: sample=%u role=%s local_slot=%u local_count=%u players=%u "
                "phys=0:%s 1:%s 2:%s 3:%s extra=%d "
                "pad0(e=%u b=%04X x=%d y=%d) pad1(e=%u b=%04X x=%d y=%d)\n",
                split_diag_samples,hosting?"HOST":"JOIN",local_slot,local_count,player_count,
                xr[0]==ERROR_SUCCESS?"OK":"MISS",
                xr[1]==ERROR_SUCCESS?"OK":"MISS",
                xr[2]==ERROR_SUCCESS?"OK":"MISS",
                xr[3]==ERROR_SUCCESS?"OK":"MISS",
                extra,
                (unsigned)pads[0].err_no,(unsigned)pads[0].button,(int)pads[0].stick_x,(int)pads[0].stick_y,
                (unsigned)pads[1].err_no,(unsigned)pads[1].button,(int)pads[1].stick_x,(int)pads[1].stick_y);
        ++split_diag_samples;
    }
    /* All peers reach the first controller read after shaders and game threads
     * are initialized. Keep the input clock stopped until every peer is here. */
    if(!boot.complete){
        DWORD begin=GetTickCount(),sent=0;
        while(!boot.complete&&!failed&&GetTickCount()-begin<60000){
            DWORD now=GetTickCount();
            if(!hosting&&(!sent||now-sent>=100)){send_host_message(mknet::BOOT_READY,0,0);sent=now;}
            mesh_send_probes(now);
            pump();
            if(hosting){
                int pc=peer_count();
                if(boot.all_ready()){boot.complete=true;for(int i=0;i<pc;++i){peers[i].last_received=now;send_peer_message(i,mknet::BOOT_GO,0,0);}}
            }
            XINPUT_STATE cancel;memset(&cancel,0,sizeof(cancel));XInputGetState(0,&cancel);
            if((cancel.Gamepad.wButtons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START))failed=true;
            if(!boot.complete)Sleep(1);
        }
        if(!boot.complete)failed=true;
    }
    mknet::Pad input[2];
    for(unsigned i=0;i<local_count;++i){input[i].buttons=pads[i].err_no?0:pads[i].button;input[i].x=pads[i].err_no?0:pads[i].stick_x;input[i].y=pads[i].err_no?0:pads[i].stick_y;}
    DWORD last_state_send=0;
    if(hosting&&crossplay){send_crossplay_state_snapshot();last_state_send=GetTickCount();}
    stream.sample_locals(input,input_test_active?input_test_hash:x360_net_state_hash());DWORD begin=GetTickCount(),sent=0;DWORD last_wait_screen=0;uint32_t sent_complete=0xFFFFFFFFU;mknet::Pad frame_pads[mknet::MAX_PLAYERS];
    static unsigned r55_wait_samples=0,r55_wait_nonzero=0,r55_wait_sum=0,r55_wait_max=0;
    while(true){
        pump();DWORD now=GetTickCount();mesh_send_probes(now);
        if(hosting&&crossplay&&(!last_state_send||now-last_state_send>=(mknet::rate60()?12U:15U))){send_crossplay_state_snapshot();last_state_send=now;}
        if(!sent||now-sent>=(mknet::rate60()?12U:15U)||(hosting&&sent_complete!=stream.latest_complete)){
            uint8_t packet[mknet::MAX_PACKET];int n;
            if(hosting){
                int pc=peer_count();
                /* Send P1 input immediately to every guest, without waiting for
                 * the other guest inputs needed for a complete FRAMESET. */
                for(int i=0;i<pc;++i){
                    n=stream.client_packet(packet,session,peers[i].slot);
                    if(!n)break;
                    int sr=r58_send_game_packet(peers[i].addr,packet,n,true);++net_tx_input;
                    if(sr==SOCKET_ERROR){++net_tx_input_fail;if(net_tx_input_fail<=12)net_log("MK64NET4: HOST_INPUT early send fail P%u tx=%u frame=%u wsa=%d\n",peers[i].slot+1,net_tx_input,(unsigned)stream.frame,WSAGetLastError());}
                }
                /* Keep authoritative complete FRAMESET packets for redundancy,
                 * recovery and deterministic cross-checking. */
                for(int i=0;i<pc;++i){n=stream.frameset_packet(packet,session,peers[i].slot);if(!n)break;int sr=r58_send_game_packet(peers[i].addr,packet,n,true);++net_tx_input;if(sr==SOCKET_ERROR){++net_tx_input_fail;if(net_tx_input_fail<=12)net_log("MK64NET4: FRAMESET send fail P%u tx=%u frame=%u wsa=%d\n",peers[i].slot+1,net_tx_input,(unsigned)stream.frame,WSAGetLastError());}}
            }else{
                n=stream.client_packet(packet,session);if(!n)break;int sr=r58_send_game_packet(host_peer,packet,n,true);++net_tx_input;if(sr==SOCKET_ERROR){++net_tx_input_fail;if(net_tx_input_fail<=12)net_log("MK64NET4: CLIENT_INPUT send fail P%u tx=%u frame=%u wsa=%d\n",local_slot+1,net_tx_input,(unsigned)stream.frame,WSAGetLastError());}
                /* R56A direct guest-to-guest duplicate. Host relay remains active. */
                if(player_count>2){for(unsigned mi=0;mi<mknet::MAX_PLAYERS-1;++mi){MeshPeerState &m=mesh_peers[mi];if(!m.used)continue;n=stream.client_packet(packet,session,m.slot);if(!n)break;sr=r58_send_game_packet(m.addr,packet,n,false);++mesh_direct_tx;++net_tx_input;if(sr==SOCKET_ERROR)++net_tx_input_fail;}}
            }
            sent=now;sent_complete=stream.latest_complete;
        }
        XINPUT_STATE cancelState;memset(&cancelState,0,sizeof(cancelState));XInputGetState(0,&cancelState);
        if((cancelState.Gamepad.wButtons&(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START))==(XINPUT_GAMEPAD_BACK|XINPUT_GAMEPAD_START)){net_log("MK64NET4: gameplay cancelled by BACK+START\n");failed=true;}
        if(failed||stream.fault||gameplay_timeout(now))break;
        if(menu_frame_window_ready()&&stream.consume(frame_pads)){
            unsigned wait_ms=(unsigned)(GetTickCount()-begin);++r55_wait_samples;r55_wait_sum+=wait_ms;if(wait_ms)++r55_wait_nonzero;if(wait_ms>r55_wait_max)r55_wait_max=wait_ms;
            if(r55_wait_samples>=120U){net_log("R56_WAIT: role=%s frame=%u delay=%u samples=%u blocked=%u avg_ms=%u max_ms=%u rx=%u rej=%u txf=%u meshTx=%u meshRx=%u probeTx=%u probeRx=%u\n",hosting?"HOST":"JOIN",(unsigned)stream.frame,chosen_delay,r55_wait_samples,r55_wait_nonzero,r55_wait_sum/r55_wait_samples,r55_wait_max,net_rx_input,net_rx_input_reject,net_tx_input_fail,mesh_direct_tx,mesh_direct_rx,mesh_probe_tx,mesh_probe_rx);r55_wait_samples=r55_wait_nonzero=r55_wait_sum=r55_wait_max=0;}
            if(stream.frame<=4)net_log("MK64NET4: frame consumed=%u players=%u RX=%u REJ=%u TX=%u TF=%u\n",(unsigned)stream.frame,player_count,net_rx_input,net_rx_input_reject,net_tx_input,net_tx_input_fail);
            memset(pads,0,sizeof(*pads)*count);
            for(unsigned i=0;i<player_count&&i<(unsigned)count;++i){pads[i].button=frame_pads[i].buttons;pads[i].stick_x=frame_pads[i].x;pads[i].stick_y=frame_pads[i].y;pads[i].err_no=0;}
            for(int i=(int)player_count;i<count;++i)pads[i].err_no=1;
            return;
        }
        if(now-begin>1000&&(!last_wait_screen||now-last_wait_screen>=250)){last_wait_screen=now;char diag[96],who[80];_snprintf(diag,sizeof(diag)-1,"%uP FRAME=%u RX=%u REJ=%u TXF=%u",player_count,(unsigned)stream.frame,net_rx_input,net_rx_input_reject,net_tx_input_fail);diag[sizeof(diag)-1]=0;if(hosting)_snprintf(who,sizeof(who)-1,"WAITING FOR REMOTE PLAYERS");else _snprintf(who,sizeof(who)-1,"P%u WAITING FOR HOST",local_slot+1);who[sizeof(who)-1]=0;screen("WAITING FOR PLAYERS","GAME PAUSED - RETRYING CONNECTION",who,diag,"BACK AND START EXIT GAME");}
        Sleep(1);
    }

    if(return_to_premenu){
        memset(pads,0,sizeof(*pads)*count);
        close_network();
        return;
    }

    const bool desync=stream.fault;if(crossplay)crossplay_fault_log();const unsigned stopped_players=player_count;const char *why=desync?"STATE/HASH MISMATCH":(failed?"PEER/CANCEL/NETWORK FAILURE":"GAMEPLAY INPUT TIMEOUT");
    net_log("MK64NET4: session stop reason=%s frame=%u players=%u RX=%u REJ=%u TX=%u TF=%u\n",why,(unsigned)stream.frame,stopped_players,net_rx_input,net_rx_input_reject,net_tx_input,net_tx_input_fail);
    close_network();char stats[96];_snprintf(stats,sizeof(stats)-1,"%uP RX=%u REJ=%u TX=%u F=%u",stopped_players,net_rx_input,net_rx_input_reject,net_tx_input,net_tx_input_fail);stats[sizeof(stats)-1]=0;
    for(;;){screen(desync?"GAME STATE MISMATCH":"CONNECTION LOST","SESSION STOPPED TO PREVENT DIVERGENCE",why,stats,"B EXIT GAME");if(pressed()&XINPUT_GAMEPAD_B)XLaunchNewImage(0,0);Sleep(16);}
}


static const char *race8Names[]={"MARIO","LUIGI","YOSHI","TOAD","DK","WARIO","PEACH","BOWSER"};
static const int race8Courses[]={8,9,6,11,10,5,1,0,14,12,7,2,18,4,3,13,15,16,17,19};
static const char *race8CourseNames[]={"LUIGI RACEWAY","MOO MOO FARM","KOOPA TROOPA BEACH","KALAMARI DESERT",
    "TOADS TURNPIKE","FRAPPE SNOWLAND","CHOCO MOUNTAIN","MARIO RACEWAY","WARIO STADIUM","SHERBET LAND",
    "ROYAL RACEWAY","BOWSERS CASTLE","DK JUNGLE PARKWAY","YOSHI VALLEY","BANSHEE BOARDWALK","RAINBOW ROAD",
    "BLOCK FORT","SKYSCRAPER","DOUBLE DECK","BIG DONUT"};
extern "C" int x360_net8_character(int slot){return raceLobby.character[slot&7];}
extern "C" int x360_net8_course(void){return race8Courses[raceLobby.course];}
extern "C" int x360_net8_cc(void){return raceLobby.cc;}

extern "C" void x360_net8_configure(void){
    input_test_active=true;input_test_hash=2166136261U;
    for(unsigned i=0;i<8;++i){raceLobby.ready[i]=0;raceLobby.previous[i]=0;}
    for(;;){
        NetPadCompat pads[8];unsigned short buttons[8];mknet::Pad normalized[8];
        const DWORD begin=GetTickCount();
        memset(pads,0,sizeof(pads));x360_read_controllers(pads,8);
        for(unsigned i=0;i<player_count;++i){buttons[i]=pads[i].button;normalized[i].buttons=buttons[i];normalized[i].x=pads[i].stick_x;normalized[i].y=pads[i].stick_y;}
        const int start=race8_lobby_step(&raceLobby,buttons);
        input_test_hash=mknet::diagnostic_step(input_test_hash,normalized,player_count);
        input_test_hash=(input_test_hash^raceLobby.course)*16777619U;
        input_test_hash=(input_test_hash^raceLobby.cc)*16777619U;
        for(unsigned i=0;i<player_count;++i){input_test_hash=(input_test_hash^raceLobby.character[i])*16777619U;input_test_hash=(input_test_hash^raceLobby.ready[i])*16777619U;}
        if(start)break;
        IDirect3DDevice9 *dev=x360_d3d_device();
        if(dev){
            const DWORD sw=(DWORD)x360_video_width(),sh=(DWORD)x360_video_height();
            D3DVIEWPORT9 full={0,0,sw,sh,0,1};dev->SetViewport(&full);
            RECT all={0,0,(LONG)sw,(LONG)sh};dev->SetScissorRect(&all);
            dev->Clear(0,0,D3DCLEAR_TARGET,0xFF102030,1,0);
            net_text(dev,60,36,"ONLINE GAME - CHOOSE YOUR RACER",4,0xFFFFD050);
            char line[120];_snprintf(line,sizeof(line),"%s - %s - %dCC",raceLobby.course<16?"VS":"BATTLE",race8CourseNames[raceLobby.course],50*(raceLobby.cc+1));
            net_text(dev,60,100,line,2,0xFF90D0FF);
            for(unsigned i=0;i<player_count;++i){
                _snprintf(line,sizeof(line),"P%u %s  %-6s  %s",i+1,x360_net_is_local(i)?"LOCAL ":"REMOTE",race8Names[raceLobby.character[i]],raceLobby.ready[i]?"READY":"CHOOSING");
                net_text(dev,60,158+i*45,line,3,x360_net_is_local(i)?0xFFFFD050:0xFFFFFFFF);
            }
            net_text(dev,60,546,"D-PAD LEFT/RIGHT: CHARACTER   A: READY   B: UNREADY",2,0xFFCCCCCC);
            net_text(dev,60,588,"HOST: UP/DOWN COURSE   R BUTTON: CC   START: RACE",2,0xFFCCCCCC);
            net_text(dev,60,630,"CONTROLS FOLLOW YOUR SAVED N64 BUTTON MAPPINGS",2,0xFFCCCCCC);
            dev->Present(0,0,0,0);
        }
        DWORD elapsed=GetTickCount()-begin;if(elapsed<33)Sleep(33-elapsed);
    }
    input_test_active=false;
}

extern "C" void x360_net8_draw_hud(void){
    if(!x360_net8_active())return;
    IDirect3DDevice9 *dev=x360_d3d_device();if(!dev)return;
    D3DVIEWPORT9 full={0,0,(DWORD)x360_video_width(),(DWORD)x360_video_height(),0,1};dev->SetViewport(&full);
    RECT all={0,0,(LONG)full.Width,(LONG)full.Height};dev->SetScissorRect(&all);
    for(int view=0;view<x360_net_local_count();++view) {
        const bool split=x360_net_local_count()==2;
        bool disconnected=false;
        if(split){
            XINPUT_STATE controller;memset(&controller,0,sizeof(controller));
            disconnected=XInputGetState(view,&controller)!=ERROR_SUCCESS;
        }
        for(int row=0;row<9;++row){
            char line[128];
            bool show=x360_race8_hud_line_for_view(view,row,line,sizeof(line))!=0;
            if(row==0 && disconnected){
                _snprintf(line,sizeof(line),"RECONNECT CONTROLLER %d",view+1);
                show=true;
            }
            if(show){
                int y=split ? view*360+12+row*27 : 35+row*45;
                net_text(dev,42,y,line,2,0xFF000000);
                net_text(dev,40,y-2,line,2,row==0?0xFFFFFF60:0xFFFFFFFF);
            }
        }
    }
}


/* MK64_CROSSPLAY_COMPONENT_DIAG_R15_NET implementation.
 * Writes one snapshot at the first crossplay fault. No packet/wire changes.
 */
static bool mkdiag_component_written = false;

static void mkdiag_component_append(const char *text) {
    if (!crossplay_diagnostics_enabled) return;
    static const char *paths[] = {
        "game:\\mk64-crossplay-components.log",
        "D:\\mk64-crossplay-components.log",
        "T:\\mk64-crossplay-components.log",
        "mk64-crossplay-components.log"
    };
    unsigned int i;
    if (!text || !text[0]) return;
    for (i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
        HANDLE f = CreateFileA(paths[i], GENERIC_WRITE, FILE_SHARE_READ,
                               NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wrote = 0;
            SetFilePointer(f, 0, NULL, FILE_END);
            WriteFile(f, text, (DWORD)strlen(text), &wrote, NULL);
            CloseHandle(f);
            return;
        }
    }
}

static void mkdiag_write_component_snapshot(void) {
    if (!crossplay_diagnostics_enabled) return;
    unsigned int d[40];
    char line[1024];
    if (mkdiag_component_written) return;
    mkdiag_component_written = true;
    memset(d, 0, sizeof(d));
    mk64_crossplay_component_diag(d, 40);

    _snprintf(line, sizeof(line)-1,
        "MKDIAG V1 MAGIC=%08X TIMER=%08X GS=%08X MODE=%08X SEED=%08X CORE=%08X FULL=%08X\r\n",
        d[0],d[1],d[2],d[3],d[4],d[5],d[28]);
    line[sizeof(line)-1]=0; mkdiag_component_append(line);

    _snprintf(line, sizeof(line)-1,
        "MKDIAG P1 META=%08X POSRAW=%08X VELRAW=%08X POSQ=%08X VELQ=%08X TYPE=%08X LAP=%08X EFFECTS=%08X\r\n",
        d[6],d[7],d[8],d[9],d[10],d[30],d[31],d[32]);
    line[sizeof(line)-1]=0; mkdiag_component_append(line);

    _snprintf(line, sizeof(line)-1,
        "MKDIAG P1BITS PX=%08X PY=%08X PZ=%08X VX=%08X VY=%08X VZ=%08X\r\n",
        d[16],d[17],d[18],d[19],d[20],d[21]);
    line[sizeof(line)-1]=0; mkdiag_component_append(line);

    _snprintf(line, sizeof(line)-1,
        "MKDIAG P2 META=%08X POSRAW=%08X VELRAW=%08X POSQ=%08X VELQ=%08X TYPE=%08X LAP=%08X EFFECTS=%08X\r\n",
        d[11],d[12],d[13],d[14],d[15],d[33],d[34],d[35]);
    line[sizeof(line)-1]=0; mkdiag_component_append(line);

    _snprintf(line, sizeof(line)-1,
        "MKDIAG P2BITS PX=%08X PY=%08X PZ=%08X VX=%08X VY=%08X VZ=%08X\r\n",
        d[22],d[23],d[24],d[25],d[26],d[27]);
    line[sizeof(line)-1]=0; mkdiag_component_append(line);
}
