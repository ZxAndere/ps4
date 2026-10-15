#include "engine/framegen.hpp"
#include "overlay/overlay.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
fg::Resolution resolution(const std::string& s) {
    if(s=="native")return fg::Resolution::Native;
    if(s=="1080")return fg::Resolution::P1080;
    if(s=="720")return fg::Resolution::P720;
    if(s=="540")return fg::Resolution::P540;
    if(s=="360")return fg::Resolution::P360;
    throw std::invalid_argument("Resolution must be native, 1080, 720, 540 or 360");
}
void dimension(std::ostream& f,fg::Size s) {f<<'['<<s.width<<','<<s.height<<']';}
void number(std::ostream& f,double n) {if(std::isfinite(n))f<<n;else f<<"null";}
}
int main(int argc,char** argv) {
 try {
    fg::Config config;config.enabled=true;
    bool demo=false;std::string input,next,output="artifacts/demo-v03";int repeats=1;
    std::vector<std::string> truth;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--demo"){demo=true;continue;}
        if(arg=="--disabled"){config.enabled=false;continue;}
        if(arg=="--help") {
            std::cout<<"framegen_cli (--demo | --input A.ppm --next B.ppm)\n"
                "  --multiplier 2|3|4 --process native|1080|720|540|360 --output native|1080|720\n"
                "  --profile performance|balanced|quality --filter area|bilinear|nearest|bicubic --sharpness 0..100 --repeats N\n"
                "  --threads 1..8 --kernels fast|reference --simd on|off --vector-flow on|off --reuse on|off\n"
                "  --direct on|off --motion full|early --integer-scale on|off --cached-scale on|off --warp-iterations 1|2\n"
                "  --truth intermediate.ppm (repeat for each generated image)\n"
                "  --source-fps N (source metadata only) --output-dir PATH --disabled\n";
            return 0;
        }
        if(i+1>=argc)throw std::invalid_argument("Missing option value: "+arg);
        std::string value=argv[++i];
        if(arg=="--input")input=value;else if(arg=="--next")next=value;
        else if(arg=="--output-dir")output=value;
        else if(arg=="--multiplier")config.multiplier=std::stoi(value);
        else if(arg=="--repeats")repeats=std::stoi(value);
        else if(arg=="--threads")config.execution.threads=std::stoi(value);
        else if(arg=="--kernels") {if(value!="fast"&&value!="reference")throw std::invalid_argument("Use fast/reference");config.execution.kernels=value=="fast";}
        else if(arg=="--motion") {if(value!="early"&&value!="full")throw std::invalid_argument("Use early/full");config.execution.efficient_motion=value=="early";}
        else if(arg=="--simd"||arg=="--reuse"||arg=="--direct"||arg=="--vector-flow"||arg=="--integer-scale"||arg=="--cached-scale") {
            if(value!="on"&&value!="off")throw std::invalid_argument("Use on/off");
            if(arg=="--vector-flow"||arg=="--integer-scale"||arg=="--cached-scale")config.execution.vector_flow=value=="on";
            if(arg=="--simd")config.execution.simd=value=="on";
            if(arg=="--reuse")config.execution.reuse=value=="on";
            if(arg=="--direct"||arg=="--vector-flow"||arg=="--integer-scale"||arg=="--cached-scale")config.execution.direct=value=="on";
        }
        else if(arg=="--warp-iterations")config.warp_iterations=std::stoi(value);
        else if(arg=="--sharpness")config.sharpness=std::stoi(value);
        else if(arg=="--source-fps")config.source_interval_ms=1000/std::stod(value);
        else if(arg=="--process")config.processing=resolution(value);
        else if(arg=="--output")config.output=resolution(value);
        else if(arg=="--truth")truth.push_back(value);
        else if(arg=="--profile") {
            if(value!="performance"&&value!="quality"&&value!="balanced")throw std::invalid_argument("Invalid profile");
            config.profile=value=="quality"?fg::Profile::Quality:value=="balanced"?fg::Profile::Balanced:fg::Profile::Performance;
        } else if(arg=="--filter") {
            if(value!="area"&&value!="bilinear"&&value!="nearest"&&value!="bicubic")throw std::invalid_argument("Invalid filter");
            config.filter=value=="area"?fg::Filter::Area:value=="nearest"?fg::Filter::Nearest:value=="bicubic"?fg::Filter::Bicubic:fg::Filter::Bilinear;
        } else throw std::invalid_argument("Unknown option: "+arg);
    }
    if(repeats<1||repeats>100)throw std::invalid_argument("Repeats must be 1..100");
    if(!demo&&(input.empty()||next.empty()))throw std::invalid_argument("Provide --demo or --input and --next");
    if(demo&&(!input.empty()||!next.empty()||!truth.empty()))throw std::invalid_argument("Do not mix demo and captured frames");
    fg::Image a=demo?fg::test_scene({2560,1440},0):fg::read_ppm(input);
    fg::Image b=demo?fg::test_scene({2560,1440},16):fg::read_ppm(next);
    fg::validate(config,a.size());
    fg::Processor processor(config.execution);std::vector<double> totals;
    double prep=0,motion=0,interp=0,scaling=0;
    // Explicit warm-up. Measurements exclude decoding, generation, file writes and overlay.
    if(repeats>1)processor.run(a,b,config);
    for(int n=0;n<repeats;++n) {
        const auto& m=processor.run(a,b,config).metrics;
        totals.push_back(m.total_ms);prep+=m.preparation_ms;motion+=m.motion_ms;
        interp+=m.interpolation_ms;scaling+=m.scaling_ms;
    }
    auto batch=processor.take_result();
    if(!truth.empty()&&truth.size()!=std::size_t(batch.metrics.generated))
        throw std::invalid_argument("Supply exactly one truth image per generated frame");
    std::filesystem::create_directories(output);
    fg::write_ppm(a,output+"/input_a.ppm");fg::write_ppm(b,output+"/input_b.ppm");
    std::vector<double> quality,baseline;
    for(std::size_t i=0;i<batch.frames.size();++i) {
        fg::write_ppm(batch.frames[i],output+"/frame_"+std::to_string(i)+".ppm");
        if(i>0&&(demo||!truth.empty())) {
            auto target=demo?fg::test_scene(a.size(),16*batch.times[i]):fg::read_ppm(truth.at(i-1));
            if(target.size()!=a.size())throw std::invalid_argument("Truth dimensions differ from input");
            auto reference=fg::scale(target,batch.metrics.output,config.filter);
            fg::write_ppm(reference,output+"/truth_"+std::to_string(i)+".ppm");
            quality.push_back(fg::psnr(batch.frames[i],reference));
            baseline.push_back(fg::psnr(fg::scale(batch.times[i]<=.5?a:b,batch.metrics.output,config.filter),reference));
        }
    }
    auto final=fg::scale(b,batch.metrics.output,config.filter);
    fg::write_ppm(final,output+"/frame_"+std::to_string(batch.frames.size())+".ppm");
    fg::Menu menu;menu.visible=true;menu.config=config;fg::PresentationMonitor monitor;
    auto overlay=batch.frames[0];fg::draw_menu(overlay,menu,batch.metrics,monitor,"FILE SINK - NO DISPLAY FPS");
    fg::write_ppm(overlay,output+"/overlay.ppm");
    for(int section=0;section<5;++section){menu.section=section;auto view=batch.frames[0];fg::draw_menu(view,menu,batch.metrics,monitor,"HOST RASTER - CONSOLE UNVERIFIED");fg::write_ppm(view,output+"/overlay-section-"+std::to_string(section)+".ppm");}
    std::sort(totals.begin(),totals.end());const auto& m=batch.metrics;
    std::ofstream f(output+"/metrics.json");f<<std::setprecision(8);
    f<<"{\n  \"source\": \""<<(demo?"analytic_translation":"decoded_image_sequence")<<"\",\n  \"input\": ";dimension(f,m.input);
    f<<",\n  \"processing\": ";dimension(f,m.processing);f<<",\n  \"output_buffer\": ";dimension(f,m.output);
    f<<",\n  \"presented_surface\": null,\n  \"presented_fps\": null,\n  \"source_fps_measured\": null,\n"
        <<"  \"profile\": \""<<(config.profile==fg::Profile::Quality?"quality":config.profile==fg::Profile::Balanced?"balanced":"performance")<<"\",\n"
        <<"  \"filter\": \""<<(config.filter==fg::Filter::Area?"area":config.filter==fg::Filter::Nearest?"nearest":config.filter==fg::Filter::Bicubic?"bicubic":"bilinear")<<"\",\n"
        <<"  \"enabled\": "<<(config.enabled?"true":"false")<<",\n"
        <<"  \"kernels\": \""<<(config.execution.kernels?"fast":"reference")<<"\",\n"
        <<"  \"threads\": "<<config.execution.threads<<",\n  \"simd\": "<<(config.execution.simd?"true":"false")<<",\n"
        <<"  \"reuse\": "<<(config.execution.reuse?"true":"false")<<",\n  \"direct\": "<<(config.execution.direct?"true":"false")<<",\n"
        <<"  \"motion_early_exit\": "<<(config.execution.efficient_motion?"true":"false")<<",\n"
        <<"  \"image_allocations_last_pair\": "<<m.image_allocations<<",\n  \"copied_image_bytes_last_pair\": "<<m.copied_image_bytes<<",\n"
        <<"  \"multiplier\": "<<config.multiplier<<",\n  \"generated_frames\": "<<m.generated<<",\n"
        <<"  \"scene_cut\": "<<(m.scene_cut?"true":"false")<<",\n  \"repeats\": "<<repeats<<",\n"
        <<"  \"median_pair_ms\": "<<totals[totals.size()/2]<<",\n  \"min_pair_ms\": "<<totals.front()<<",\n  \"max_pair_ms\": "<<totals.back()<<",\n"
        <<"  \"preparation_mean_ms\": "<<prep/repeats<<",\n  \"motion_mean_ms\": "<<motion/repeats<<",\n"
        <<"  \"interpolation_mean_ms\": "<<interp/repeats<<",\n  \"output_scaling_mean_ms\": "<<scaling/repeats<<",\n"
        <<"  \"image_buffer_bytes\": "<<m.image_bytes<<",\n  \"working_buffer_bound_bytes\": "<<m.working_set_upper_bound<<",\n"
        <<"  \"source_interval_metadata_ms\": "<<config.source_interval_ms<<",\n  \"estimated_lookahead_ms\": "<<m.lookahead_ms<<",\n"
        <<"  \"estimated_pipeline_latency_ms\": "<<m.latency_estimate_ms<<",\n  \"measured_end_to_end_latency_ms\": null,\n"
        <<"  \"over_budget\": "<<(m.overloaded?"true":"false")<<",\n  \"quality\": [";
    for(std::size_t i=0;i<quality.size();++i) {
        if(i)f<<',';f<<"{\"t\":"<<batch.times[i+1]<<",\"psnr_db\":";number(f,quality[i]);
        f<<",\"nearest_original_psnr_db\":";number(f,baseline[i]);f<<'}';
    }
    f<<"]\n}\n";f.close();if(!f)throw std::runtime_error("Cannot write metrics");
    std::cout<<"Generated "<<m.generated<<" intermediate image(s), "<<m.output.width<<'x'<<m.output.height
        <<", median pair "<<totals[totals.size()/2]<<" ms; no display presentation measured.\n";
    return 0;
 } catch(const std::exception& e){std::cerr<<"framegen: "<<e.what()<<'\n';return 1;}
}
