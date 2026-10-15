// Progressive FrameGen components with the interpolation engine permanently OFF.
#include "platform.h"
#include "engine/framegen.hpp"
#include "overlay/overlay.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
#ifndef BOOT_STAGE
#error Define BOOT_STAGE as 3 (image/text/Pad) or 4 (add bounded Processor pool)
#endif
int main() {
    boot_mark("CPP MAIN REACHED");boot_optional_log(BOOT_STAGE);
    boot_notify(BOOT_STAGE==3?"FrameGen BOOT03: C++, imagen, texto y mando. Motor OFF.":"FrameGen BOOT04: pool CPU y copia de original. Motor OFF.");
    try {
        if(boot_video_open()<0)throw std::runtime_error("Video initialization failed");
        if(boot_pad_open()<0)throw std::runtime_error("Pad initialization failed");
        boot_mark("IMAGE ALLOCATION BEGIN");
        fg::Image image=fg::test_scene({1280,720},0);
        fg::draw_text(image,48,48,BOOT_STAGE==3?"BOOT03 CXX IMAGE PAD":"BOOT04 CPU POOL ORIGINAL",4);
        fg::draw_text(image,48,96,"FRAME GENERATION OFF - PRX ABSENT",3);
        fg::draw_text(image,48,132,"OPTIONS EXIT - AUTO EXIT 30 SECONDS",3);
        boot_mark("IMAGE AND TEXT READY");
#if BOOT_STAGE == 4
        fg::Execution execution;execution.threads=2;
        boot_mark("PROCESSOR POOL CONSTRUCTION BEGIN");fg::Processor processor(execution);
        fg::Config config;config.enabled=false;config.execution=execution;
        auto& batch=processor.run(image,image,config);
        if(batch.metrics.generated!=0||batch.frames.size()!=1)throw std::runtime_error("Disabled engine invariant violated");
        boot_mark("PROCESSOR POOL AND ORIGINAL COPY READY");
        const auto& frame=batch.frames[0];
#else
        const auto& frame=image;
#endif
        if(boot_video_present(frame.data())<0)throw std::runtime_error("First flip failed");
        boot_mark("FIRST FLIP CONFIRMED");
        // Keep the same real frame, no synthetic FPS or generated frames reported.
        for(int i=0;i<300;++i){int exit=boot_pad_options();if(exit<0)throw std::runtime_error("Pad read failed");if(exit)break;boot_sleep(100);}
        boot_pad_close();boot_video_close();boot_mark("CLEAN RETURN");return 0;
    }catch(const std::exception& error){boot_mark(error.what());boot_notify("FrameGen BOOT: fallo detectado. Conservar log; no continuar a la siguiente etapa.");boot_pad_close();boot_video_close();boot_sleep(8000);return 1;}
}
