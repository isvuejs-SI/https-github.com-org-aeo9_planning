// C++ twin of failsafe_sim.py: integer math only, same LCG, same output.
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
using namespace std;
static const char* NAMES[] = {"CRUISE","ESCAPE_CLIMB","RTL_CLIMB","RTL_CRUISE","RTL_DESCEND","LAND","LANDED","PANELS","DONE"};
enum { CRUISE, ESCAPE, RTL_CLIMB, RTL_CRUISE, RTL_DESC, LAND, LANDED, PANELS, DONE };
static const int FENCE=120000, WP=100000, CRUISE_ALT=30000, ESC_CLIMB=15000;
static const int LOWB=2500, CRIT=1500, GPS_MS=2000, LINK_MS=3000, STALE_MS=200;
static const int STALE_RTL_MS=4000, CONFIRM=3, CONF_MIN=600, MISSION_END=3000;
static const int COVER_TICKS=150, PANEL_TICKS=250, LAND_ALT=300, GEAR_ALT=5000, MAX_TICKS=8000;
struct Rng { int64_t s; int64_t next(){ s=(s*1103515245LL+12345LL)&0x7FFFFFFFLL; return s>>8; } };
static const char* SCEN[] = {"nominal","animal","false_positive","geofence","low_battery",
                             "gps_loss","link_loss","dropout_short","dropout_long","wind_gust"};
