// Adds only VideoOut/direct surfaces to BOOT01. No C++, Pad or interpolation.
#include "platform.h"
static uint32_t pixels[1280*720];
static void number(int digit,int x,int y) {
    static const unsigned mask[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    const int segments[7][4]={{20,0,120,15},{140,20,15,90},{140,125,15,90},{20,220,120,15},{0,125,15,90},{0,20,15,90},{20,110,120,15}};
    for(int i=0;i<7;++i)if(mask[digit]&(1u<<i))for(int yy=0;yy<segments[i][3];++yy)for(int xx=0;xx<segments[i][2];++xx)
        pixels[(y+segments[i][1]+yy)*1280+x+segments[i][0]+xx]=0xff70ed55;
}
int main(void) {
    boot_mark("BOOT02 MAIN REACHED");boot_optional_log(2);boot_notify("FrameGen BOOT02: iniciando VideoOut. Debe aparecer 02 en verde.");
    int rc=boot_video_open();if(rc<0){boot_notify("BOOT02: fallo de inicializacion grafica. Ver BOOT02.log");boot_video_close();boot_sleep(8000);return 1;}
    for(unsigned y=0;y<720;++y)for(unsigned x=0;x<1280;++x)pixels[y*1280+x]=0xff24160a;
    number(0,440,230);number(2,650,230);
    rc=boot_video_present(pixels);
    if(rc==0){boot_mark("BOOT02 FIRST FLIP CONFIRMED");boot_notify("BOOT02: primer flip confirmado. Salida automatica en 30 segundos.");for(int i=0;i<30;++i)boot_sleep(1000);}
    boot_video_close();boot_mark("BOOT02 CLEAN RETURN");return rc<0?1:0;
}
