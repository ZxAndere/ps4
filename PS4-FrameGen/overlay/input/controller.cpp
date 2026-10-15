#include "overlay/overlay.hpp"
namespace fg {
int Menu::rows()const {const int n[]={5,6,1,1,6};return section==4&&!profile_restore_available?5:n[section];}
void Menu::update(uint32_t buttons) {
    auto pressed=buttons&~last_;
    constexpr uint32_t chord=L3|R3;
    if((buttons&chord)==chord&&(last_&chord)!=chord)visible=!visible;
    last_=buttons;
    if(pressed&Emergency){config.enabled=false;config.automatic=false;config.safe_mode=true;visible=true;}
    if(!visible)return;
    if(pressed&Circle){visible=false;return;}
    if(pressed&L1){section=(section+4)%5;selected=0;}
    if(pressed&R1){section=(section+1)%5;selected=0;}
    if(pressed&Up)selected=(selected+rows()-1)%rows();
    if(pressed&Down)selected=(selected+1)%rows();
    int delta=(pressed&Left)?-1:(pressed&(Right|Cross))?1:0;
    if(!delta)return;
    auto cycle=[&](int v,int count){return (v+delta+count)%count;};
    if(section==0)switch(selected) {
    case 0: if(!config.safe_mode)config.enabled=!config.enabled;break;
    case 1: config.multiplier=2+cycle(config.multiplier-2,3);break;
    case 2: config.automatic=!config.automatic;break;
    case 3: config.low_latency=!config.low_latency;break;
    case 4: config.scene_guard=!config.scene_guard;break;
    }
    if(section==1)switch(selected) {
    case 0: config.processing=Resolution(cycle(int(config.processing),5));break;
    case 1: config.output=Resolution(cycle(int(config.output),3));break;
    case 2: config.filter=Filter(cycle(int(config.filter),4));break;
    case 3: config.sharpness=cycle(config.sharpness/25,5)*25;break;
    case 4: config.profile=Profile(cycle(int(config.profile),3));break;
    case 5: comparison=!comparison;break;
    }
    if(section==4)switch(selected) {
    case 0: break; // CPU is the only executable backend; no decorative GPU selector.
    case 1: config.safe_mode=!config.safe_mode;if(config.safe_mode)config.enabled=false;break;
    case 2: config.diagnostics=!config.diagnostics;break;
    case 3: config.fps_limit=cycle(config.fps_limit/30,3)*30;break;
    case 5: restore_previous=true;break;
    case 4: {auto execution=config.execution;config=Config{};config.execution=execution;break;}
    }
}
}
