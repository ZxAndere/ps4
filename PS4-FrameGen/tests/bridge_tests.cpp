#include "goldhen/plugin/bridge.h"
#include "overlay/overlay.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
struct Sink {std::vector<fg::Image> images;FGContext* context;const FGBridgeAPI* api;bool fail=false,confirm=false,drop_generated=false;};
static int consume(void* opaque,const FGOutput* output,double* confirmed) {
    auto& sink=*static_cast<Sink*>(opaque);FGSnapshot snapshot{};
    CHECK(sink.api->snapshot(sink.context,&snapshot)==FG_BUSY); // Callback reentry must not deadlock.
    if(sink.fail)return -99;
    if(sink.drop_generated&&output->generated)return FG_DROPPED_BY_CONSUMER;
    fg::Image image({int(output->width),int(output->height)});
    std::memcpy(image.data(),output->rgba,image.bytes());sink.images.push_back(std::move(image));
    if(sink.confirm){*confirmed=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();return FG_CONFIRMED_FLIP;}
    return FG_CONSUMED_OFFLINE;
}
int main() {
 try {
    CHECK(!fg_bridge_api(FG_BRIDGE_ABI+1));const auto* api=fg_bridge_api(FG_BRIDGE_ABI);
    CHECK(api&&api->size==sizeof(FGBridgeAPI));CHECK(!api->game_hooks&&!api->gpu_backend);
    auto* context=api->create(2);CHECK(context);Sink sink{{},context,api};
    FGSnapshot snapshot{};CHECK(api->snapshot(context,&snapshot)==0);CHECK(!snapshot.configuration.enabled);
    CHECK(api->pad(context,fg::L3|fg::R3)==0);CHECK(api->pad(context,fg::L3|fg::R3)==0);
    CHECK(api->snapshot(context,&snapshot)==0&&snapshot.menu_visible==1);
    CHECK(api->pad(context,0)==0);CHECK(api->pad(context,fg::Cross)==0);
    CHECK(api->snapshot(context,&snapshot)==0&&snapshot.configuration.enabled==1);
    api->pad(context,0);api->pad(context,fg::Circle); // Test output without overlay pixels.
    auto a=fg::test_scene({160,120},0),b=fg::test_scene({160,120},8);
    FGInput input{a.data(),a.bytes(),160,120,640,FG_RGBA8_LINEAR,FG_CPU_READY|FG_RETAIN_UNTIL_RETURN,1};
    CHECK(api->capture(context,&input,consume,&sink)==1);input.rgba=b.data();input.captured_seconds=1.04;
    CHECK(api->capture(context,&input,consume,&sink)==0);CHECK(sink.images.size()==2);
    CHECK(fg::mse(sink.images[1],a)>0&&fg::mse(sink.images[1],b)>0);
    CHECK(api->snapshot(context,&snapshot)==0&&snapshot.delivered==2&&snapshot.confirmed==0);
    input.capacity=1;input.captured_seconds=1.08;CHECK(api->capture(context,&input,consume,&sink)==FG_UNSUPPORTED_SURFACE);
    CHECK(api->snapshot(context,&snapshot)==0&&!snapshot.configuration.enabled);
    input.capacity=b.bytes();input.format=99;CHECK(api->capture(context,&input,consume,&sink)==FG_UNSUPPORTED_SURFACE);
    input.format=FG_RGBA8_LINEAR;input.flags=FG_CPU_READY;CHECK(api->capture(context,&input,consume,&sink)==FG_UNSUPPORTED_SURFACE);
    input.flags|=FG_RETAIN_UNTIL_RETURN;input.captured_seconds=1.04;CHECK(api->capture(context,&input,consume,&sink)==FG_BAD_ARGUMENT);
    FGConfiguration config{1,4,0,0,0};CHECK(api->configure(context,&config)==0);
    sink.fail=true;input.captured_seconds=1.12;CHECK(api->capture(context,&input,consume,&sink)==FG_CALLBACK_ERROR);
    CHECK(api->snapshot(context,&snapshot)==0&&!snapshot.configuration.enabled);
    sink.fail=false;sink.confirm=true;sink.drop_generated=true;config={1,2,0,0,0};
    CHECK(api->configure(context,&config)==0);input.captured_seconds=1.16;
    CHECK(api->capture(context,&input,consume,&sink)==0);CHECK(api->snapshot(context,&snapshot)==0);
    CHECK(snapshot.confirmed==1&&snapshot.dropped==1);CHECK(!snapshot.deadline_available);
    // Actual steady-clock capture, an intentionally tiny budget forces newest-original bypass.
    config.low_latency=1;config.source_budget_us=1;CHECK(api->configure(context,&config)==0);
    input.flags|=FG_CLOCK_MONOTONIC;
    input.captured_seconds=std::chrono::duration<double>(fg::Clock::now().time_since_epoch()).count();
    CHECK(api->capture(context,&input,consume,&sink)==0);CHECK(api->snapshot(context,&snapshot)==0);
    CHECK(snapshot.deadline_available&&snapshot.skipped_reason==4&&snapshot.dropped>=2);
    CHECK(snapshot.confirmed==2&&snapshot.configuration.source_budget_us==1);
    CHECK(api->destroy(context)==0);
    std::cout<<"PASS cooperative ABI, actual frame capture/interpolation, L3 R3, offline-vs-flip accounting, bounds, reentry callback failure, confirmed timestamps, consumer discards and live deadline bypass\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
