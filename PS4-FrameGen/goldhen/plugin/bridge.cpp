#include "goldhen/plugin/bridge.h"
#include "engine/framegen.hpp"
#include "engine/runtime.hpp"
#include "overlay/overlay.hpp"
#include <atomic>
#include <cmath>
#include <cstring>

static std::atomic<unsigned> instances{0};
struct FGContext {
    std::atomic_flag busy=ATOMIC_FLAG_INIT;
    fg::Menu menu;
    fg::Runtime processor;
    fg::Image previous{{1,1}},current{{1,1}};
    fg::PresentationMonitor monitor;
    FGSnapshot state{};
    bool initialized=false;
    double last_timestamp=0;uint32_t source_budget_us=0;
    explicit FGContext(unsigned threads):processor([&]{fg::Execution e;e.threads=int(threads);return e;}()) {
        menu.config.execution.threads=int(threads);menu.profile_restore_available=false;
    }
};
struct BusyGuard {
    FGContext& context;bool acquired;
    explicit BusyGuard(FGContext& c):context(c),acquired(!c.busy.test_and_set(std::memory_order_acquire)){}
    ~BusyGuard(){if(acquired)context.busy.clear(std::memory_order_release);}
    explicit operator bool()const{return acquired;}
};
static FGConfiguration configuration(const fg::Config& c) {
    return {uint32_t(c.enabled),uint32_t(c.multiplier),uint32_t(c.profile),uint32_t(c.processing),uint32_t(c.output),uint32_t(c.automatic),uint32_t(c.low_latency),uint32_t(c.filter),uint32_t(c.sharpness),
        uint32_t(c.fps_limit),uint32_t(c.scene_guard),uint32_t(c.safe_mode),uint32_t(c.diagnostics)};
}
static FGContext* create(uint32_t threads) {
    try {auto* context=new FGContext(threads);++instances;return context;}catch(...){return nullptr;}
}
static int destroy(FGContext* context) {
    if(!context)return FG_BAD_ARGUMENT;
    // Caller must stop issuing new calls before destruction/unload.
    if(context->busy.test_and_set(std::memory_order_acquire))return FG_BUSY;
    delete context;--instances;return 0;
}
static int configure(FGContext* context,const FGConfiguration* value) {
    if(!context||!value)return FG_BAD_ARGUMENT;
    BusyGuard lock(*context);if(!lock)return FG_BUSY;
    if(value->enabled>1||value->multiplier<2||value->multiplier>4||value->profile>2||value->processing>4||value->output>2||value->automatic>1||value->low_latency>1||value->filter>3||value->sharpness>100||
       value->scene_guard>1||value->safe_mode>1||value->diagnostics>1||value->source_budget_us>1000000)return FG_BAD_ARGUMENT;
    auto candidate=context->menu.config;candidate.enabled=value->enabled!=0;candidate.multiplier=int(value->multiplier);
    candidate.profile=fg::Profile(value->profile);candidate.processing=fg::Resolution(value->processing);candidate.output=fg::Resolution(value->output);
    candidate.automatic=value->automatic;candidate.low_latency=value->low_latency;candidate.filter=fg::Filter(value->filter);
    candidate.sharpness=int(value->sharpness);candidate.fps_limit=int(value->fps_limit);candidate.scene_guard=value->scene_guard;candidate.safe_mode=value->safe_mode;candidate.diagnostics=value->diagnostics;
    if(candidate.safe_mode)candidate.enabled=false;
    try {fg::validate(candidate,context->initialized?context->previous.size():fg::Size{1920,1080});}
    catch(...){return FG_BAD_ARGUMENT;}
    context->menu.config=candidate;context->source_budget_us=value->source_budget_us;return 0;
}
static int pad(FGContext* context,uint32_t buttons) {
    if(!context)return FG_BAD_ARGUMENT;
    BusyGuard lock(*context);if(!lock)return FG_BUSY;
    context->menu.update(buttons);return 0;
}
static int snapshot(FGContext* context,FGSnapshot* out) {
    if(!context||!out)return FG_BAD_ARGUMENT;
    BusyGuard lock(*context);if(!lock)return FG_BUSY;
    *out=context->state;out->configuration.source_budget_us=context->source_budget_us;out->menu_visible=context->menu.visible;out->configuration=configuration(context->menu.config);out->configuration.source_budget_us=context->source_budget_us;return 0;
}
static int capture(FGContext* context,const FGInput* input,FGPresent present,void* user) {
    if(!context||!input||!present)return FG_BAD_ARGUMENT;
    BusyGuard lock(*context);if(!lock)return FG_BUSY;
    auto begin=fg::Clock::now();
    auto fail=[&](int code){context->state.last_error=code;context->menu.config.enabled=false;return code;};
    try {
        if(!input->rgba||input->width>4096||input->height>2160||!input->width||!input->height||
            !std::isfinite(input->captured_seconds)||
            (context->initialized&&input->captured_seconds<=context->last_timestamp))return fail(FG_BAD_ARGUMENT);
        if(input->format!=FG_RGBA8_LINEAR||(input->flags&(FG_CPU_READY|FG_RETAIN_UNTIL_RETURN))!=(FG_CPU_READY|FG_RETAIN_UNTIL_RETURN)||
            input->pitch<uint64_t(input->width)*4||
            input->capacity<uint64_t(input->height-1)*input->pitch+uint64_t(input->width)*4)return fail(FG_UNSUPPORTED_SURFACE);
        fg::Size size{int(input->width),int(input->height)};fg::checked_pixels(size);
        context->current.reset(size);
        for(unsigned y=0;y<input->height;++y)
            std::memcpy(context->current.data()+std::size_t(y)*size.width,static_cast<const char*>(input->rgba)+std::size_t(y)*input->pitch,std::size_t(size.width)*4);
        context->monitor.original(input->captured_seconds);++context->state.captured;
        if(!context->initialized||context->previous.size()!=size) {
            std::swap(context->previous,context->current);context->initialized=true;context->last_timestamp=input->captured_seconds;
            context->state.input_width=input->width;context->state.input_height=input->height;return 1;
        }
        auto& c=context->menu.config;double actual_interval_ms=(input->captured_seconds-context->last_timestamp)*1000;
        c.source_interval_ms=context->source_budget_us?context->source_budget_us/1000.0:actual_interval_ms;
        auto& batch=context->processor.run(context->previous,context->current,c);
        batch.metrics.lookahead_ms=context->processor.effective().enabled?actual_interval_ms:0;
        batch.metrics.latency_estimate_ms=batch.metrics.lookahead_ms+batch.metrics.total_ms;
        auto& s=context->state;s.input_width=input->width;s.input_height=input->height;
        s.processing_width=uint32_t(batch.metrics.processing.width);s.processing_height=uint32_t(batch.metrics.processing.height);
        s.output_width=uint32_t(batch.metrics.output.width);s.output_height=uint32_t(batch.metrics.output.height);s.cpu_ms=batch.metrics.total_ms;
        const auto& effective=context->processor.effective();
        s.adaptive_pressure=uint32_t(context->processor.adaptive().pressure());s.effective_multiplier=uint32_t(effective.multiplier);
        s.skipped_reason=uint32_t(batch.metrics.skipped_reason);s.dropped+=batch.metrics.dropped;s.rgba_buffer_bytes=batch.metrics.image_bytes;
        s.interpolation_ms=batch.metrics.interpolation_ms;s.scaling_ms=batch.metrics.scaling_ms;s.latency_estimate_ms=batch.metrics.latency_estimate_ms;
        const bool live=(input->flags&FG_CLOCK_MONOTONIC)!=0;
        double completed=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();
        if(live&&(input->captured_seconds>completed||completed-input->captured_seconds>10))return fail(FG_BAD_ARGUMENT);
        fg::PresentationSchedule schedule(live?input->captured_seconds:completed,completed,c.source_interval_ms,c.low_latency,c.fps_limit);
        s.deadline_available=live;
        context->menu.dropped=s.dropped;context->menu.pressure=int(s.adaptive_pressure);
        for(std::size_t i=0;i<batch.frames.size();++i) {
            double now=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();
            if(live&&schedule.expired(i>0,batch.times[i],effective.multiplier,now)){++s.dropped;continue;}
            auto& frame=batch.frames[i];
            if(context->menu.comparison&&i>0){auto original=fg::scale(context->previous,frame.size(),c.filter);frame=fg::comparison_panel(original,frame);}
            context->menu.dropped=s.dropped;
            fg::draw_menu(frame,context->menu,batch.metrics,context->monitor,"COOPERATIVE PRX - NO GAME HOOKS");
            FGOutput output{frame.data(),uint32_t(frame.size().width),uint32_t(frame.size().height),uint32_t(frame.size().width*4),uint32_t(i>0),batch.times[i],
                live?schedule.due(batch.times[i]):0,live?schedule.due(batch.times[i])+c.source_interval_ms/(1000*effective.multiplier):0};
            double confirmed=0;int result=present(user,&output,&confirmed);
            if(result==FG_DROPPED_BY_CONSUMER){if(!i)return fail(FG_CALLBACK_ERROR);++s.dropped;continue;}
            if(result!=FG_CONSUMED_OFFLINE&&result!=FG_CONFIRMED_FLIP)return fail(FG_CALLBACK_ERROR);
            ++s.delivered;
            if(result==FG_CONFIRMED_FLIP){context->monitor.confirmed_present(confirmed);s.confirmed=context->monitor.presented_count();}
        }
        std::swap(context->previous,context->current);context->last_timestamp=input->captured_seconds;
        s.total_bridge_ms=fg::elapsed_ms(begin);s.last_error=0;
        s.source_fps=context->monitor.original_fps();s.presented_fps=context->monitor.presented_fps();
        auto ft=context->monitor.frame_times();s.frametime_mean_ms=ft.mean;s.frametime_max_ms=ft.maximum;s.frametime_p95_ms=ft.p95;return 0;
    }catch(...){return fail(FG_ENGINE_ERROR);}
}
extern "C" FG_EXPORT const FGBridgeAPI* fg_bridge_api(uint32_t abi) {
    static const FGBridgeAPI api{FG_BRIDGE_ABI,sizeof(FGBridgeAPI),1,1,1,0,0,create,destroy,configure,pad,capture,snapshot};
    return abi==FG_BRIDGE_ABI?&api:nullptr;
}
extern "C" unsigned fg_bridge_instances(){return instances.load();}
#ifdef __PS4__
extern "C" {
FG_EXPORT const char* g_pluginName="PS4 FrameGen Cooperative Bridge";
FG_EXPORT const char* g_pluginDesc="Explicit owned RGBA capture and callback presentation. No game hooks.";
FG_EXPORT const char* g_pluginAuth="PS4 FrameGen";
FG_EXPORT uint32_t g_pluginVersion=0x00000300;
FG_EXPORT int plugin_load(int,const char**){return 0;}
FG_EXPORT int plugin_unload(int,const char**){return instances.load()?FG_BUSY:0;}
}
#endif
