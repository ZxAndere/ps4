#include "engine/runtime.hpp"
#include "overlay/overlay.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <stdexcept>
namespace fg {
Config AdaptiveController::select(const Config& requested,Size input) const {
    Config out=requested;
    if(out.safe_mode){out.enabled=false;out.automatic=false;out.sharpness=0;return out;}
    if(!out.enabled||!out.automatic)return out;
    out.scene_guard=true;
    if(pressure_>=1){out.profile=Profile::Performance;out.warp_iterations=1;}
    if(pressure_>=2)out.multiplier=std::max(2,out.multiplier-(pressure_-1));
    if(pressure_>=4) {
        auto current=resolve(out.processing,input);
        const Resolution ladder[]={Resolution::P720,Resolution::P540,Resolution::P360};
        int steps=pressure_-3;
        for(auto next:ladder) {
            auto size=resolve(next,input);
            if(size.width<current.width&&size.height<=current.height&&steps>0){out.processing=next;current=size;--steps;}
        }
        out.sharpness=0;
    }
    if(pressure_>=7)out.enabled=false; // bypass when even the minimum tier overruns
    return out;
}
void AdaptiveController::observe(double cpu,double budget,bool automatic) {
    if(!std::isfinite(cpu)||cpu<0||!std::isfinite(budget)||budget<=0)throw std::invalid_argument("Invalid controller observation");
    if(!automatic){reset();return;}
    ewma_=ewma_?ewma_*.75+cpu*.25:cpu;
    if(cpu>budget*1.05||(ewma_>budget*1.05&&cpu>budget)) {
        pressure_=std::min(7,pressure_+1);stable_=0;
        // Previous-tier costs must not force further degradation of a cheaper tier.
        ewma_=0;
    }
    else if(cpu<budget*.70&&ewma_<budget*.70) {
        if(++stable_>=45){pressure_=std::max(0,pressure_-1);stable_=0;ewma_=0;}
    }else stable_=0;
}
Batch& Runtime::run(const Image& a,const Image& b,const Config& requested) {
    validate(requested,a.size());effective_=adaptive_.select(requested,a.size());
    // Bypass presents the current original, avoiding the interpolation lookahead.
    auto& batch=effective_.enabled?processor_.run(a,b,effective_):processor_.run(b,b,effective_);
    if(effective_.enabled&&requested.low_latency&&batch.metrics.total_ms>requested.source_interval_ms) {
        auto attempted=batch.metrics;
        effective_.enabled=false;
        auto& newest=processor_.run(b,b,effective_);
        newest.metrics.total_ms+=attempted.total_ms;
        newest.metrics.image_allocations+=attempted.image_allocations;newest.metrics.copied_image_bytes+=attempted.copied_image_bytes;
        newest.metrics.preparation_ms+=attempted.preparation_ms;newest.metrics.motion_ms+=attempted.motion_ms;
        newest.metrics.interpolation_ms+=attempted.interpolation_ms;newest.metrics.scaling_ms+=attempted.scaling_ms;
        newest.metrics.dropped=uint64_t(attempted.generated);newest.metrics.skipped_reason=4;
        newest.metrics.latency_estimate_ms=newest.metrics.total_ms;newest.metrics.overloaded=true;
    }
    adaptive_.observe(batch.metrics.total_ms,requested.source_interval_ms,requested.automatic&&requested.enabled&&!requested.safe_mode);
    return batch;
}
PresentationSchedule::PresentationSchedule(double captured,double completed,double ms,bool low,int fps,double hz):
    anchor_(low?captured:std::max(captured,completed)),interval_(ms/1000),fps_(fps) {
    if(!std::isfinite(captured)||!std::isfinite(completed)||completed<captured||!std::isfinite(ms)||ms<=0||
       !std::isfinite(hz)||hz<=0||(fps!=0&&fps!=30&&fps!=60)||(fps&&fps>hz+.01))
        throw std::invalid_argument("Unsupported presentation schedule/cap");
}
double PresentationSchedule::due(float t)const {
    if(!std::isfinite(t)||t<0||t>=1)throw std::invalid_argument("Invalid temporal position");
    return anchor_+t*interval_;
}
bool PresentationSchedule::expired(bool generated,float t,int multiplier,double now)const {
    if(multiplier<2||multiplier>4||!std::isfinite(now))throw std::invalid_argument("Invalid presentation observation");
    return generated&&now>=due(t)+interval_/multiplier;
}
double PresentationSchedule::limited_due(double time,double last,int fps) {
    if(!std::isfinite(time)||!std::isfinite(last)||(fps!=0&&fps!=30&&fps!=60))throw std::invalid_argument("Invalid limiter observation");
    return fps&&last>0?std::max(time,last+1.0/fps):time;
}
const char* compatibility_name(Compatibility state) {
    switch(state){case Compatibility::Untested:return "UNTESTED";case Compatibility::Compatible:return "COMPATIBLE";
    case Compatibility::Limited:return "LIMITED";case Compatibility::Incompatible:return "INCOMPATIBLE";case Compatibility::Failed:return "FAILURE";}
    return "UNTESTED";
}
static std::string profile_path(const std::string& dir,const std::string& id) {
    if(id.empty()||id.size()>32||id.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Unsafe game ID");
    return dir+"/"+id+".profile";
}
void save_profile(const GameProfile& p,const std::string& dir) {
    validate(p.config,{1920,1080});
    if(int(p.state)<0||int(p.state)>4||p.evidence.size()>512||p.evidence.find_first_of("\r\n")!=std::string::npos)
        throw std::invalid_argument("Invalid compatibility evidence");
    if(p.state!=Compatibility::Untested&&p.evidence.empty())throw std::invalid_argument("A tested state requires evidence");
    // Experimental enablement is never persisted in Config. Authorization only permits known tested integrations.
    if(p.allow_automatic_enable&&(p.state!=Compatibility::Compatible&&p.state!=Compatibility::Limited))
        throw std::invalid_argument("Untested games cannot authorize automatic activation");
    auto target=profile_path(dir,p.game_id);auto temporary=target+".tmp";
    std::ofstream f(temporary);if(!f)throw std::runtime_error("Cannot write game profile");
    const auto& c=p.config;
    f<<"FRAMEGEN_PROFILE3\n"<<c.multiplier<<' '<<int(c.profile)<<' '<<int(c.processing)<<' '<<int(c.output)<<' '<<int(c.filter)
     <<' '<<c.sharpness<<' '<<c.automatic<<' '<<c.low_latency<<' '<<c.fps_limit<<' '<<c.scene_guard<<' '<<c.diagnostics
     <<' '<<int(p.state)<<' '<<p.allow_automatic_enable<<'\n'<<p.evidence<<'\n';
    f.close();if(!f)throw std::runtime_error("Profile write failed");
    // Preserve the last validated configuration before committing a replacement.
    std::ifstream prior(target);
    if(prior) {
        bool valid=true;try{load_profile(dir,p.game_id);}catch(...){valid=false;}
        if(valid) {
            std::ofstream backup(target+".previous.tmp");backup<<prior.rdbuf();backup.close();
            if(!backup)throw std::runtime_error("Cannot retain previous profile");
            atomic_replace(target+".previous.tmp",target+".previous");
        }
    }
    atomic_replace(temporary,target);
}
static GameProfile read_profile(const std::string& target,const std::string& id) {
    GameProfile p;p.game_id=id;std::ifstream f(target);if(!f)return p;
    std::string magic;std::getline(f,magic);if(!magic.empty()&&magic.back()=='\r')magic.pop_back();int profile,processing,output,filter,automatic,low,guard,diagnostics,state,allowed;
    auto& c=p.config;
    if(magic!="FRAMEGEN_PROFILE3"||!(f>>c.multiplier>>profile>>processing>>output>>filter>>c.sharpness>>automatic>>low>>c.fps_limit>>guard>>diagnostics>>state>>allowed))
        throw std::runtime_error("Corrupt game profile");
    auto bit=[](int n){return n==0||n==1;};
    if(!bit(automatic)||!bit(low)||!bit(guard)||!bit(diagnostics)||!bit(allowed)||state<0||state>4)throw std::runtime_error("Invalid profile flags");
    c.profile=Profile(profile);c.processing=Resolution(processing);c.output=Resolution(output);c.filter=Filter(filter);
    c.automatic=automatic;c.low_latency=low;c.scene_guard=guard;c.diagnostics=diagnostics;p.state=Compatibility(state);p.allow_automatic_enable=allowed;
    std::string rest;std::getline(f,rest);if(rest.find_first_not_of(" \t\r")!=std::string::npos)throw std::runtime_error("Trailing profile data");
    std::getline(f,p.evidence);if(!p.evidence.empty()&&p.evidence.back()=='\r')p.evidence.pop_back();if(std::getline(f,rest))throw std::runtime_error("Trailing profile data");
    validate(c,{1920,1080});
    if((p.state!=Compatibility::Untested&&p.evidence.empty())||p.evidence.size()>512||
       (allowed&&p.state!=Compatibility::Compatible&&p.state!=Compatibility::Limited))throw std::runtime_error("Invalid compatibility record");
    // Records are user-supplied evidence, never inferred compatibility or permission to inject.
    c.enabled=false;return p;
}
GameProfile load_profile(const std::string& dir,const std::string& id) {return read_profile(profile_path(dir,id),id);}
GameProfile previous_profile(const std::string& dir,const std::string& id) {
    auto target=profile_path(dir,id)+".previous";std::ifstream check(target);
    if(!check)throw std::runtime_error("No previous profile saved");
    auto result=read_profile(target,id);result.config.enabled=false;return result;
}
GameProfile recover_profile(const std::string& dir,const std::string& id) {
    try{return load_profile(dir,id);}catch(const std::exception&) {
        GameProfile result;try{result=previous_profile(dir,id);}catch(...){result.game_id=id;}
        result.config.enabled=false;result.config.safe_mode=true;return result;
    }
}
bool RecoverySession::begin(Config& c) {
    std::ifstream old(directory_+"/session-v03.state");std::string state;old>>state;
    bool clean=!old.is_open()||state=="CLEAN";c.enabled=false;
    if(!clean){c=Config{};c.safe_mode=true;fault("Unclean previous session; experiments blocked until manual safe-mode exit");}
    std::ofstream marker(directory_+"/session-v03.state");marker<<"RUNNING\n";marker.close();
    if(!marker)throw std::runtime_error("Cannot arm recovery marker");
    started_=true;return clean;
}
void RecoverySession::fault(const std::string& message) {
    std::ofstream log(directory_+"/faults-v03.log",std::ios::app);
    log<<std::chrono::duration<double>(Clock::now().time_since_epoch()).count()<<' '<<message<<'\n';
    if(!log)throw std::runtime_error("Cannot record fault");
}
void RecoverySession::finish(){
    if(!started_)return;
    std::ofstream f(directory_+"/session-v03.state");f<<"CLEAN\n";f.close();
    if(!f)throw std::runtime_error("Cannot commit clean session");
    started_=false;
}
Diagnostics::Diagnostics(const std::string& path):output_(path,std::ios::app) {
    if(!output_)throw std::runtime_error("Cannot open diagnostics");
}
void Diagnostics::record(const Metrics& m,const PresentationMonitor& monitor,uint64_t dropped,const Config& c,int pressure) {
    auto times=monitor.frame_times();output_<<std::setprecision(9)<<"{\"cpu_ms\":"<<m.total_ms<<",\"interpolation_ms\":"<<m.interpolation_ms
      <<",\"scaling_ms\":"<<m.scaling_ms<<",\"input\":["<<m.input.width<<','<<m.input.height<<"],\"processing\":["<<m.processing.width<<','<<m.processing.height
      <<"],\"output_buffer\":["<<m.output.width<<','<<m.output.height<<"],\"rgba_buffer_bytes\":"<<m.image_bytes<<",\"process_rss_bytes\":null,\"dropped\":"<<dropped
      <<",\"confirmed_flips\":"<<monitor.presented_count()<<",\"source_fps\":";
    if(monitor.original_fps()>0)output_<<monitor.original_fps();else output_<<"null";
    output_<<",\"presented_fps\":";if(monitor.presented_fps()>0)output_<<monitor.presented_fps();else output_<<"null";
    output_<<",\"frametime_mean_ms\":";if(times.samples)output_<<times.mean;else output_<<"null";
    output_<<",\"frametime_max_ms\":";if(times.samples)output_<<times.maximum;else output_<<"null";
    output_<<",\"frametime_p95_ms\":";if(times.samples)output_<<times.p95;else output_<<"null";
    output_<<",\"frametime_p50_ms\":";if(times.samples)output_<<times.p50;else output_<<"null";
    output_<<",\"frametime_p99_ms\":";if(times.samples)output_<<times.p99;else output_<<"null";
    output_<<",\"latency_estimate_ms\":"<<m.latency_estimate_ms<<",\"measured_input_to_photon_ms\":null,\"gpu_ms\":null,\"skip_reason\":"<<m.skipped_reason
      <<",\"pressure\":"<<pressure<<",\"effective_multiplier\":"<<c.multiplier<<",\"enabled\":"<<c.enabled<<"}\n";
    output_.flush();if(!output_)throw std::runtime_error("Diagnostic write failed");
}
Comparison compare(const Image& candidate,const Image& reference) {
    if(candidate.size()!=reference.size())throw std::invalid_argument("Comparison dimensions differ");
    Comparison out{psnr(candidate,reference),0,0,Image(candidate.size())};auto s=candidate.size();double edges=0;
    for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x) {
        auto i=std::size_t(y)*s.width+x;auto a=candidate.data()[i],b=reference.data()[i];
        int r=std::abs(int(a.r)-b.r),g=std::abs(int(a.g)-b.g),bl=std::abs(int(a.b)-b.b);out.mae+=r+g+bl;
        out.difference.data()[i]={uint8_t(std::min(255,r*4)),uint8_t(std::min(255,g*4)),uint8_t(std::min(255,bl*4)),255};
        if(x){auto aa=candidate.data()[i-1],bb=reference.data()[i-1];
            out.edge_mae+=std::abs((int(a.r)-aa.r)-(int(b.r)-bb.r))+std::abs((int(a.g)-aa.g)-(int(b.g)-bb.g))+std::abs((int(a.b)-aa.b)-(int(b.b)-bb.b));edges+=3;}
    }
    out.mae/=checked_pixels(s)*3;if(edges)out.edge_mae/=edges;return out;
}
Image comparison_panel(const Image& original,const Image& generated,const Image* reference) {
    if(original.size()!=generated.size()||(reference&&reference->size()!=generated.size()))throw std::invalid_argument("Panel dimensions differ");
    Image out(generated);auto size=out.size();
    for(int y=0;y<size.height;++y)for(int x=0;x<size.width/2;++x)out.data()[std::size_t(y)*size.width+x]=original.data()[std::size_t(y)*size.width+x];
    if(reference) {
        auto diff=compare(generated,*reference);
        for(int y=size.height/2;y<size.height;++y)for(int x=size.width/2;x<size.width;++x)out.data()[std::size_t(y)*size.width+x]=diff.difference.data()[std::size_t(y)*size.width+x];
    }
    draw_text(out,8,8,"ORIGINAL / GENERATED",1);return out;
}
}
