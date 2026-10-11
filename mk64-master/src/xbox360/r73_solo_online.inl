/* R73 SOLO ONLINE: authenticated self-reported results, independent of netplay lockstep. */
static bool r73_solo_active=false;
static char r73_solo_sid[40]={0};
static unsigned r73_solo_seq=0,r73_solo_pending_seq=0,r73_solo_pending_value=0;
static int r73_solo_pending_course=0;
static char r73_solo_pending_mode[4]={0};
static DWORD r73_solo_ping_at=0,r73_solo_retry_at=0;
static unsigned r73_solo_counter=0;
extern "C" int x360_net_solo_active(void){return r73_solo_active?1:0;}
extern "C" void x360_net_solo_finish(int mode,int course,int value){
    if(!r73_solo_active||r73_solo_pending_seq||course<0||course>18)return;
    if(mode==1){if(value<10000||value>3599999)return;strcpy(r73_solo_pending_mode,"TT");}
    else if(mode==2){if(value<0||value>7)return;strcpy(r73_solo_pending_mode,"GP");}
    else return;
    r73_solo_pending_course=course;r73_solo_pending_value=(unsigned)value;
    r73_solo_pending_seq=++r73_solo_seq;r73_solo_retry_at=0;
}
static void r73_solo_exit(void){
    if(!r73_solo_active)return;
    R48Account360 *me=r48_me();if(me&&r48_dir_sock!=INVALID_SOCKET){char m[160];
        _snprintf(m,sizeof(m)-1,"MKDIR2|SOLO_EXIT|%s|%s|%s",me->id,me->secret,r73_solo_sid);
        m[sizeof(m)-1]=0;r48_send(m);
    }
    r73_solo_active=false;r73_solo_sid[0]=0;r73_solo_pending_seq=0;
}
static bool r73_solo_begin(void){
    if(active||r73_solo_active||!r48_me()||!r48_dir_open())return false;
    R48Account360 *me=r48_me();char m[220],response[256];
    unsigned gpRaces=0,gpWins=0,ttTracks=0;
    _snprintf(m,sizeof(m)-1,"MKDIR2|SOLO_SUMMARY|%s|%s",me->id,me->secret);m[sizeof(m)-1]=0;
    response[0]=0;
    if(r48_wait_prefix(m,"MKDIR2|SOLO_SUMMARY|",response,sizeof(response),1800U))
        sscanf(response,"MKDIR2|SOLO_SUMMARY|%u|%u|%u",&gpRaces,&gpWins,&ttTracks);
    r47_consume_buttons();
    for(;;){char gp[80],tt[80];
        _snprintf(gp,sizeof(gp)-1,"SOLO GP: %u RACES / %u WINS",gpRaces,gpWins);
        _snprintf(tt,sizeof(tt)-1,"TIME TRIAL TRACK BESTS: %u / 16",ttTracks);
        screen("SOLO ONLINE",gp,tt,"TT TIMES: SELF-REPORTED",
               "NO SECOND CONSOLE REQUIRED","A START  B BACK");
        DWORD q=pressed(false);if(q&XINPUT_GAMEPAD_B){r47_consume_buttons();return false;}
        if(q&XINPUT_GAMEPAD_A){r47_consume_buttons();break;}Sleep(16);
    }
    ++r73_solo_counter;
    _snprintf(m,sizeof(m)-1,"MKDIR2|SOLO_OPEN|%s|%s|X%08lX%04X|X360|%s",
        me->id,me->secret,(unsigned long)GetTickCount(),r73_solo_counter&0xffffU,r484_region);
    m[sizeof(m)-1]=0;response[0]=0;
    if(!r48_wait_prefix(m,"MKDIR2|SOLO_OPEN_OK|",response,sizeof(response),4500U)||
       sscanf(response,"MKDIR2|SOLO_OPEN_OK|%39s",r73_solo_sid)!=1){
        screen("SOLO ONLINE UNAVAILABLE","HUB NEEDS R2.19 SOLO SUPPORT",
               "NO RESULTS WILL BE RECORDED","","A / B BACK");
        r49_wait_a_or_b();return false;
    }
    r73_solo_active=true;r73_solo_seq=0;r73_solo_pending_seq=0;
    r73_solo_ping_at=r73_solo_retry_at=0;
    r57_presence_active=false;r59_world_ui_active=false;
    screen("SOLO CONNECTED","PLAY GRAND PRIX OR TIME TRIALS",
           "SOLO GP + TT BESTS TRACKED","TT RECORDS: SELF-REPORTED",
           "STARTING MARIO KART 64");
    Sleep(350);r47_consume_buttons();return true;
}
extern "C" void x360_net_solo_tick(void){
    if(!r73_solo_active)return;
    R48Account360 *me=r48_me();if(!me||r48_dir_sock==INVALID_SOCKET)return;
    DWORD now=GetTickCount();
    if(!r73_solo_ping_at||now-r73_solo_ping_at>=4000U){
        char m[220];_snprintf(m,sizeof(m)-1,"MKDIR2|SOLO_PING|%s|%s|%s|X360|%s",
            me->id,me->secret,r73_solo_sid,r484_region);m[sizeof(m)-1]=0;
        r48_send(m);r73_solo_ping_at=now;
    }
    if(r73_solo_pending_seq&&(!r73_solo_retry_at||now-r73_solo_retry_at>=900U)){
        char m[240];_snprintf(m,sizeof(m)-1,"MKDIR2|SOLO_RESULT|%s|%s|%s|%u|%s|%d|%u",
            me->id,me->secret,r73_solo_sid,r73_solo_pending_seq,r73_solo_pending_mode,
            r73_solo_pending_course,r73_solo_pending_value);m[sizeof(m)-1]=0;
        r48_send(m);r73_solo_retry_at=now;
    }
    for(int i=0;i<12;++i){char b[512];sockaddr_in from;int flen=sizeof(from);
        int n=recvfrom(r48_dir_sock,b,sizeof(b)-1,0,(sockaddr*)&from,&flen);
        if(n<=0)break;b[n]=0;
        char sid[40]={0};unsigned seq=0;
        if(sscanf(b,"MKDIR2|SOLO_RESULT_OK|%39[^|]|%u",sid,&seq)==2){
            if(!strcmp(sid,r73_solo_sid)&&seq==r73_solo_pending_seq)
                r73_solo_pending_seq=0;
        }else if(!strncmp(b,"MKDIR2|STATUS|",14))r57_parse_status(b);
        else if(r59_world_packet(b)||r61_social_packet(b)){}
    }
}
