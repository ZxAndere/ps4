#include "platform.h"
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static FILE* log_file;
static void* video_memory;
static off_t video_offset;
static int video_handle=-1,pad_handle=-1,buffer_index;
static int64_t serial;
static int video_allocated,video_mapped;
static const size_t slot=0x400000; // 720p RGBA fits; two independently aligned surfaces.

void boot_mark(const char* message) {
    sceKernelDebugOutText(0,"[FrameGenBoot] %s\n",message);
    if(log_file){fprintf(log_file,"%s\n",message);fflush(log_file);}
}
void boot_log_result(const char* operation,int result) {
    sceKernelDebugOutText(0,"[FrameGenBoot] %s rc=0x%08x\n",operation,(unsigned)result);
    if(log_file){fprintf(log_file,"%s rc=0x%08x\n",operation,(unsigned)result);fflush(log_file);}
}
int boot_optional_log(int stage) {
    char path[128];int rc=mkdir("/data/PS4FrameGenBoot",0700);(void)rc;
    snprintf(path,sizeof(path),"/data/PS4FrameGenBoot/BOOT%02d.log",stage);
    log_file=fopen(path,"a");
    if(!log_file){boot_mark("OPTIONAL LOG UNAVAILABLE; continuing");return -1;}
    boot_mark("MAIN REACHED - NEW BOOT AUDIT SESSION");return 0;
}
void boot_notify(const char* text) {
    OrbisNotificationRequest request={0};request.targetId=-1;
    size_t i=0;while(text[i]&&i<sizeof(request.message)-1){request.message[i]=text[i];++i;}
    int rc=sceKernelSendNotificationRequest(0,&request,sizeof(request),0);
    boot_log_result("Notification",rc); // A disabled notification is not an application failure.
}
void boot_sleep(unsigned milliseconds){sceKernelUsleep(milliseconds*1000u);}
int boot_video_open(void) {
    int rc=sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),2*slot,0x200000,3,&video_offset);
    boot_log_result("AllocateDirectMemory",rc);if(rc<0)return rc;video_allocated=1;
    rc=sceKernelMapDirectMemory(&video_memory,2*slot,0x33,0,video_offset,0x200000);
    boot_log_result("MapDirectMemory",rc);if(rc<0)return rc;video_mapped=1;
    video_handle=sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN,ORBIS_VIDEO_OUT_BUS_MAIN,0,0);
    boot_log_result("VideoOutOpen",video_handle);if(video_handle<0)return video_handle;
    OrbisVideoOutBufferAttribute attr={0};
    sceVideoOutSetBufferAttribute(&attr,ORBIS_VIDEO_OUT_PIXEL_FORMAT_A8B8G8R8_SRGB,
        ORBIS_VIDEO_OUT_TILING_MODE_LINEAR,ORBIS_VIDEO_OUT_ASPECT_RATIO_16_9,1280,720,1280);
    void* buffers[2]={video_memory,(char*)video_memory+slot};
    rc=sceVideoOutRegisterBuffers(video_handle,0,buffers,2,&attr);boot_log_result("RegisterBuffers",rc);if(rc<0)return rc;
    rc=sceVideoOutSetFlipRate(video_handle,ORBIS_VIDEO_OUT_FLIP_60HZ);boot_log_result("SetFlipRate",rc);
    return rc;
}
int boot_video_present(const void* rgba) {
    if(video_handle<0||!video_mapped||!rgba)return -1;
    void* destination=(char*)video_memory+buffer_index*slot;
    memcpy(destination,rgba,1280u*720u*4u);
    int64_t wanted=++serial;int rc=sceVideoOutSubmitFlip(video_handle,buffer_index,ORBIS_VIDEO_OUT_FLIP_VSYNC,wanted);
    if(rc<0){boot_log_result("SubmitFlip",rc);return rc;}
    // Bound the wait. A submission alone never counts as a displayed frame.
    for(int i=0;i<6000;++i) {
        OrbisVideoOutFlipStatus status={0};rc=sceVideoOutGetFlipStatus(video_handle,&status);
        if(rc<0){boot_log_result("GetFlipStatus",rc);return rc;}
        if(status.num>0&&status.flipArg==wanted){buffer_index=1-buffer_index;return 0;}
        sceKernelUsleep(500);
    }
    boot_mark("FLIP TIMEOUT - STOPPING; no further submissions");return -1;
}
int boot_video_close(void) {
    if(video_handle>=0) {
        int rc=sceVideoOutClose(video_handle);boot_log_result("VideoOutClose",rc);
        if(rc<0){boot_mark("CLOSE FAILED - memory retained until process exit");return rc;}
        video_handle=-1;
    }
    if(video_mapped){int rc=sceKernelMunmap(video_memory,2*slot);boot_log_result("Munmap",rc);if(rc<0)return rc;video_mapped=0;}
    if(video_allocated){int rc=sceKernelReleaseDirectMemory(video_offset,2*slot);boot_log_result("ReleaseDirectMemory",rc);if(rc<0)return rc;video_allocated=0;}
    return 0;
}
int boot_pad_open(void) {
    OrbisUserServiceInitializeParams p={0};p.priority=ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    int rc=sceUserServiceInitialize(&p);boot_log_result("UserServiceInitialize",rc);if(rc<0)return rc;
    int user=0;rc=sceUserServiceGetInitialUser(&user);boot_log_result("GetInitialUser",rc);if(rc<0)return rc;
    rc=scePadInit();boot_log_result("PadInit",rc);if(rc<0)return rc;
    pad_handle=scePadOpen(user,0,0,0);boot_log_result("PadOpen",pad_handle);return pad_handle<0?pad_handle:0;
}
int boot_pad_options(void) {
    OrbisPadData data={0};int rc=scePadReadState(pad_handle,&data);
    if(rc<0){boot_log_result("PadReadState",rc);return -1;}
    return data.connected&&(data.buttons&ORBIS_PAD_BUTTON_OPTIONS)?1:0;
}
void boot_pad_close(void){if(pad_handle>=0){boot_log_result("PadClose",scePadClose(pad_handle));pad_handle=-1;}}
