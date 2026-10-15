#include "engine/framegen.hpp"
#include <cmath>
#include <stdexcept>

namespace fg {
double elapsed_ms(Clock::time_point start) {return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void validate(const Config& c,Size input) {
    checked_pixels(input);
    if(c.execution.threads<1||c.execution.threads>8)throw std::invalid_argument("Worker count must be 1..8");
    if(int(c.output)<0||int(c.output)>2)throw std::invalid_argument("540p and 360p are processing resolutions only");
    if(c.multiplier<2||c.multiplier>4)throw std::invalid_argument("Multiplier must be 2, 3 or 4");
    if(c.profile!=Profile::Performance&&c.profile!=Profile::Quality&&c.profile!=Profile::Balanced)throw std::invalid_argument("Unknown profile");
    if(c.filter!=Filter::Area&&c.filter!=Filter::Bilinear&&c.filter!=Filter::Nearest&&c.filter!=Filter::Bicubic)throw std::invalid_argument("Unknown filter");
    if(c.warp_iterations<1||c.warp_iterations>2)throw std::invalid_argument("Warp iterations must be 1 or 2");
    if(c.sharpness<0||c.sharpness>100)throw std::invalid_argument("Sharpness must be 0..100");
    if(c.fps_limit!=0&&c.fps_limit!=30&&c.fps_limit!=60)throw std::invalid_argument("Presentation cap must be off, 30 or 60");
    if(!std::isfinite(c.source_interval_ms)||c.source_interval_ms<=0)throw std::invalid_argument("Invalid source interval");
    auto out=resolve(c.output,input),proc=resolve(c.processing,input);
    if(c.enabled&&(out.width>input.width||out.height>input.height||proc.width>input.width||proc.height>input.height))
        throw std::invalid_argument("Requested resolution exceeds input; upscaling is not downscaling");
}
Batch process_reference(const Image& a,const Image& b,const Config& c) {
    if(a.size()!=b.size())throw std::invalid_argument("Input frame dimensions changed; reset capture");
    validate(c,a.size());auto activity=image_activity();Batch result;auto begin=Clock::now();auto& met=result.metrics;
    met.input=a.size();met.processing=c.enabled?resolve(c.processing,a.size()):a.size();
    met.output=c.enabled?resolve(c.output,a.size()):a.size();
    auto start=Clock::now();
    Image pa=scale(a,met.processing,c.filter),pb=scale(b,met.processing,c.filter);
    met.preparation_ms=elapsed_ms(start);
    int count=c.enabled?c.multiplier:1;Motion motion;
    if(c.enabled) {start=Clock::now();motion=estimate(pa,pb,c.profile);met.motion_ms=elapsed_ms(start);}
    met.scene_cut=c.enabled&&motion.scene_cut;
    if(met.scene_cut)count=1; // No generated images or invented cadence across scene cuts.
    result.frames.reserve(std::size_t(count));result.times.reserve(std::size_t(count));
    for(int i=0;i<count;++i) {
        float t=float(i)/count;start=Clock::now();
        Image image=i==0?pa:interpolate(pa,pb,motion,t);
        met.interpolation_ms+=elapsed_ms(start);start=Clock::now();
        result.frames.push_back(scale(image,met.output,c.filter));
        met.scaling_ms+=elapsed_ms(start);result.times.push_back(t);
    }
    met.generated=count-1;met.total_ms=elapsed_ms(begin);
    met.lookahead_ms=c.enabled?c.source_interval_ms:0;
    met.latency_estimate_ms=met.lookahead_ms+met.total_ms;
    met.image_bytes=a.bytes()+b.bytes()+pa.bytes()+pb.bytes();
    for(const auto& image:result.frames)met.image_bytes+=image.bytes();
    met.working_set_upper_bound=met.image_bytes+3*pa.bytes()+4*320u*2160u*sizeof(float)+
        (motion.forward.vectors.size()+motion.backward.vectors.size())*sizeof(Vector);
    met.overloaded=c.enabled&&met.total_ms>c.source_interval_ms;
    auto final_activity=image_activity();met.image_allocations=final_activity.allocations-activity.allocations;
    met.copied_image_bytes=final_activity.copied_bytes-activity.copied_bytes;
    return result;
}
}
