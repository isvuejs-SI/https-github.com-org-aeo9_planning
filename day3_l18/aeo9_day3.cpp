// AEO-9 Day 3 supervisor replay, C++ twin of aeo9_day3.py. Illustrative placeholders only.
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;
const long CAP=4000,RESERVE=800,FLOOR=600,CRUISE=100,FILM=70,ESC=140,IDLE=2,SLEEP=1,UPL=4;
const long SUNRISE=390,SUNSET=1080,NOON=735,PEAK=10,WIND_MAX=110,GUST_ABORT=140,OPEN=480,LAST=840;
const long FILM_MIN=15,MAX_ESC=3,CAM_FAILS=3,UP_NEED=60,UP_DEADLINE=1200,ALT=75,MIN_AGL=40;
const long D4_NEED=13*CRUISE+8*FILM+3*ESC;
struct LCG{ unsigned long long x; long next(long m){ x=(x*1103515245ULL+12345ULL)%2147483648ULL; return (long)((x>>8)%(unsigned long long)m);} };
long solar(long t,long cloud){ t%=1440; if(t<=SUNRISE||t>=SUNSET) return 0; return PEAK*(345-labs(t-NOON))*cloud/(345*100); }
long terrain(long x){ return (x%800)? 30-labs((x%800)-400)/10 : 30; }
struct Sc{ const char* name; long soc,wind,gust_at,det,os,ol,cam,cpm,camfail,behind; };
string F(const char* f,...){ char b[256]; va_list a; va_start(a,f); vsnprintf(b,256,f,a); va_end(a); return b; }
string hm(long t){ return F("D%ld %02ld:%02ld",3+t/1440,(t%1440)/60,t%60); }
bool run(int i,const Sc& c,vector<string>& out,FILE* csv){
  long soc=c.soc; LCG r{(unsigned long long)(2026+i)};
  long need=5*CRUISE+FILM_MIN*FILM+2*ESC+5*CRUISE;
  string st="NIGHT"; long t0=0,film=0,esc=0,dets=0,fails=0,trig=0,x=0,up=0,minsoc=soc,minagl=999,d4go=-1,soc8=0;
  vector<string> ev; out.push_back(F("# %s dawn_soc=%ld need=%ld+%ld",c.name,soc,need,RESERVE));
  for(long t=0;t<1440+LAST+1;t++){
    long cloud=((t%1440)<840||t>=1440)?c.cam:c.cpm;
    long draw=0, gain=(st=="NIGHT"||st=="WAIT"||st=="DEPLOYED"||st=="UPLOAD"||st=="CHARGE"||st=="DONE")?solar(t,cloud):0;
    if(st=="NIGHT"){ draw=SLEEP; if(t>=OPEN&&t<1440) st="WAIT"; }
    else if(st=="WAIT"){ draw=IDLE;
      if(c.behind){ ev.push_back(hm(t)+" SKIP day3 (behind schedule)"); st="CHARGE"; }
      else if(c.wind>WIND_MAX){}
      else if(soc>=need+RESERVE){ ev.push_back(F("%s GO soc=%ld",hm(t).c_str(),soc)); st="STOW"; t0=t; }
      else if(t>=LAST){ ev.push_back(hm(t)+" NO-GO energy"); st="CHARGE"; } }
    else if(st=="STOW"){ draw=IDLE; if(t-t0>=3){ st="CLIMB"; t0=t; } }
    else if(st=="CLIMB"){ draw=CRUISE; if(t-t0>=5){ st="FILM"; t0=t; } }
    else if(st=="FILM"){ draw=FILM; film++; x+=120;
      bool ok=!(c.camfail>=0&&film>=c.camfail); trig++; fails=ok?0:fails+1;
      long g=c.wind+((c.gust_at>=0&&film==c.gust_at)?90:r.next(20));
      long agl=ALT-terrain(x);
      if(r.next(100)<c.det){ dets++; if(r.next(100)<50){ esc++; draw=ESC; agl+=35; ev.push_back(F("%s ESCAPE #%ld climb +35 m",hm(t).c_str(),esc)); } }
      minagl=min(minagl,agl); string reason;
      if(g>GUST_ABORT) reason=F("gust %ld dm/s",g);
      else if(esc>=MAX_ESC) reason="escape limit";
      else if(fails>=CAM_FAILS) reason=F("camera trigger fail x%ld",fails);
      else if(soc<FLOOR+5*CRUISE) reason="energy floor";
      else if(film>=FILM_MIN) reason="film done";
      if(!reason.empty()){ ev.push_back(F("%s RTL camp2 (%s) film=%ld min",hm(t).c_str(),reason.c_str(),film)); st="RETURN"; t0=t; } }
    else if(st=="RETURN"){ draw=CRUISE; if(t-t0>=5){ st="DEPLOY"; t0=t; ev.push_back(hm(t)+" LANDED camp2"); } }
    else if(st=="DEPLOY"){ draw=IDLE; if(t-t0>=3){ st="UPLOAD"; t0=t; } }
    else if(st=="UPLOAD"){ bool link=!(c.os>=0&&t-t0<c.ol); draw=IDLE+(link?UPL:0); up+=link?1:0;
      if(up>=UP_NEED){ ev.push_back(hm(t)+" UPLOAD done"); st="CHARGE"; }
      else if(t%1440>=UP_DEADLINE){ ev.push_back(F("%s UPLOAD deferred to D4 (%ld/%ld min)",hm(t).c_str(),up,UP_NEED)); st="CHARGE"; } }
    else if(st=="CHARGE"){ draw=IDLE; if(t%1440>=SUNSET) st="DONE"; }
    else if(st=="DONE"){ draw=SLEEP; }
    soc=max(0L,min(CAP,soc-draw+gain)); minsoc=min(minsoc,soc);
    fprintf(csv,"%s,%ld,%s,%ld,%ld,%ld\n",c.name,t,st.c_str(),draw,gain,soc);
    if(t==1439) ev.push_back(F("D3 23:59 soc=%ld",soc));
    if(t==1440+OPEN) soc8=soc;
    if(t>=1440+OPEN&&d4go<0&&soc>=D4_NEED+RESERVE) d4go=t;
  }
  bool ready=d4go>=0;
  for(auto& e:ev) out.push_back("  "+e);
  out.push_back(F("  dets=%ld esc=%ld triggers=%ld min_soc=%ld min_agl=%s D4_08:00 soc=%ld need=%ld -> %s",dets,esc,trig,minsoc,
    minagl<999?to_string(minagl).c_str():"-",soc8,D4_NEED+RESERVE,ready?("D4_GO "+hm(d4go)).c_str():"D4_HOLD_CHARGE"));
  bool exp=string(c.name)!="cloudy_afternoon";
  bool pass=ready==exp&&minsoc>=FLOOR&&(minagl==999||minagl>=MIN_AGL);
  out.push_back(pass?"  PASS":"  FAIL"); return pass;
}
int main(int argc,char**argv){
  vector<Sc> S={{"nominal",3200,60,-1,6,-1,0,100,100,-1,0},{"gust_filming",3200,60,12,6,-1,0,100,100,-1,0},
    {"repeat_escapes",3200,60,-1,40,-1,0,100,100,-1,0},{"starlink_outage",3200,60,-1,6,0,150,100,100,-1,0},
    {"cloudy_afternoon",3200,60,-1,6,-1,0,100,25,-1,0},{"camera_trigger_fail",3200,60,-1,6,-1,0,100,100,7,0},
    {"behind_schedule",3200,60,-1,6,-1,0,100,100,-1,1}};
  FILE* csv=fopen(argc>1?argv[1]:"budget_cpp.csv","w"); fprintf(csv,"scenario,minute,state,draw_dwh,solar_dwh,soc_dwh\n");
  vector<string> out; int ok=0; for(size_t i=0;i<S.size();i++) ok+=run((int)i,S[i],out,csv);
  out.push_back(F("TOTAL pass=%d/%zu",ok,S.size())); fclose(csv);
  for(auto& l:out) printf("%s\n",l.c_str());
}
