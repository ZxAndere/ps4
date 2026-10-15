// Experimental standalone application. No hooks, game patches, or firmware writes.
#include "engine/framegen.hpp"
#include "engine/runtime.hpp"
#include "overlay/overlay.hpp"
#include "goldhen/plugin/bridge.h"
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#include <sstream>
#include <algorithm>

namespace {
constexpr fg::Size native={1920,1080};
constexpr std::size_t alignment=0x200000;
constexpr std::size_t slot=(std::size_t(native.width)*native.height*4+alignment-1)/alignment*alignment;
void wait_until(double deadline) {
    for(;;){double now=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();
        if(now>=deadline)return;
        sceKernelUsleep(unsigned(std::min(2000.0,std::max(1.0,(deadline-now)*1000000))));}
}
double now_seconds(){return std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();}
void require(int rc,const char* operation) {
    if(rc<0){char message[160];std::snprintf(message,sizeof(message),"%s failed: 0x%08x",operation,unsigned(rc));throw std::runtime_error(message);}
}
class Display {
    void* memory_=nullptr;
    off_t offset_=0;
    int handle_=-1,index_=0;
    int64_t serial_=0,pending_=-1;
    fg::Size size_;
    bool mapped_=false,allocated_=false,valid_=false;
    void open(fg::Size s) {
        handle_=sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN,ORBIS_VIDEO_OUT_BUS_MAIN,0,nullptr);
        require(handle_,"VideoOutOpen");
        void* buffers[2]={memory_,static_cast<char*>(memory_)+slot};
        OrbisVideoOutBufferAttribute attribute={};
        sceVideoOutSetBufferAttribute(&attribute,ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB,
            ORBIS_VIDEO_OUT_TILING_MODE_LINEAR,ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9,s.width,s.height,s.width);
        require(sceVideoOutRegisterBuffers(handle_,0,buffers,2,&attribute),"RegisterBuffers");
        require(sceVideoOutSetFlipRate(handle_,ORBIS_VIDEO_OUT_FLIP_60HZ),"SetFlipRate");
        size_=s;index_=0;
        OrbisVideoOutResolutionStatus status={};
        require(sceVideoOutGetResolutionStatus(handle_,&status),"GetResolutionStatus");
        std::printf("SURFACE %dx%d PITCH %d RGBA8; DISPLAY MODE %ux%u refreshRaw=%llu\n",
            s.width,s.height,s.width,status.width,status.height,(unsigned long long)status.refreshRate);
        valid_=true;
    }
    void wait() {
        if(pending_<0)return;
        auto start=fg::Clock::now();
        for(;;) {
            OrbisVideoOutFlipStatus status={};
            require(sceVideoOutGetFlipStatus(handle_,&status),"GetFlipStatus");
            if(status.flipArg==pending_&&status.num>0){pending_=-1;return;}
            if(fg::elapsed_ms(start)>3000)throw std::runtime_error("Flip timeout; stopping presentation");
            sceKernelUsleep(500);
        }
    }
public:
    Display() {
        require(sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),2*slot,alignment,3,&offset_),"AllocateDirectMemory");
        allocated_=true;
        int rc=sceKernelMapDirectMemory(&memory_,2*slot,0x33,0,offset_,alignment);
        if(rc<0){sceKernelReleaseDirectMemory(offset_,2*slot);allocated_=false;require(rc,"MapDirectMemory");}
        mapped_=true;
    }
    ~Display() {
        // Never unmap a surface if the driver did not confirm handle closure.
        if(handle_>=0&&sceVideoOutClose(handle_)<0){std::puts("Video close failed; memory retained until process exit");return;}
        if(mapped_)sceKernelMunmap(memory_,2*slot);
        if(allocated_)sceKernelReleaseDirectMemory(offset_,2*slot);
    }
    fg::Size size()const{return size_;}
    void resize(fg::Size s) {
        fg::checked_pixels(s);
        if(s!=native&&s!=(fg::Size{1280,720}))throw std::invalid_argument("Unsupported display surface");
        if(handle_>=0&&valid_&&size_==s)return;
        if(handle_>=0) {wait();require(sceVideoOutClose(handle_),"Close before resize");handle_=-1;}
        valid_=false;
        // Storage remains allocated at full capacity across re-registration; no buffer is freed during resize.
        // A rejection is fatal to this transaction. The caller can restore native via another resize.
        try {open(s);}catch(...) {
            if(handle_>=0){int rc=sceVideoOutClose(handle_);if(rc>=0)handle_=-1;}
            throw;
        }
    }
    void present_pixels(const void* pixels,fg::Size size,fg::PresentationMonitor& monitor) {
        auto bytes=fg::checked_pixels(size)*4;
        if(!pixels||handle_<0||!valid_||size!=size_||bytes>slot)throw std::runtime_error("Surface/buffer mismatch");
        wait();
        void* destination=static_cast<char*>(memory_)+std::size_t(index_)*slot;
        std::memcpy(destination,pixels,bytes);
        pending_=++serial_;
        require(sceVideoOutSubmitFlip(handle_,index_,ORBIS_VIDEO_OUT_FLIP_VSYNC,pending_),"SubmitFlip");
        wait();monitor.confirmed_present(now_seconds());index_=1-index_;
    }
    void present(const fg::Image& im,fg::PresentationMonitor& monitor){present_pixels(im.data(),im.size(),monitor);}
};
class Controller {
    int pad_=-1;
public:
    Controller() {
        OrbisUserServiceInitializeParams parameters={};parameters.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
        require(sceUserServiceInitialize(&parameters),"UserServiceInitialize");
        int user=0;require(sceUserServiceGetInitialUser(&user),"GetInitialUser");
        require(scePadInit(),"PadInit");pad_=scePadOpen(user,0,0,nullptr);require(pad_,"PadOpen");
    }
    ~Controller(){if(pad_>=0)scePadClose(pad_);}
    uint32_t read(bool& quit) {
        OrbisPadData data={};require(scePadReadState(pad_,&data),"PadReadState");
        if(!data.connected)return 0;
        quit=(data.buttons&ORBIS_PAD_BUTTON_OPTIONS)!=0;
        const uint32_t masks[]={ORBIS_PAD_BUTTON_L3,ORBIS_PAD_BUTTON_R3,ORBIS_PAD_BUTTON_UP,ORBIS_PAD_BUTTON_DOWN,
            ORBIS_PAD_BUTTON_LEFT,ORBIS_PAD_BUTTON_RIGHT,ORBIS_PAD_BUTTON_CROSS,ORBIS_PAD_BUTTON_CIRCLE,ORBIS_PAD_BUTTON_L1,ORBIS_PAD_BUTTON_R1,ORBIS_PAD_BUTTON_TRIANGLE};
        uint32_t result=0;for(int i=0;i<11;++i)if(data.buttons&masks[i])result|=1u<<i;
        return result;
    }
};
class CooperativeBridge {
    int module_=-1;const FGBridgeAPI* api_=nullptr;FGContext* context_=nullptr;
    Display& display_;fg::PresentationMonitor& monitor_;
    double last_flip_=0;int fps_limit_=0;fg::Diagnostics& diagnostics_;bool logging_=false;
    static int present(void* user,const FGOutput* output,double* confirmed) {
        auto& self=*static_cast<CooperativeBridge*>(user);
        try {
            fg::Size size{int(output->width),int(output->height)};
            if(output->pitch!=output->width*4)return FG_CALLBACK_ERROR;
            if(output->due_seconds>0)wait_until(fg::PresentationSchedule::limited_due(output->due_seconds,self.last_flip_,self.fps_limit_));
            if(output->generated&&output->expires_seconds>0&&now_seconds()>=output->expires_seconds)return FG_DROPPED_BY_CONSUMER;
            self.display_.resize(size);self.display_.present_pixels(output->rgba,size,self.monitor_);
            *confirmed=now_seconds();self.last_flip_=*confirmed;return FG_CONFIRMED_FLIP;
        }catch(const std::exception& e){std::puts(e.what());return FG_CALLBACK_ERROR;}
    }
public:
    CooperativeBridge(Display& display,fg::PresentationMonitor& monitor,const fg::Config& config,fg::Diagnostics& diagnostics):display_(display),monitor_(monitor),diagnostics_(diagnostics) {
        if(config.safe_mode)return;
        FILE* disabled=std::fopen("/data/PS4FrameGen/disable_cooperative_prx_v03","r");
        if(disabled){std::fclose(disabled);return;}
        FILE* opt_in=std::fopen("/data/PS4FrameGen/enable_cooperative_prx_v03","r");
        if(!opt_in)return;
        std::fclose(opt_in);int start_result=0;
        module_=int(sceKernelLoadStartModule("/app0/framegen_bridge.prx",0,nullptr,0,nullptr,&start_result));
        if(module_<0||start_result<0){std::puts("PRX load rejected; using built-in engine");return;}
        void* symbol=nullptr;
        if(sceKernelDlsym(module_,"fg_bridge_api",&symbol)<0||!symbol){std::puts("PRX API missing; using built-in engine");return;}
        using Query=const FGBridgeAPI* (*)(uint32_t);api_=reinterpret_cast<Query>(symbol)(FG_BRIDGE_ABI);
        if(!api_||api_->size!=sizeof(FGBridgeAPI)||api_->game_hooks||!api_->cooperative_capture){api_=nullptr;return;}
        context_=api_->create(uint32_t(config.execution.threads));
        if(context_) {
            FGConfiguration c{0,uint32_t(config.multiplier),uint32_t(config.profile),uint32_t(config.processing),uint32_t(config.output),
                uint32_t(config.automatic),uint32_t(config.low_latency),uint32_t(config.filter),uint32_t(config.sharpness),
                uint32_t(config.fps_limit),uint32_t(config.scene_guard),uint32_t(config.safe_mode),uint32_t(config.diagnostics),33333};
            if(api_->configure(context_,&c)<0){api_->destroy(context_);context_=nullptr;}
        }
        if(context_)std::puts("COOPERATIVE PRX CONNECTED - supplied frames and callbacks; no game hooks");
    }
    ~CooperativeBridge() { disable(); }
    void disable() {
        if(context_&&api_->destroy(context_)!=0){std::puts("PRX busy: retained until process exit");return;}
        if(module_>=0){int result=0;sceKernelStopUnloadModule(module_,0,nullptr,0,nullptr,&result);}
        context_=nullptr;api_=nullptr;module_=-1;
    }
    bool active()const{return context_!=nullptr;}
    void pad(uint32_t buttons,fg::Config& config) {
        require(api_->pad(context_,buttons),"Cooperative pad");FGSnapshot snapshot{};
        require(api_->snapshot(context_,&snapshot),"Cooperative state");const auto& s=snapshot.configuration;
        config.enabled=s.enabled!=0;config.multiplier=int(s.multiplier);config.profile=fg::Profile(s.profile);
        config.processing=fg::Resolution(s.processing);config.output=fg::Resolution(s.output);
        config.automatic=s.automatic;config.low_latency=s.low_latency;config.filter=fg::Filter(s.filter);config.sharpness=int(s.sharpness);
        config.fps_limit=int(s.fps_limit);config.scene_guard=s.scene_guard;config.safe_mode=s.safe_mode;fps_limit_=config.fps_limit;config.diagnostics=s.diagnostics;logging_=s.diagnostics;
        if(config.safe_mode) {
            FILE* block=std::fopen("/data/PS4FrameGen/disable_cooperative_prx_v03","w");if(block){std::fputs("Manual recovery required\n",block);std::fclose(block);}
            disable();
        }
    }
    void capture(const fg::Image& image,double timestamp) {
        auto size=image.size();FGInput input{image.data(),image.bytes(),uint32_t(size.width),uint32_t(size.height),uint32_t(size.width*4),
            FG_RGBA8_LINEAR,FG_CPU_READY|FG_RETAIN_UNTIL_RETURN|FG_CLOCK_MONOTONIC,timestamp};
        require(api_->capture(context_,&input,present,this),"Cooperative capture/presentation");
        FGSnapshot s{};require(api_->snapshot(context_,&s),"Cooperative telemetry");
        if(logging_&&s.output_width>0) {
            fg::Metrics metrics;metrics.input=size;metrics.processing={int(s.processing_width),int(s.processing_height)};
            metrics.output={int(s.output_width),int(s.output_height)};metrics.total_ms=s.cpu_ms;metrics.interpolation_ms=s.interpolation_ms;
            metrics.scaling_ms=s.scaling_ms;metrics.latency_estimate_ms=s.latency_estimate_ms;metrics.image_bytes=s.rgba_buffer_bytes;metrics.skipped_reason=int(s.skipped_reason);
            fg::Config effective;effective.enabled=s.configuration.enabled&&!s.configuration.safe_mode;effective.multiplier=int(s.effective_multiplier?s.effective_multiplier:2);
            diagnostics_.record(metrics,monitor_,s.dropped,effective,int(s.adaptive_pressure));
        }
        std::printf("PRX captured=%llu delivered=%llu confirmed=%llu cpu_ms=%.3f bridge_ms=%.3f proc=%ux%u output=%ux%u\n",
            (unsigned long long)s.captured,(unsigned long long)s.delivered,(unsigned long long)s.confirmed,s.cpu_ms,s.total_bridge_ms,
            s.processing_width,s.processing_height,s.output_width,s.output_height);
    }
};
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("PS4 FrameGen 0.3 optimized CPU app - no game injection");
    try {
        mkdir("/data/PS4FrameGen",0700);
        const std::string directory="/data/PS4FrameGen/v03";
        mkdir(directory.c_str(),0700);
        fg::Menu menu;menu.visible=true;
        fg::GameProfile profile;profile.game_id="FGEN00003";
        try{profile=fg::recover_profile(directory,profile.game_id);menu.config=profile.config;menu.compatibility=fg::compatibility_name(profile.state);}
        catch(const std::exception& e){std::puts(e.what());}
        fg::RecoverySession recovery(directory);recovery.begin(menu.config);
        fg::Diagnostics diagnostics(directory+"/diagnostics.jsonl");
        menu.config.enabled=false; // Every new process requires manual activation.
        Display display;display.resize(native);Controller controller;fg::PresentationMonitor monitor;
        fg::Runtime processor(menu.config.execution);CooperativeBridge bridge(display,monitor,menu.config,diagnostics);
        fg::Image previous=fg::test_scene(native,0);
        double last_capture=now_seconds();monitor.original(last_capture);
        float position=0;bool quit=false;std::string configuration;uint64_t dropped=0;double last_flip=0;
        std::string status="CPU OPTIMIZED - HARDWARE PERFORMANCE UNVERIFIED";
        while(!quit) {
            auto buttons=controller.read(quit);if(quit)break;
            if(bridge.active())bridge.pad(buttons,menu.config);else menu.update(buttons);
            if(menu.restore_previous) {
                menu.restore_previous=false;
                try{profile=fg::previous_profile(directory,profile.game_id);menu.config=profile.config;menu.compatibility=fg::compatibility_name(profile.state);}
                catch(const std::exception& e){status=e.what();}
            }
            auto& c=menu.config;
            std::ostringstream signature;
            signature<<c.multiplier<<int(c.profile)<<int(c.output)<<int(c.processing)<<int(c.filter)<<c.sharpness<<c.automatic<<c.low_latency<<c.fps_limit<<c.scene_guard<<c.diagnostics;
            if(signature.str()!=configuration) {
                try {profile.config=c;fg::save_profile(profile,directory);configuration=signature.str();}
                catch(const std::exception& e){status=e.what();std::puts(e.what());}
            }
            position+=16;
            auto current=fg::test_scene(native,position);
            double capture=now_seconds();c.source_interval_ms=(capture-last_capture)*1000;
            last_capture=capture;monitor.original(capture);
            if(bridge.active()) {
                try{bridge.capture(current,capture);previous=std::move(current);continue;}
                catch(const std::exception& e){c.enabled=false;c.safe_mode=true;recovery.fault(e.what());bridge.disable();status=e.what();}
            }
            try {
                // Controller budget is the declared 30-Hz source period, not the self-throttled loop time.
                auto requested=c;requested.source_interval_ms=1000.0/30;
                auto& batch=processor.run(previous,current,requested);
                menu.pressure=processor.adaptive().pressure();dropped+=batch.metrics.dropped;
                batch.metrics.lookahead_ms=processor.effective().enabled?c.source_interval_ms:0;
                batch.metrics.latency_estimate_ms=batch.metrics.lookahead_ms+batch.metrics.total_ms;
                // Evaluate the experiment against a declared 30-Hz source budget as well as observed cadence.
                batch.metrics.overloaded=batch.metrics.overloaded||batch.metrics.total_ms>1000.0/30;
                display.resize(batch.metrics.output);
                fg::PresentationSchedule schedule(capture,now_seconds(),requested.source_interval_ms,c.low_latency,c.fps_limit);
                auto original=menu.comparison?fg::scale(previous,batch.metrics.output,c.filter):fg::Image({1,1});
                for(std::size_t i=0;i<batch.frames.size();++i) {
                    auto& frame=batch.frames[i];double due=fg::PresentationSchedule::limited_due(schedule.due(batch.times[i]),last_flip,c.fps_limit);
                    wait_until(due);
                    if(schedule.expired(i>0,batch.times[i],processor.effective().multiplier,now_seconds())){++dropped;continue;}
                    if(menu.comparison&&i>0)frame=fg::comparison_panel(original,frame);
                    menu.dropped=dropped;fg::draw_menu(frame,menu,batch.metrics,monitor,status);
                    display.present(frame,monitor);last_flip=now_seconds();
                }
                if(c.diagnostics)diagnostics.record(batch.metrics,monitor,dropped,processor.effective(),menu.pressure);
                std::printf("FRAME input=%dx%d proc=%dx%d surface=%dx%d gen=%d cpu_ms=%.3f scale_ms=%.3f original_fps=%.3f confirmed_flip_fps=%.3f lookahead_est_ms=%.3f buffers=%zu over=%d\n",
                    native.width,native.height,batch.metrics.processing.width,batch.metrics.processing.height,
                    display.size().width,display.size().height,batch.metrics.generated,batch.metrics.total_ms,
                    batch.metrics.scaling_ms,monitor.original_fps(),monitor.presented_fps(),batch.metrics.lookahead_ms,
                    batch.metrics.image_bytes,int(batch.metrics.overloaded));
            } catch(const std::exception& e) {
                // Fail closed. Restore the original registration before showing another image.
                c.enabled=false;c.safe_mode=true;recovery.fault(e.what());status=e.what();std::puts(e.what());display.resize(native);
                auto fallback=current;fg::Metrics metrics;metrics.input=metrics.processing=metrics.output=native;
                fg::draw_menu(fallback,menu,metrics,monitor,status);display.present(fallback,monitor);
            }
            previous=std::move(current);
        }
        display.resize(native);
        recovery.finish();
        std::puts("Engine disabled, original surface restored; exiting");
        return 0;
    }catch(const std::exception& e){
        FILE* fault=std::fopen("/data/PS4FrameGen/v03/faults-v03.log","a");
        if(fault){std::fprintf(fault,"FATAL %s\n",e.what());std::fclose(fault);}
        std::printf("FATAL %s\n",e.what());return 1;
    }
}
