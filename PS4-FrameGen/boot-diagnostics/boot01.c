// Small C/SDK-CRT launch control: no graphics, Pad, C++, threads, PRX, or engine.
#include "platform.h"
int main(void) {
    boot_mark("BOOT01 MAIN REACHED");
    boot_notify("FrameGen BOOT01: MAIN alcanzado. Sin graficos. Salida automatica en 20 segundos.");
    // File logging is deliberately absent from this minimum control.
    for(int i=0;i<20;++i)boot_sleep(1000);
    boot_mark("BOOT01 CLEAN RETURN");return 0;
}
