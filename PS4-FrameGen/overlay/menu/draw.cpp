#include "overlay/overlay.hpp"
#include <cstdio>
namespace fg {
// Compact original 3x5 bitmap alphabet. No system fonts or proprietary assets.
static const char alphabet[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-:/ >";
static const char* glyphs[]={
 "111101101101111", "010110010010111", "110001111100111", "110001011001110", "101101111001001",
 "111100110001110", "011100111101111", "111001010010010", "111101111101111", "111101111001110",
 "010101111101101", "110101110101110", "111100100100111", "110101101101110", "111100110100111",
 "111100110100100", "111100101101111", "101101111101101", "111010010010111", "001001001101111",
 "101101110101101", "100100100100111", "101111111101101", "101111111111101", "111101101101111",
 "111101111100100", "111101101111001", "110101110101101", "111100111001111", "111010010010010",
 "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
 "111001010100111", "000000000000010", "000000111000000", "000010000010000", "001001010100100",
 "000000000000000", "100010001010100"};
static_assert(sizeof(glyphs)/sizeof(*glyphs)==sizeof(alphabet)-1,"Complete bitmap alphabet");
void draw_text(Image& im,int x,int y,const std::string& text,int k) {
    int start=x;
    for(char c:text) {
        if(c=='\n'){x=start;y+=6*k;continue;}
        if(c>='a'&&c<='z')c=char(c-'a'+'A');
        int i=0;while(alphabet[i]&&alphabet[i]!=c)++i;
        const char* bits=alphabet[i]?glyphs[i]:"000000000000000";
        for(int row=0;row<5;++row)for(int col=0;col<3;++col)
            if(bits[row*3+col]=='1')for(int dy=0;dy<k;++dy)for(int dx=0;dx<k;++dx) {
                int px=x+col*k+dx,py=y+row*k+dy;
                if(px>=0&&py>=0&&px<im.size().width&&py<im.size().height)im.at(px,py)={224,236,255,255};
            }
        x+=4*k;
    }
}
void draw_menu(Image& im,const Menu& menu,const Metrics& m,const PresentationMonitor& monitor,const std::string& status) {
    if(!menu.visible)return;
    int panel=im.size().width<1000?im.size().width:900;
    for(int y=20;y<440&&y<im.size().height;++y)for(int x=20;x<panel-20;++x) {
        auto& p=im.at(x,y);p={uint8_t(p.r/4+6),uint8_t(p.g/4+12),uint8_t(p.b/4+24),255};
    }
    const char* resolutions[]={"NATIVE","1080P","720P","540P","360P"};
    const char* sections[]={"MAIN","GRAPHICS","PERFORMANCE","COMPATIBILITY","ADVANCED"};
    const char* filters[]={"BILINEAR","AREA","NEAREST","BICUBIC"};
    const char* profiles[]={"PERFORMANCE","QUALITY","BALANCED"};
    std::string text="PS4 FRAMEGEN 0.3 / "+std::string(sections[menu.section])+"\nL1 R1 SECTIONS - CIRCLE CLOSE\n\n";
    auto row=[&](int i,const std::string& label){text+=(i==menu.selected?"> ":"  ")+label+"\n";};
    const auto& c=menu.config;
    if(menu.section==0) {
        row(0,std::string("ENGINE ")+(c.enabled?"ON":"OFF")+(c.safe_mode?" SAFE":""));
        row(1,"MULTIPLIER X"+std::to_string(c.multiplier));row(2,std::string("AUTO ")+(c.automatic?"ON":"OFF"));
        row(3,std::string("LOW LATENCY ")+(c.low_latency?"ON":"OFF"));row(4,std::string("SCENE GUARD ")+(c.scene_guard?"ON":"OFF"));
        text+="STATE "+std::string(m.skipped_reason?"SKIPPED":m.generated?"INTERPOLATING":"ORIGINAL")+" / GENERATED "+std::to_string(m.generated)+"\n";
    } else if(menu.section==1) {
        row(0,"PROCESS "+std::string(resolutions[int(c.processing)]));row(1,"OUTPUT "+std::string(resolutions[int(c.output)]));
        row(2,"FILTER "+std::string(filters[int(c.filter)]));row(3,"SHARPNESS "+std::to_string(c.sharpness));
        row(4,"PROFILE "+std::string(profiles[int(c.profile)]));row(5,std::string("VISUAL SPLIT ")+(menu.comparison?"ON":"OFF"));
    } else if(menu.section==2) {
        char values[700];auto ft=monitor.frame_times();
        std::snprintf(values,sizeof(values),"INPUT FPS %s PRESENT FPS %s\nCPU %.2f MS WARP %.2f MS\nSCALE %.2f MS LAT EST %.2f MS\nDROPPED %llu RGBA %.2f MIB\nRSS GPU INPUT TO PHOTON NO DISPONIBLE\nADAPTIVE PRESSURE %d SKIP %d\n",
            monitor.original_fps()>0?std::to_string(monitor.original_fps()).c_str():"NO DISPONIBLE",
            monitor.presented_fps()>0?std::to_string(monitor.presented_fps()).c_str():"NO DISPONIBLE",
            m.total_ms,m.interpolation_ms,m.scaling_ms,m.latency_estimate_ms,(unsigned long long)menu.dropped,m.image_bytes/1048576.0,menu.pressure,m.skipped_reason);
        text+=values;
        if(ft.samples){std::snprintf(values,sizeof(values),"FLIP MS AVG %.2f MAX %.2f\nP50 %.2f P95 %.2f P99 %.2f\n",ft.mean,ft.maximum,ft.p50,ft.p95,ft.p99);text+=values;}
        else text+="FLIP FRAMETIME AVG MAX P50 P95 P99 NO DISPONIBLE\n";
    } else if(menu.section==3) {
        text+="GAME "+menu.game_id+"\nPROFILE LOCAL / MANUAL START\nSTATE "+menu.compatibility+"\nEXTERNAL GAME HOOKS NO DISPONIBLE\nFIRMWARE 13.52 UNVERIFIED\nGOLDHEN BETA 12 UNVERIFIED\n";
    } else {
        row(0,"BACKEND CPU / GPU NO DISPONIBLE");row(1,std::string("SAFE MODE ")+(c.safe_mode?"ON":"OFF"));
        row(2,std::string("JSONL DIAGNOSTICS ")+(c.diagnostics?"ON":"OFF"));row(3,"PRESENT CAP "+std::to_string(c.fps_limit));row(4,"RESTORE DEFAULTS");if(menu.profile_restore_available)row(5,"RESTORE PREVIOUS PROFILE");
    }
    char dimensions[240];std::snprintf(dimensions,sizeof(dimensions),"\nINPUT %dX%d INTERNAL %dX%d\nOUTPUT BUFFER %dX%d\n%s\nL3 R3 MENU - DPAD X EDIT - TRIANGLE SAFE",m.input.width,m.input.height,m.processing.width,m.processing.height,m.output.width,m.output.height,status.c_str());
    text+=dimensions;draw_text(im,36,36,text,im.size().width<640?1:2);
}
}
