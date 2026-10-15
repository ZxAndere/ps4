#include "engine/framegen.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>

// Interleave variants without decoding/writing images between timed pairs.
// This reduces the order/frequency bias of separate CLI invocations.
struct Variant {
    std::string name;fg::Config config;fg::Processor processor;
    std::vector<double> samples,motion;fg::Batch* last=nullptr;
    Variant(std::string n,fg::Config c):name(std::move(n)),config(c),processor(c.execution){}
};
static double quantile(std::vector<double> values,double q) {
    std::sort(values.begin(),values.end());return values.at(std::min(values.size()-1,std::size_t(q*(values.size()-1))));
}
int main(int argc,char** argv) {
 try {
    if(argc!=5)throw std::invalid_argument("framegen_ablation A.ppm B.ppm output.json rounds");
    if(std::filesystem::exists(argv[3]))throw std::invalid_argument("Refusing to replace an existing report");
    int rounds=std::stoi(argv[4]);if(rounds<5||rounds>100)throw std::invalid_argument("Rounds must be 5..100");
    auto a=fg::read_ppm(argv[1]),b=fg::read_ppm(argv[2]);
    fg::Config c;c.enabled=true;c.processing=fg::Resolution::P720;c.output=fg::Resolution::P720;c.source_interval_ms=80;
    c.execution.threads=1;c.execution.kernels=false;c.execution.simd=false;c.execution.direct=false;c.execution.reuse=false;c.execution.efficient_motion=false;
    std::vector<std::unique_ptr<Variant>> variants;
    auto add=[&](const char* name){variants.emplace_back(new Variant(name,c));};
    add("reference");c.execution.kernels=true;add("scalar");c.execution.simd=true;add("sse2");
    c.execution.direct=true;add("direct");c.execution.reuse=true;add("reuse");
    c.execution.threads=2;add("threads2");c.execution.threads=4;add("threads4");
    c.execution.efficient_motion=true;add("motion-early");
    for(auto& v:variants)v->processor.run(a,b,v->config);
    std::vector<unsigned> order(variants.size());std::iota(order.begin(),order.end(),0);
    std::mt19937 random(0x46524732);
    for(int round=0;round<rounds;++round) {
        std::shuffle(order.begin(),order.end(),random);
        for(auto index:order){auto& v=*variants[index];v.last=&v.processor.run(a,b,v.config);
            v.samples.push_back(v.last->metrics.total_ms);v.motion.push_back(v.last->metrics.motion_ms);}
    }
    std::filesystem::path output=argv[3];std::filesystem::create_directories(output.parent_path());
    std::ofstream f(output);f<<"{\n\"rounds\":"<<rounds<<",\n\"seed\":1179797298,\n\"interleaved\":true,\n\"cases\":[\n";
    double baseline=quantile(variants[0]->samples,.5);
    for(std::size_t i=0;i<variants.size();++i) {
        auto& v=*variants[i];auto& batch=*v.last;
        double error=fg::mse(batch.frames[1],variants[0]->last->frames[1]);
        if(error!=0)throw std::runtime_error("Pixel equivalence regression in "+v.name);
        if(i)f<<",\n";
        auto& m=batch.metrics;
        f<<"{\"stage\":\""<<v.name<<"\",\"median_pair_ms\":"<<quantile(v.samples,.5)
            <<",\"p10_ms\":"<<quantile(v.samples,.1)<<",\"p90_ms\":"<<quantile(v.samples,.9)
            <<",\"min_ms\":"<<quantile(v.samples,0)<<",\"max_ms\":"<<quantile(v.samples,1)
            <<",\"motion_median_ms\":"<<quantile(v.motion,.5)<<",\"speedup\":"<<baseline/quantile(v.samples,.5)
            <<",\"mse_vs_reference\":"<<error<<",\"image_allocations_last_pair\":"<<m.image_allocations
            <<",\"copied_image_bytes_last_pair\":"<<m.copied_image_bytes<<",\"samples_ms\":[";
        for(std::size_t j=0;j<v.samples.size();++j){if(j)f<<',';f<<v.samples[j];}f<<"]}";
        std::cout<<v.name<<": median "<<quantile(v.samples,.5)<<" ms, speedup "<<baseline/quantile(v.samples,.5)<<", MSE "<<error<<'\n';
    }
    f<<"\n]}\n";f.close();if(!f)throw std::runtime_error("Report write failed");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
