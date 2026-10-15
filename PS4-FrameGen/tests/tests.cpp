#include "engine/framegen.hpp"
#include "overlay/overlay.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#define CHECK(expr) do { if(!(expr)) throw std::runtime_error("Check failed: " #expr); } while(false)
template<class F> void rejects(F f) {bool threw=false;try{f();}catch(const std::exception&){threw=true;}CHECK(threw);}
int main() {
 using namespace fg;int passed=0,failed=0;
 auto directory=std::filesystem::temp_directory_path()/("framegen-functional-tests-"+std::to_string(Clock::now().time_since_epoch().count()));
 std::filesystem::create_directories(directory);
 std::vector<std::pair<std::string,std::function<void()>>> cases={
 {"invalid allocation and bounds",[]{rejects([]{Image i({-1,10});});rejects([]{Image i({4096,2161});});Image im({3,2});rejects([&]{im.at(3,0);});}},
 {"PPM round trip preserves binary whitespace",[&]{Image a({3,2});a.at(0,0)={10,13,32,255};a.at(2,1)={1,249,88,255};auto p=(directory/"image.ppm").string();write_ppm(a,p);CHECK(mse(a,read_ppm(p))==0);}},
 {"malformed and truncated capture",[&]{auto p=(directory/"bad.ppm").string();{std::ofstream f(p);f<<"P6\n4 4\n255\nabc";}rejects([&]{read_ppm(p);});}},
 {"area integrates pixels instead of relabeling",[]{Image a({4,4});for(int y=0;y<4;++y)for(int x=0;x<4;++x)a.at(x,y)={uint8_t((x+y)%2*200),0,0,255};auto b=scale(a,{2,2});CHECK(b.size()==(Size{2,2}));for(int y=0;y<2;++y)for(int x=0;x<2;++x)CHECK(b.at(x,y).r==100);}},
 {"letterbox preserves aspect ratio",[]{Image a({100,50});for(int y=0;y<50;++y)for(int x=0;x<100;++x)a.at(x,y)={200,100,50,255};auto b=scale(a,{50,50});CHECK(b.at(25,0).r==0);CHECK(b.at(25,25).r==200);CHECK(b.at(25,49).r==0);}},
 {"true 1080 and 720 output buffers",[]{auto a=test_scene({2560,1440},0);for(auto res:{Resolution::P1080,Resolution::P720}){auto expected=resolve(res,a.size());auto out=scale(a,expected);CHECK(out.size()==expected);CHECK(out.bytes()==std::size_t(expected.width)*expected.height*4);CHECK(out.at(100,100).r!=a.at(100,100).r);}}},
 {"motion recovers image translation",[]{auto a=test_scene({160,120},0),b=test_scene({160,120},8);auto m=estimate(a,b,Profile::Quality);auto v=m.forward.at(80,60);CHECK(std::abs(v.x-8)<1);CHECK(std::abs(v.y-3)<1);}},
 {"intermediate agrees with withheld ground truth",[]{auto a=test_scene({160,120},0),b=test_scene({160,120},8),truth=test_scene({160,120},4);auto m=estimate(a,b,Profile::Quality);auto out=interpolate(a,b,m,.5f);double error=0,base=0;for(int y=24;y<96;++y)for(int x=24;x<136;++x){auto p=out.at(x,y),q=truth.at(x,y),r=a.at(x,y);error+=std::pow(int(p.r)-int(q.r),2);base+=std::pow(int(r.r)-int(q.r),2);}CHECK(error<base*.15);CHECK(mse(out,a)>0);CHECK(mse(out,b)>0);}},
 {"distinct times and pixels for all multipliers",[]{auto a=test_scene({160,120},0),b=test_scene({160,120},8);for(int multiplier=2;multiplier<=4;++multiplier){Config c;c.enabled=true;c.multiplier=multiplier;c.profile=Profile::Quality;auto batch=process(a,b,c);CHECK(batch.frames.size()==std::size_t(multiplier));CHECK(batch.metrics.generated==multiplier-1);for(int i=1;i<multiplier;++i){CHECK(std::abs(batch.times[i]-float(i)/multiplier)<1e-6);CHECK(mse(batch.frames[i],batch.frames[i-1])>0);}}}},
 {"invalid configuration rejected",[]{Config c;c.enabled=true;c.output=Resolution::P1080;rejects([&]{validate(c,{1280,720});});c.output=Resolution::Native;c.multiplier=5;rejects([&]{validate(c,{1280,720});});c.multiplier=2;c.source_interval_ms=std::numeric_limits<double>::quiet_NaN();rejects([&]{validate(c,{1280,720});});}},
 {"malformed motion rejected before buffer access",[]{auto a=test_scene({32,24},0);Motion m;m.forward.image_size=m.backward.image_size=a.size();rejects([&]{interpolate(a,a,m,.5f);});}},
 {"disable restores original image and resolution",[]{auto a=test_scene({1920,1080},0),b=test_scene({1920,1080},8);Config c;c.enabled=false;c.output=Resolution::P720;c.processing=Resolution::P720;auto out=process(a,b,c);CHECK(out.metrics.output==a.size());CHECK(out.metrics.processing==a.size());CHECK(out.frames.size()==1);CHECK(mse(out.frames[0],a)==0);}},
 {"scene cut suppresses generation",[]{Image a({64,48}),b({64,48});for(int y=0;y<48;++y)for(int x=0;x<64;++x)b.at(x,y)={255,255,255,255};Config c;c.enabled=true;auto batch=process(a,b,c);CHECK(batch.metrics.scene_cut);CHECK(batch.metrics.generated==0);CHECK(batch.frames.size()==1);}},
 {"L3 R3 edge toggles once and controls edit",[]{Menu m;m.update(L3);CHECK(!m.visible);m.update(L3|R3);CHECK(m.visible);m.update(L3|R3);CHECK(m.visible);m.update(0);m.update(Cross);CHECK(m.config.enabled);m.update(0);m.update(Down);m.update(0);m.update(Right);CHECK(m.config.multiplier==3);m.update(0);m.update(L3|R3);CHECK(!m.visible);}},
 {"safe preferences and corrupt config recovery",[&]{Config c;c.multiplier=4;c.output=Resolution::P720;save_settings(c,directory.string(),"BREW00001");auto r=load_settings(directory.string(),"BREW00001");CHECK(r.multiplier==4);CHECK(r.output==Resolution::P720);CHECK(!r.enabled);c.multiplier=3;save_settings(c,directory.string(),"BREW00001");CHECK(load_settings(directory.string(),"BREW00001").multiplier==3);rejects([&]{load_settings(directory.string(),"../bad");});{std::ofstream f(directory/"BREW00001.cfg");f<<"FRAMEGEN1 99 0 0 0 1";}rejects([&]{load_settings(directory.string(),"BREW00001");});}},
 {"presentation counts only explicit confirmations",[]{PresentationMonitor m;m.original(1);m.original(1.04);CHECK(std::abs(m.original_fps()-25)<1e-6);CHECK(m.presented_count()==0);CHECK(m.presented_fps()==0);m.confirmed_present(2);m.confirmed_present(2.02);CHECK(std::abs(m.presented_fps()-50)<1e-6);rejects([&]{m.confirmed_present(1);});}},
 {"overlay rasterizes actual pixels safely",[]{Image image({320,240});Menu menu;menu.visible=true;Metrics metrics;PresentationMonitor monitor;auto before=image;draw_menu(image,menu,metrics,monitor,"TEST");CHECK(mse(image,before)>0);}}
 };
 cases.emplace_back("optimized output agrees with reference at every temporal position",[]{
    for(auto size:{Size{160,120},Size{157,93},Size{3,2},Size{1,1}}) {
        auto a=test_scene(size,0),b=test_scene(size,8);
        for(int multiplier=2;multiplier<=4;++multiplier)for(bool simd:{false,true}) {
            Config c;c.enabled=true;c.multiplier=multiplier;c.execution.simd=simd;c.execution.threads=2;
            auto ref=process_reference(a,b,c);auto fast=process(a,b,c);
            CHECK(ref.times==fast.times);CHECK(ref.frames.size()==fast.frames.size());
            for(std::size_t i=0;i<ref.frames.size();++i) {
                CHECK(mse(ref.frames[i],fast.frames[i])<=.05);
                for(std::size_t p=0;p<checked_pixels(size);++p) {
                    auto x=ref.frames[i].data()[p],y=fast.frames[i].data()[p];
                    CHECK(std::abs(int(x.r)-y.r)<=1);CHECK(std::abs(int(x.g)-y.g)<=1);CHECK(std::abs(int(x.b)-y.b)<=1);
                }
            }
        }
    }
 });
 cases.emplace_back("motion early exit preserves fields and costs",[]{
    auto a=test_scene({157,93},0),b=test_scene({157,93},11);
    for(auto profile:{Profile::Performance,Profile::Quality}) {
        auto full=estimate(a,b,profile),fast=estimate_efficient(a,b,profile);
        for(int direction=0;direction<2;++direction) {
            auto& x=direction?full.backward:full.forward;auto& y=direction?fast.backward:fast.forward;
            CHECK(x.vectors.size()==y.vectors.size());
            for(std::size_t i=0;i<x.vectors.size();++i) {
                CHECK(x.vectors[i].x==y.vectors[i].x);CHECK(x.vectors[i].y==y.vectors[i].y);CHECK(x.vectors[i].cost==y.vectors[i].cost);
            }
        }
    }
 });
 cases.emplace_back("parallel output deterministic and warm image storage reused",[]{
    auto a=test_scene({160,120},0),b=test_scene({160,120},8);Config c;c.enabled=true;c.multiplier=4;
    c.execution.threads=1;Processor single(c.execution);auto expected=single.run(a,b,c);
    c.execution.threads=4;Processor pool(c.execution);pool.run(a,b,c);
    auto& output=pool.run(a,b,c);CHECK(output.metrics.image_allocations==0);
    CHECK(output.metrics.copied_image_bytes==a.bytes());
    for(std::size_t i=0;i<output.frames.size();++i)CHECK(mse(expected.frames[i],output.frames[i])==0);
    c.enabled=false;CHECK(pool.run(a,b,c).metrics.generated==0);
    c.enabled=true;CHECK(pool.run(a,b,c).metrics.generated==3);
    auto resized=test_scene({157,93},0);CHECK(pool.run(resized,resized,c).metrics.output==resized.size());
 });
 cases.emplace_back("540p and 360p are real processing surfaces only",[]{
    auto a=test_scene({1280,720},0),b=test_scene({1280,720},8);
    for(auto resolution:{Resolution::P540,Resolution::P360}) {
        Config c;c.enabled=true;c.processing=resolution;c.output=Resolution::P720;
        auto output=process(a,b,c);CHECK(output.metrics.processing==resolve(resolution,a.size()));
        CHECK(output.frames[1].size()==(Size{1280,720}));c.output=resolution;
        rejects([&]{validate(c,a.size());});
    }
 });
 cases.emplace_back("fast scaler preserves letterboxing and both filters",[]{
    auto a=test_scene({1920,900},0);
    for(auto filter:{Filter::Area,Filter::Bilinear}) {
        Config c;c.enabled=true;c.output=Resolution::P720;c.filter=filter;
        auto x=process_reference(a,a,c),y=process(a,a,c);CHECK(mse(x.frames[0],y.frames[0])<=.01);
        CHECK(y.frames[0].at(0,0).r==0);
    }
 });
 for(auto& item:cases)try{item.second();++passed;std::cout<<"PASS "<<item.first<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<item.first<<": "<<e.what()<<'\n';}
 std::filesystem::remove_all(directory);
 std::cout<<passed<<" passed, "<<failed<<" failed, 0 skipped\n";return failed?1:0;
}
