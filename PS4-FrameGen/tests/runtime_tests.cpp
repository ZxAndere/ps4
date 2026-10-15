#include "engine/runtime.hpp"
#include "overlay/overlay.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
#include <cmath>
#include <random>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}CHECK(bad);}
int main(){
 using namespace fg;int passed=0,failed=0;
 auto dir=std::filesystem::temp_directory_path()/("framegen-v03-"+std::to_string(Clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(dir);
 std::vector<std::pair<std::string,std::function<void()>>> cases={
 {"adaptive overload degrades then bypasses and recovers with hysteresis",[]{
    AdaptiveController changed;changed.observe(100,33,true);CHECK(changed.pressure()==1);
    changed.observe(10,33,true);CHECK(changed.pressure()==1); // no stale-tier EWMA cascade
    AdaptiveController a;Config c;c.enabled=true;c.automatic=true;c.multiplier=4;c.profile=Profile::Quality;
    c.processing=Resolution::P1080;
    auto initial=a.select(c,{1920,1080});CHECK(initial.multiplier==4);
    a.observe(60,33,true);CHECK(a.select(c,{1920,1080}).profile==Profile::Performance);
    a.observe(60,33,true);CHECK(a.select(c,{1920,1080}).multiplier==3);
    for(int i=0;i<4;++i)a.observe(60,33,true);
    CHECK(a.select(c,{1920,1080}).processing==Resolution::P360);
    a.observe(60,33,true);CHECK(!a.select(c,{1920,1080}).enabled);
    for(int i=0;i<200;++i)a.observe(1,33,true);
    CHECK(a.pressure()<7);a.observe(1,33,false);CHECK(a.pressure()==0);
    rejects([&]{a.observe(-1,33,true);});
 }},
 {"capture deadlines discard generated frames only and limiter cannot exceed display",[]{
    PresentationSchedule low(10,10.045,40,true,0),normal(10,10.045,40,false,0);
    CHECK(low.expired(true,.5f,2,10.045));CHECK(!low.expired(false,0,2,11));
    CHECK(!normal.expired(true,.5f,2,10.045));
    CHECK(std::abs(normal.due(.5f)-10.065)<1e-8);
    CHECK(PresentationSchedule::limited_due(10,10,30)>10.033);
    rejects([]{PresentationSchedule bad(1,1,40,true,60,50);});
 }},
 {"runtime bypass and safe mode present newest original",[]{
    auto a=test_scene({80,60},0),b=test_scene({80,60},8);Config c;Runtime runtime;
    auto& output=runtime.run(a,b,c);CHECK(output.metrics.generated==0);CHECK(mse(output.frames[0],b)==0);
    c.enabled=true;c.safe_mode=true;CHECK(mse(runtime.run(a,b,c).frames[0],b)==0);
 }},
 {"low latency overload bypass counts discards and presents current original",[]{
    auto a=test_scene({80,60},0),b=test_scene({80,60},8);Config c;c.enabled=true;c.low_latency=true;c.source_interval_ms=.000001;
    Runtime runtime;auto& output=runtime.run(a,b,c);CHECK(output.metrics.skipped_reason==4);CHECK(output.metrics.dropped==1);
    CHECK(output.metrics.generated==0);CHECK(mse(output.frames[0],b)==0);CHECK(!runtime.effective().enabled);
 }},
 {"SIMD flow and integer area preserve RGB across scales and multipliers",[]{
    for(auto size:{Size{162,90},Size{159,91}}) {
      auto a=test_scene(size,0),b=test_scene(size,12);
      for(int multiplier:{2,3,4}) {
        Config c;c.enabled=true;c.multiplier=multiplier;c.execution.threads=2;
        c.execution.vector_flow=false;auto expected=process(a,b,c);c.execution.vector_flow=true;auto actual=process(a,b,c);
        for(std::size_t i=0;i<actual.frames.size();++i)CHECK(mse(expected.frames[i],actual.frames[i])==0);
      }
    }
    auto a=test_scene({1920,1080},0);
    for(auto resolution:{Resolution::P540,Resolution::P360}) {
      Config c;c.enabled=true;c.processing=resolution;c.output=Resolution::P720;c.execution.integer_scale=false;
      auto expected=process(a,a,c);c.execution.integer_scale=true;auto actual=process(a,a,c);
      for(std::size_t i=0;i<actual.frames.size();++i)CHECK(mse(expected.frames[i],actual.frames[i])==0);
    }
 }},
 {"nearest and cubic handle corners constants and letterbox",[]{
    Image constant({13,7});for(std::size_t i=0;i<checked_pixels(constant.size());++i)constant.data()[i]={70,120,210,255};
    for(auto f:{Filter::Nearest,Filter::Bicubic}) {
      auto enlarged=scale(constant,{31,31},f);CHECK(enlarged.at(15,15).r==70);CHECK(enlarged.at(0,0).r==0);
      auto small=scale(constant,{1,1},f);CHECK(small.at(0,0).b==210);
    }
    CHECK(mse(sharpen(constant,100),constant)==0);rejects([&]{sharpen(constant,101);});
    Image edge({3,1});edge.at(1,0)={100,100,100,255};CHECK(sharpen(edge,100).at(1,0).r>100);
 }},
 {"scene guard records unreliable motion and cut suppression",[]{
    Image a({160,90}),b({160,90});std::mt19937 random(1347);
    for(std::size_t i=0;i<checked_pixels(a.size());++i) {
      a.data()[i]={uint8_t(random()),uint8_t(random()),uint8_t(random()),255};
      b.data()[i]={uint8_t(random()),uint8_t(random()),uint8_t(random()),255};
    }
    Config c;c.enabled=true;c.scene_guard=true;auto result=process(a,b,c);
    CHECK(result.metrics.generated==0);CHECK(result.metrics.skipped_reason==2);
    for(std::size_t i=0;i<checked_pixels(a.size());++i){a.data()[i]={0,0,0,255};b.data()[i]={255,255,255,255};}
    CHECK(process(a,b,c).metrics.skipped_reason==1);
 }},
 {"flip statistics have bounded samples and no fake presentation",[]{
    PresentationMonitor m;CHECK(m.frame_times().samples==0);m.original(1);m.original(1.04);CHECK(m.presented_count()==0);
    m.confirmed_present(2);m.confirmed_present(2.01);m.confirmed_present(2.03);
    auto f=m.frame_times();CHECK(f.samples==2);CHECK(std::abs(f.mean-15)<1e-8);CHECK(std::abs(f.p95-20)<1e-8);
    for(int i=1;i<=300;++i)m.confirmed_present(2.03+i*.01);CHECK(m.frame_times().samples==240);
 }},
 {"versioned profiles roundtrip without activating untested games",[&]{
    GameProfile p;p.game_id="CUSA00000";p.config.multiplier=4;p.config.low_latency=true;p.config.sharpness=25;
    p.config.filter=Filter::Bicubic;p.config.automatic=true;save_profile(p,dir.string());
    auto loaded=load_profile(dir.string(),p.game_id);CHECK(!loaded.config.enabled);CHECK(loaded.config.low_latency);CHECK(loaded.config.sharpness==25);
    CHECK(loaded.state==Compatibility::Untested);p.allow_automatic_enable=true;rejects([&]{save_profile(p,dir.string());});
    p.allow_automatic_enable=false;p.state=Compatibility::Compatible;rejects([&]{save_profile(p,dir.string());});
    p.state=Compatibility::Untested;p.game_id="../escape";rejects([&]{save_profile(p,dir.string());});
 }},
 {"corrupt profile recovers last valid preferences without activation",[&]{
    GameProfile p;p.game_id="BACKUP";p.config.multiplier=3;save_profile(p,dir.string());
    p.config.multiplier=4;save_profile(p,dir.string());CHECK(previous_profile(dir.string(),p.game_id).config.multiplier==3);
    {std::ofstream corrupt(dir/"BACKUP.profile");corrupt<<"invalid";}
    auto recovered=recover_profile(dir.string(),p.game_id);CHECK(recovered.config.multiplier==3);CHECK(!recovered.config.enabled&&recovered.config.safe_mode);
 }},
 {"unclean restart locks experiments and faults remain recoverable",[&]{
    Config c;c.enabled=true;RecoverySession first(dir.string());CHECK(first.begin(c));CHECK(!c.enabled);
    RecoverySession after_crash(dir.string());CHECK(!after_crash.begin(c));CHECK(c.safe_mode&&!c.enabled);
    after_crash.fault("deliberate test fault");after_crash.finish();RecoverySession next(dir.string());CHECK(next.begin(c));next.finish();
    CHECK(std::filesystem::file_size(dir/"faults-v03.log")>0);
 }},
 {"JSONL diagnostics report unavailable display values as null",[&]{
    Diagnostics log((dir/"diagnostics.jsonl").string());Metrics m;PresentationMonitor monitor;Config c;
    log.record(m,monitor,3,c,2);std::ifstream f(dir/"diagnostics.jsonl");std::string text;std::getline(f,text);
    CHECK(text.find("\"presented_fps\":null")!=std::string::npos);CHECK(text.find("\"gpu_ms\":null")!=std::string::npos);CHECK(text.find("\"dropped\":3")!=std::string::npos);
 }},
 {"visual comparator uses supplied reference and produces actual differences",[]{
    auto a=test_scene({80,60},0),b=test_scene({80,60},8);auto same=compare(a,a);CHECK(same.mae==0&&std::isinf(same.psnr_db));
    auto different=compare(a,b);CHECK(different.mae>0&&different.edge_mae>0);CHECK(mse(different.difference,Image(a.size()))>0);
    CHECK(mse(comparison_panel(a,b,&a),a)>0);
 }},
 {"sections emergency disable and defaults are connected",[]{
    Menu m;m.update(L3|R3);m.update(0);m.update(Cross);CHECK(m.config.enabled);
    m.update(0);m.update(R1);CHECK(m.section==1);m.update(0);m.update(Down);m.update(0);m.update(Down);m.update(0);m.update(Cross);CHECK(m.config.filter==Filter::Nearest);
    m.update(0);m.update(Emergency);CHECK(!m.config.enabled&&m.config.safe_mode);m.update(0);m.update(Cross);CHECK(!m.config.enabled);
 }},
 {"steady memory remains bounded over alternating modes",[]{
    Image retained({320,240});auto capacity=retained.capacity_bytes();retained.reset({3,2});
    CHECK(retained.capacity_bytes()==capacity);CHECK(retained.bytes()==24);
    auto a=test_scene({80,60},0),b=test_scene({80,60},8);Config c;c.enabled=true;Processor processor(c.execution);
    for(int i=0;i<120;++i) {
       c.multiplier=2+i%3;c.enabled=i%5!=0;auto& result=processor.run(a,b,c);
       CHECK(result.frames.size()<=4);CHECK(result.metrics.image_bytes<80u*60*4*10);
    }
    c.enabled=true;c.multiplier=4;processor.run(a,b,c);CHECK(processor.run(a,b,c).metrics.image_allocations==0);
 }}
 };
 for(auto& item:cases)try{item.second();++passed;std::cout<<"PASS "<<item.first<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<item.first<<": "<<e.what()<<'\n';}
 std::filesystem::remove_all(dir);std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
