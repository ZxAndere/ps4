#include "engine/runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
 try {
    if(argc!=5)throw std::runtime_error("framegen_replay A.ppm B.ppm NEW_DIRECTORY pairs");
    int pairs=std::stoi(argv[4]);if(pairs<8||pairs>500)throw std::runtime_error("Use 8..500 pairs");
    if(!std::filesystem::create_directory(argv[3]))throw std::runtime_error("Use a new report directory");
    auto a=fg::read_ppm(argv[1]),b=fg::read_ppm(argv[2]);std::string dir=argv[3];
    for(bool low:{false,true}) {
        fg::Config c;c.enabled=true;c.automatic=true;c.low_latency=low;c.multiplier=4;c.processing=fg::Resolution::P720;c.output=fg::Resolution::P720;
        c.source_interval_ms=1000.0/30;fg::Runtime runtime;fg::PresentationMonitor monitor;uint64_t dropped=0;
        fg::Diagnostics diagnostics(dir+(low?"/low-latency.jsonl":"/normal.jsonl"));
        std::ofstream trace(dir+(low?"/low-schedule.jsonl":"/normal-schedule.jsonl"));
        for(int i=0;i<pairs;++i) {
            double captured=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();monitor.original(captured);
            auto& result=runtime.run(a,b,c);double completed=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();
            fg::PresentationSchedule schedule(captured,completed,c.source_interval_ms,low,0);
            dropped+=result.metrics.dropped;int stale=0;
            for(std::size_t j=1;j<result.frames.size();++j)if(schedule.expired(true,result.times[j],runtime.effective().multiplier,completed))++stale;
            dropped+=stale;diagnostics.record(result.metrics,monitor,dropped,runtime.effective(),runtime.adaptive().pressure());
            // These are readiness/deadline observations, not measurements of a display or input latency.
            trace<<std::setprecision(10)<<"{\"pair\":"<<i<<",\"cpu_ms\":"<<result.metrics.total_ms<<",\"bypassed\":"<<!runtime.effective().enabled
              <<",\"stale_at_completion\":"<<stale<<",\"estimated_original_age_at_ready_ms\":"
              <<(runtime.effective().enabled?c.source_interval_ms+result.metrics.total_ms:result.metrics.total_ms)<<",\"presented_fps\":null}\n";
        }
    }
    std::cout<<"Measured offline pair replay and deadline readiness. No display flips or input-to-photon measurement.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
