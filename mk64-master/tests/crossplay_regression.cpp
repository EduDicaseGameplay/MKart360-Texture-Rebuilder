#include "xbox360/netplay_protocol.h"
#include "xbox360/netplay_state_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
using namespace mknet;
struct Datagram { unsigned at; std::vector<uint8_t> bytes; };
static unsigned rng=1;
static unsigned random_value(){rng=rng*1664525U+1013904223U;return rng;}
static unsigned attempts,recovered;
static void send(std::vector<Datagram>& q,const uint8_t *p,int n,unsigned now,bool lossy){
    ++attempts;
    if(lossy && (random_value()%100 < 20 || (now>=100 && now<260)))return;
    Datagram d;d.at=now+(lossy?random_value()%21:0);d.bytes.assign(p,p+n);q.push_back(d);
}
static unsigned state_hash(unsigned f){return 0x7391AB01U ^ (f*193U);}
static Pad input(unsigned f,unsigned s){Pad p={uint16_t(f*17+s),int8_t(f+s),int8_t(2*f+s)};return p;}
static void loss_simulation(unsigned delay,unsigned players,bool split,bool lossy){
    const unsigned stop=160, release=80;
    static Stream4 host,guest[3];
    static CrossStateHistory states,received[3];
    host.reset(delay,players,0,1);states.reset();
    unsigned guests=split?1:players-1;
    for(unsigned i=0;i<guests;++i){guest[i].reset(delay,players,i+1,split?2:1);received[i].reset();}
    std::vector<Datagram> inbound,outbound[3];
    bool sampledHost=false,sampledGuest[3]={false,false,false};
    uint8_t session[16]={0},p[MAX_PACKET],snapshot[CROSS_STATE_BYTES];
    unsigned hostConsumed=0,guestConsumed[3]={0,0,0};
    for(unsigned time=0;time<20000;++time){
        for(unsigned i=0;i<inbound.size();){
            if(inbound[i].at>time){++i;continue;}
            std::vector<uint8_t> b=inbound[i].bytes;
            CHECK(host.receive_remote(b[HEADER],&b[0],int(b.size()),split?2:1));
            inbound.erase(inbound.begin()+i);
        }
        for(unsigned g=0;g<guests;++g)for(unsigned i=0;i<outbound[g].size();){
            if(outbound[g][i].at>time){++i;continue;}
            std::vector<uint8_t> b=outbound[g][i].bytes;
            if(b[5]==STATE_SYNC){
                CHECK(valid(&b[0],int(b.size())));
                unsigned f=get32(&b[HEADER]);
                if(f>=guest[g].frame && f<guest[g].frame+32){
                    received[g].save(f,&b[HEADER+4]);
                    if(f<host.frame)++recovered;
                }
            }else CHECK(guest[g].receive_frameset(&b[0],int(b.size())));
            outbound[g].erase(outbound[g].begin()+i);
        }
        if(!sampledHost && host.frame<stop){
            if(host.frame<=release){memset(snapshot,int(host.frame),sizeof(snapshot));states.save(host.frame,snapshot);}
            Pad pad=input(host.frame,0);host.sample_local(pad,state_hash(host.frame));sampledHost=true;
        }
        // A missing STATE_SYNC blocks guest sampling, exactly as on OG Xbox.
        for(unsigned g=0;g<guests;++g)if(!sampledGuest[g] && guest[g].frame<stop){
            unsigned f=guest[g].frame;
            if(f<=release && !received[g].find(f))continue;
            if(f<=release)for(unsigned j=0;j<CROSS_STATE_BYTES;++j)CHECK(received[g].find(f)[j]==uint8_t(f));
            Pad pads[2]={input(f,g+1),input(f,g+2)};
            guest[g].sample_locals(pads,state_hash(f));sampledGuest[g]=true;
        }
        for(unsigned g=0;g<guests;++g){
            // Recovery must remain active after host menu_sync goes false.
            uint32_t frames[3];unsigned n=states.replay_frames(host.peer_frame[g+1],frames,g+1);
            for(unsigned j=0;j<n;++j){
                int size=header(p,STATE_SYNC,session,4+CROSS_STATE_BYTES);put32(p+HEADER,frames[j]);
                memcpy(p+HEADER+4,states.find(frames[j]),CROSS_STATE_BYTES);send(outbound[g],p,size,time,lossy);
            }
            if(sampledHost){int n=host.frameset_packet(p,session,g+1);CHECK(n);send(outbound[g],p,n,time,lossy);}
            if(sampledGuest[g]){int n=guest[g].client_packet(p,session);CHECK(n);send(inbound,p,n,time,lossy);}
        }
        Pad out[MAX_PLAYERS];
        if(sampledHost && host.frame<stop && host.consume(out)){
            for(unsigned s=0;s<players;++s)CHECK(equal(out[s],host.frame-1<delay?Pad():input(host.frame-1-delay,s)));
            sampledHost=host.frame==stop; ++hostConsumed;
        }
        bool done=hostConsumed==stop;
        for(unsigned g=0;g<guests;++g){
            if(sampledGuest[g] && guest[g].frame<stop && guest[g].consume(out)){
                for(unsigned s=0;s<players;++s)CHECK(equal(out[s],guest[g].frame-1<delay?Pad():input(guest[g].frame-1-delay,s)));
                sampledGuest[g]=guest[g].frame==stop; ++guestConsumed[g];
            }
            CHECK(!guest[g].fault);done=done && guestConsumed[g]==stop;
        }
        CHECK(!host.fault);
        if(done)return;
    }
    fprintf(stderr,"stalled delay=%u players=%u host=%u guest=%u split=%u loss=%u latest=%u complete=%u\n",delay,players,host.frame,guest[0].frame,split,lossy,states.latest,host.latest_complete);
    for(unsigned g=0;g<guests;++g)fprintf(stderr,"g%u frame=%u ack=%u state=%u sampled=%u\n",g,guest[g].frame,host.peer_frame[g+1],received[g].find(guest[g].frame)!=0,sampledGuest[g]);
    exit(1);
}
int main(){
    static CrossStateHistory h;h.reset();uint8_t a[CROSS_STATE_BYTES],b[CROSS_STATE_BYTES];
    memset(a,0x12,sizeof(a));memset(b,0x34,sizeof(b));h.save(0,a);h.save(0,b);CHECK(h.find(0)[0]==0x12);
    for(unsigned i=1;i<1000;++i)h.save(i,a);
    CHECK(!h.find(0));CHECK(!h.find(967));CHECK(h.find(968));CHECK(h.find(999));
    uint32_t frames[3];CHECK(h.replay_frames(1000,frames)==0);CHECK(h.replay_frames(998,frames)==2);
    h.reset();CHECK(!h.present && !h.find(999));
    for(unsigned d=2;d<=12;++d)for(unsigned players=2;players<=4;++players)for(unsigned loss=0;loss<2;++loss)
        loss_simulation(d,players,false,loss!=0);
    loss_simulation(2,3,true,true);loss_simulation(12,3,true,true);
    CHECK(recovered>0);
    printf("PASS: 68 transport/snapshot scenarios, loss, jitter, reordering, burst outage, GO recovery, split input. attempts=%u recovered=%u\n",attempts,recovered);
}
