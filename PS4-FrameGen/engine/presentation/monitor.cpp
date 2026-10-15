#include "engine/framegen.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace fg {
void PresentationMonitor::original(double s) {
    if(!std::isfinite(s)||(originals_&&s<=last_original_))throw std::invalid_argument("Non-monotonic capture timestamp");
    if(!originals_)first_original_=s;
    last_original_=s;++originals_;
}
void PresentationMonitor::confirmed_present(double s) {
    if(!std::isfinite(s)||(presented_&&s<=last_presented_))throw std::invalid_argument("Non-monotonic flip timestamp");
    if(presented_) {
        if(frame_ms_.size()==240)frame_ms_.erase(frame_ms_.begin());
        frame_ms_.push_back((s-last_presented_)*1000);
    }
    if(!presented_)first_presented_=s;
    last_presented_=s;++presented_;
}
double PresentationMonitor::original_fps() const {return originals_<2?0:double(originals_-1)/(last_original_-first_original_);}
double PresentationMonitor::presented_fps() const {return presented_<2?0:double(presented_-1)/(last_presented_-first_presented_);}
PresentationMonitor::FrameTimes PresentationMonitor::frame_times() const {
    FrameTimes out;out.samples=frame_ms_.size();if(!out.samples)return out;
    auto sorted=frame_ms_;std::sort(sorted.begin(),sorted.end());
    for(double value:sorted)out.mean+=value;
    out.mean/=sorted.size();out.maximum=sorted.back();
    auto q=[&](double p){return sorted[std::size_t(std::ceil(p*sorted.size()))-1];};
    out.p50=q(.5);out.p95=q(.95);out.p99=q(.99);return out;
}

}