static const int NS = 10;
static bool run(int sid){
  string name = SCEN[sid];
  Rng rng{2026 + sid};
  int st=CRUISE, d=0, alt=CRUISE_ALT;
  int bat0 = name=="low_battery" ? 3200 : 9000;
  int rate = name=="low_battery" ? 20 : 2;
  int last_frame=0, cnt=0, gps_bad=0, link_bad=0, esc_target=0, rtl_alt=0, timer=0;
  vector<string> log; string first_reason; int first_tick=-1;
  int max_d=0, max_alt=alt, det_hold=0, viol=0;
  int gear_tick=-1, cover_tick=-1, panel_tick=-1, done_tick=-1;
  set<int> visited; visited.insert(CRUISE);
  char buf[160];
  auto go=[&](int i,int nw,const string& why){
    snprintf(buf,sizeof buf,"  t=%05d %s->%s %s",i,NAMES[st],NAMES[nw],why.c_str());
    log.push_back(buf);
    if(first_tick<0 && why!="" && why!="START"){ first_reason=why; first_tick=i; }
    st=nw; visited.insert(nw);
  };
  for(int i=0;i<MAX_TICKS;i++){
    bool gps_ok = !(name=="gps_loss" && i>=2000);
    bool link_ok = !(name=="link_loss" && i>=2000 && i<2500);
    bool animal = (name=="animal" && i>=1000 && i<1500) || (name=="dropout_short" && i>=1550 && i<2000);
    bool drop = (name=="dropout_short" && i>=1500 && i<1625) || (name=="dropout_long" && i>=1500 && i<1800);
    if(i%5==0 && !drop){
      int64_t r=rng.next();
      int conf = animal ? (int)(700 + r%250) : (int)(r%500);
      if(name=="false_positive" && i==750) conf=650;
      cnt = conf>=CONF_MIN ? cnt+1 : 0;
      last_frame=i;
    }
    int g=0;
    if(name=="wind_gust" && i>=1000 && i<2000) g=(int)(rng.next()%201)-100;
    int bat = bat0 - i*rate/50;
    int stale=(i-last_frame)*20;
    gps_bad = gps_ok?0:gps_bad+20;
    link_bad = link_ok?0:link_bad+20;
    if(st==CRUISE||st==ESCAPE||st==RTL_CLIMB||st==RTL_CRUISE){
      if(gps_bad>=GPS_MS) go(i,LAND,"GPS_LOSS");
      else if(bat<=CRIT) go(i,LAND,"BATT_CRIT");
      else if(st==CRUISE||st==ESCAPE){
        string why;
        if(d>FENCE) why="GEOFENCE";
        else if(link_bad>=LINK_MS) why="LINK_LOSS";
        else if(bat<=LOWB) why="BATT_LOW";
        else if(stale>=STALE_RTL_MS) why="DET_STALE";
        else if(i>=MISSION_END) why="MISSION_END";
        if(!why.empty()){ rtl_alt=max(alt,CRUISE_ALT); go(i,RTL_CLIMB,why); }
        else if(st==CRUISE){
          if(stale>STALE_MS) det_hold++;
          else if(cnt>=CONFIRM){ esc_target=alt+ESC_CLIMB; go(i,ESCAPE,"ANIMAL"); }
        }
      }
    }
    if(st==CRUISE||st==ESCAPE||st==RTL_CLIMB||st==RTL_CRUISE||st==RTL_DESC||st==LAND) d+=g;
    if(st==CRUISE){
      if(d<WP) d+=80;
      if(name=="geofence" && i>=1500) d+=120;
    } else if(st==ESCAPE){
      alt+=40;
      if(alt>=esc_target){ rtl_alt=alt; go(i,RTL_CLIMB,"ESCAPE_DONE"); }
    } else if(st==RTL_CLIMB){
      if(alt<rtl_alt) alt+=40; else go(i,RTL_CRUISE,"");
    } else if(st==RTL_CRUISE){
      d-=100;
      if(d<=0){ d=0; go(i,RTL_DESC,""); }
    } else if(st==RTL_DESC||st==LAND){
      alt-=30;
      if(alt<=GEAR_ALT && gear_tick<0) gear_tick=i;
      if(alt<=LAND_ALT){
        if(st==LAND && alt>LAND_ALT) viol++;
        alt=0; timer=0; go(i,LANDED,""); cover_tick=i;
      }
    } else if(st==LANDED){
      if(++timer>=COVER_TICKS){ timer=0; go(i,PANELS,""); panel_tick=i; }
    } else if(st==PANELS){
      if(++timer>=PANEL_TICKS){ go(i,DONE,""); done_tick=i; }
    }
    if(d<0) d=0;
    max_d=max(max_d,d); max_alt=max(max_alt,alt);
    if(st==DONE) break;
  }
  bool ok = viol==0 && done_tick>0;
  bool esc = visited.count(ESCAPE), rtl = visited.count(RTL_CLIMB);
  if(name=="nominal") ok = ok && first_reason=="MISSION_END" && first_tick==3000;
  if(name=="animal") ok = ok && first_reason=="ANIMAL" && first_tick>=1000 && first_tick<=1030 && max_alt>=45000;
  if(name=="false_positive") ok = ok && !esc && first_reason=="MISSION_END";
  if(name=="geofence") ok = ok && first_reason=="GEOFENCE" && max_d<=121000;
  if(name=="low_battery") ok = ok && first_reason=="BATT_LOW" && first_tick<=1760;
  if(name=="gps_loss") ok = ok && first_reason=="GPS_LOSS" && first_tick>=2090 && first_tick<=2110 && !rtl;
  if(name=="link_loss") ok = ok && first_reason=="LINK_LOSS" && first_tick>=2140 && first_tick<=2160;
  if(name=="dropout_short") ok = ok && first_reason=="ANIMAL" && first_tick>=1625 && first_tick<=1640 && det_hold>0;
  if(name=="dropout_long") ok = ok && first_reason=="DET_STALE" && first_tick>=1690 && first_tick<=1700 && !esc;
  if(name=="wind_gust") ok = ok && first_reason=="MISSION_END" && max_d<FENCE-10000;
  printf("scenario %d %s\n",sid,name.c_str());
  for(auto& l:log) printf("%s\n",l.c_str());
  printf("  first=%s@%d max_d=%d max_alt=%d det_hold=%d gear@%d landed@%d panels@%d done@%d viol=%d\n",
         first_reason.c_str(),first_tick,max_d,max_alt,det_hold,gear_tick,cover_tick,panel_tick,done_tick,viol);
  printf("  %s\n",ok?"PASS":"FAIL");
  return ok;
}
int main(){
  int n=0;
  for(int s=0;s<NS;s++) if(run(s)) n++;
  printf("TOTAL pass=%d/%d\n",n,NS);
  return 0;
}
