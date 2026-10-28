// AEO-9 five-day mission supervisor test (integer math, seeded LCG).
// Twin of aeo9_mission.py; output must match byte for byte.
// ALL NUMBERS ARE ILLUSTRATIVE PLACEHOLDERS, not real specs.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace std;
const long CAP=4000, RESERVE=800, FLOOR_=600, CRUISE_W=100, FILM_W=90, ESC_W=140;
const long IDLE_W=1, UPLINK_W=4, SOLAR_PEAK=10, SUNRISE=390, SUNSET=1080, NOON=735;
const long FLY_OPEN=480, FLY_CLOSE=1020, LAST_LAUNCH=840, SPEED=600, WIND_MAX=11;
const long FENCE_SOFT=1200, FENCE_HARD=1500, ALT=110;
const long CAMPS[3]={0,6000,12000};
struct Leg{int a,b; long fm; int opt;};
const Leg LEGS[5]={{0,1,12,0},{1,2,12,0},{2,2,20,1},{2,1,8,0},{1,0,5,0}};
const int NLEG=5;
const char* SCEN[5]={"nominal","cloudy_day1","wind_day2","animal_heavy","fence_drift"};
struct Rng{long s; long next(){ s=(s*1103515245L+12345L)&0x7FFFFFFFL; return s>>8; }};
long terrain(long x){
  if(x<5000) return 0;
  if(x<9000) return (x-5000)*60/4000;
  if(x<11000) return 60-(x-9000)*60/2000;
  return 0;
}
long solar(long t,long cloud){
  if(t<=SUNRISE||t>=SUNSET) return 0;
  return SOLAR_PEAK*(345-labs(t-NOON))*cloud/(345*100);
}
string hhmm(long t){ char b[32]; snprintf(b,32,"%02ld:%02ld",t/60,t%60); return b; }
bool run(int sid, vector<string>& out){
  string name=SCEN[sid]; Rng rng{2026L+sid}; char buf[256];
  long soc=CAP; int leg=0; long night_fly=0, deploy_air=0, min_soc=CAP, fence_hits=0, max_y=0;
  long min_agl=9999, max_agl=0;
  for(int day=1; day<=5; ++day){
    long cloud=100-rng.next()%15;
    long wind=4+rng.next()%5;
    if(name=="cloudy_day1"&&day==1) cloud=35;
    if(name=="wind_day2"&&day==2) wind=14;
    long p_animal = name=="animal_heavy"?4:20;
    int mand_left=0; for(int k=leg;k<NLEG;++k) if(!LEGS[k].opt) ++mand_left;
    while(leg<NLEG && LEGS[leg].opt && mand_left>=6-day){
      snprintf(buf,256,"  day %d drop optional leg %d (behind schedule)",day,leg); out.push_back(buf); ++leg; }
    string state="GROUND"; long launch=-1, land=-1, used=0, gain=0, animals=0, escapes=0;
    long x=0,y=0,film_left=0,tgt=0,esc_left=0,soc_launch=0,stow=0; string reason;
    for(long t=0;t<1440;++t){
      long g = state=="GROUND"? solar(t,cloud):0;
      gain+=g; soc=min(CAP,soc+g);
      if(state=="GROUND"){
        if((SUNRISE<=t&&t<SUNSET)||t%4==0) soc-=IDLE_W;
        if(land>=0&&t<land+60) soc-=UPLINK_W;
        if(launch<0&&leg<NLEG&&FLY_OPEN<=t&&t<=LAST_LAUNCH){
          const Leg& L=LEGS[leg];
          long need=(labs(CAMPS[L.b]-CAMPS[L.a])/SPEED+1)*CRUISE_W+L.fm*FILM_W+3*ESC_W;
          if(wind>WIND_MAX){ snprintf(buf,256,"wind %ld m/s",wind); reason=buf; }
          else if(soc<need+RESERVE) reason="charging";
          else { state="STOW"; stow=3; launch=t; reason=""; soc_launch=soc;
                 x=CAMPS[L.a]; y=0; tgt=CAMPS[L.b]; film_left=L.fm; }
        }
        continue;
      }
      if(state=="STOW"){ --stow; soc-=IDLE_W; if(stow==0) state = x!=tgt?"TRANSIT":"FILM"; continue; }
      if(t<FLY_OPEN||t>FLY_CLOSE) ++night_fly;
      if(state=="TRANSIT"){
        soc-=CRUISE_W; used+=CRUISE_W;
        long step=min(SPEED,labs(tgt-x)); x+= tgt>x?step:-step;
        if(name=="fence_drift"&&leg==3) y+=200+rng.next()%200;
        if(labs(y)>FENCE_SOFT) y-= y>0?400:-400;
        if(labs(y)>FENCE_HARD) ++fence_hits;
        max_y=max(max_y,labs(y));
        if(x==tgt) state="FILM";
      } else if(state=="FILM"){
        if(esc_left>0){ soc-=ESC_W; used+=ESC_W; --esc_left; }
        else { soc-=FILM_W; used+=FILM_W; --film_left;
          if(rng.next()%p_animal==0){ ++animals; if(rng.next()%3==0){ ++escapes; esc_left=2; } } }
        long dist_home=labs(tgt-x);
        if(film_left<=0||soc<FLOOR_+RESERVE/2+dist_home/SPEED*CRUISE_W) state="LAND";
      }
      long agl=ALT-terrain(x); min_agl=min(min_agl,agl); max_agl=max(max_agl,agl);
      if(state=="LAND"){ land=t; state="GROUND"; ++leg; }
      min_soc=min(min_soc,soc);
    }
    if(launch<0){
      snprintf(buf,256,"  day %d SCRUB (%s) soc_end=%ld.%ld%% solar=%ldWh",day,
        reason.empty()?"no leg":reason.c_str(),soc*100/CAP,soc*1000/CAP%10,gain/10);
    } else {
      snprintf(buf,256,"  day %d leg %d launch %s land %s used=%ldWh solar=%ldWh soc launch=%ld%% day_end=%ld%% animals=%ld escapes=%ld",
        day,leg-1,hhmm(launch).c_str(),hhmm(land).c_str(),used/10,gain/10,soc_launch*100/CAP,soc*100/CAP,animals,escapes);
    }
    out.push_back(buf);
  }
  bool home = leg==NLEG;
  bool ok = home&&night_fly==0&&deploy_air==0&&min_soc>=FLOOR_&&fence_hits==0&&min_agl>=40;
  snprintf(buf,256,"  RESULT %s home=%d night_fly=%ld min_soc=%ld%% fence=%ld max_y=%ldm agl=%ld..%ldm %s",
    name.c_str(),home?1:0,night_fly,min_soc*100/CAP,fence_hits,max_y,min_agl,max_agl,ok?"PASS":"FAIL");
  out.push_back(buf); return ok;
}
int main(){
  printf("AEO-9 5-day mission sim (illustrative numbers, seed 2026+id)\n");
  int n=0;
  for(int i=0;i<5;++i){ printf("scenario %d %s\n",i,SCEN[i]); vector<string> o; bool ok=run(i,o);
    for(auto& s:o) printf("%s\n",s.c_str());
    n+=ok; }
  printf("TOTAL pass=%d/5\n",n);
}
