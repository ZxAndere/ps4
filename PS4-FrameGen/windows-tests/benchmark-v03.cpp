#include "engine/framegen.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <cmath>
#include <numeric>
#include <stdexcept>
#ifdef _MSC_VER
constexpr const char* compiler_name="MSVC";
#else
constexpr const char* compiler_name=__VERSION__;
#endif
struct Variant {
    std::string name;fg::Config config;std::unique_ptr<fg::Processor> engine;
    std::vector<double> ms,prep,warp,scaling;fg::Image output{{1,1}};fg::Metrics metrics;
};
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(int argc,char** argv) {
 try {
    if(argc!=6)throw std::runtime_error("framegen_benchmark_v03 A.ppm B.ppm TRUTH.ppm NEW_REPORT.json rounds");
    std::ifstream prior(argv[4]);if(prior.good())throw std::runtime_error("Report already exists");
    int rounds=std::stoi(argv[5]);if(rounds<5||rounds>100)throw std::runtime_error("Use 5..100 rounds");
    auto a=fg::read_ppm(argv[1]),b=fg::read_ppm(argv[2]),truth=fg::read_ppm(argv[3]);std::vector<Variant> variants;
    auto add=[&](std::string name,fg::Resolution res,bool flow,bool integer,int multiplier=2,fg::Filter filter=fg::Filter::Area,int sharp=0,bool cached=false,int iterations=2){
        Variant v;v.name=name;v.config.enabled=true;v.config.processing=res;v.config.output=fg::Resolution::P720;
        v.config.multiplier=multiplier;v.config.filter=filter;v.config.sharpness=sharp;v.config.source_interval_ms=80;
        v.config.execution.vector_flow=flow;v.config.execution.integer_scale=integer;v.config.execution.cached_scale=cached;v.config.warp_iterations=iterations;
        v.engine=std::make_unique<fg::Processor>(v.config.execution);variants.push_back(std::move(v));
    };
    using R=fg::Resolution;using F=fg::Filter;
    add("v02-equivalent-720",R::P720,false,false);add("vector-flow-720",R::P720,true,false);
    add("v02-equivalent-540",R::P540,false,false);add("vector-flow-540",R::P540,true,false);add("integer-area-540",R::P540,true,true);
    add("v02-equivalent-360",R::P360,false,false);add("vector-flow-360",R::P360,true,false);add("integer-area-360",R::P360,true,true);
    add("nearest-360",R::P360,true,true,2,F::Nearest);add("bilinear-360",R::P360,true,true,2,F::Bilinear);
    add("bicubic-360",R::P360,true,true,2,F::Bicubic);add("sharp25-360",R::P360,true,true,2,F::Area,25);
    add("vector-flow-720-x3",R::P720,true,true,3);add("vector-flow-720-x4",R::P720,true,true,4);
    add("integer-area-360-x3",R::P360,true,true,3);add("integer-area-360-x4",R::P360,true,true,4);
    add("cached-scale-540",R::P540,true,true,2,F::Area,0,true);
    add("cached-scale-360",R::P360,true,true,2,F::Area,0,true);
    add("one-iteration-720",R::P720,true,true,2,F::Area,0,true,1);
    add("one-iteration-360",R::P360,true,true,2,F::Area,0,true,1);
    add("v02-equivalent-720-x3",R::P720,false,false,3);
    add("v02-equivalent-720-x4",R::P720,false,false,4);
    add("v02-equivalent-360-x3",R::P360,false,false,3);
    add("v02-equivalent-360-x4",R::P360,false,false,4);
    add("v03-default-720",R::P720,false,true,2,F::Area,0,true);
    add("v03-default-540",R::P540,false,true,2,F::Area,0,true);
    add("v03-default-360",R::P360,false,true,2,F::Area,0,true);
    add("one-iteration-default-720",R::P720,false,true,2,F::Area,0,true,1);
    std::vector<std::size_t> order(variants.size());std::iota(order.begin(),order.end(),0);std::mt19937 random(0x46524733);
    for(auto& v:variants)v.engine->run(a,b,v.config);
    for(int r=0;r<rounds;++r) {
        std::shuffle(order.begin(),order.end(),random);
        for(auto i:order){auto& v=variants[i];auto& batch=v.engine->run(a,b,v.config);auto m=batch.metrics;
            v.ms.push_back(m.total_ms);v.prep.push_back(m.preparation_ms);v.warp.push_back(m.interpolation_ms);v.scaling.push_back(m.scaling_ms);
        }
    }
    // No quality calculations or file writes inside the timed rounds.
    auto reference=fg::scale(truth,{1280,720},F::Area);std::vector<fg::Image> baselines;
    for(int i:{0,2,5})baselines.push_back(variants[i].engine->run(a,b,variants[i].config).frames.at(1));
    std::ofstream out(argv[4]);out<<std::setprecision(10)<<"{\"seed\":1179797299,\"rounds\":"<<rounds<<",\"compiler\":\""<<compiler_name<<"\",\"host_cpu_only\":true,\"variants\":[";
    for(std::size_t i=0;i<variants.size();++i) {
        auto& v=variants[i];auto& batch=v.engine->run(a,b,v.config);auto sorted=v.ms;std::sort(sorted.begin(),sorted.end());
        auto& frame=batch.frames.at(1);double error=-1;
        if(i<8||i==16||i==17)error=fg::mse(frame,baselines[i<2?0:(i<5||i==16)?1:2]);
        if((i<8||i==16||i==17)&&error!=0)throw std::runtime_error("Pixel regression: "+v.name);
        if(i)out<<',';
        out<<"\n{\"name\":\""<<v.name<<"\",\"median_ms\":"<<median(v.ms)<<",\"p10_ms\":"<<sorted[sorted.size()/10]<<",\"p90_ms\":"<<sorted[sorted.size()*9/10]
           <<",\"preparation_median_ms\":"<<median(v.prep)<<",\"warp_median_ms\":"<<median(v.warp)<<",\"scaling_median_ms\":"<<median(v.scaling)
           <<",\"reference_mse\":";if(error<0)out<<"null";else out<<error;
        out<<",\"withheld_psnr_db\":";if(v.config.multiplier==2)out<<fg::psnr(frame,reference);else out<<"null";
        out<<",\"image_allocations\":"<<batch.metrics.image_allocations<<",\"copied_image_bytes\":"<<batch.metrics.copied_image_bytes<<",\"samples_ms\":[";
        for(std::size_t n=0;n<v.ms.size();++n){if(n)out<<',';out<<v.ms[n];}out<<"]}";
        std::cout<<v.name<<' '<<median(v.ms)<<" ms RGB MSE "<<error<<'\n';
    }
    out<<"]}\n";if(!out)throw std::runtime_error("Cannot write benchmark");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
