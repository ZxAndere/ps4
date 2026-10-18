#include "installer.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>

/* SceBgft's ServiceInit symbol is used by PS4-Store and VitaPlex. */
extern int32_t sceBgftServiceInit(OrbisBgftInitParams *params);

#define BGFT_HEAP_SIZE (1024u * 1024u)
/* Keep heap alive while system background task is being registered. */
static void *bgft_heap = NULL;

static int read_header_content_id(const char *path, char id[37])
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t bytes[0x64] = {0};
    size_t n = fread(bytes, 1, sizeof(bytes), f);
    fclose(f);
    if (n != sizeof(bytes) ||
        bytes[0] != 0x7f || bytes[1] != 'C' ||
        bytes[2] != 'N' || bytes[3] != 'T')
        return -2;
    for (int j=0;j<36;j++) {
        char ch=(char)bytes[0x40+j];
        if (!((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||
              ch=='-'||ch=='_'))
            return -3;
        id[j]=ch;
    }
    id[36]=0;
    return 0;
}

/* Return 0 only when BGFT accepted and started an installation task.
 * No uninstalls, overwrites, filename deletion, or task re-registration.
 * stage identifies where error occurred. */
int install_local_pkg(const char *pkg_path,
                      const char *self_title,
                      InstallResult *out)
{
    if (!out) return -1;
    memset(out,0,sizeof(*out));
    out->task_id=-1;
    out->stage=1;

    if (!pkg_path || strncmp(pkg_path,"/data/",6)!=0)
        return -1;

    /* Do not allow a partially written file, arbitrary file or path traversal. */
    if (strstr(pkg_path,"..") || strstr(pkg_path,".part") ||
        strstr(pkg_path,".state") || strlen(pkg_path)>230)
        return -1;
    size_t n=strlen(pkg_path);
    if (n<4 || strcmp(pkg_path+n-4,".pkg")!=0) return -1;

    int ret=read_header_content_id(pkg_path,out->content_id);
    if (ret) {out->raw_error=ret;return -1;}

    out->stage=2;
    ret=sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    if (ret<0) {out->raw_error=ret;return -1;}
    ret=sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    if (ret<0) {out->raw_error=ret;return -1;}

    out->stage=3;
    ret=sceAppInstUtilInitialize();
    /* Already initialized isn't necessarily an error: follow with a real API. */
    char title_id[32]={0};
    int is_app=0;
    ret=sceAppInstUtilGetTitleIdFromPkg(pkg_path,title_id,&is_app);
    if(ret!=0) {out->raw_error=ret;return -1;}
    title_id[sizeof(title_id)-1]='\0';
    strncpy(out->title_id,title_id,sizeof(out->title_id)-1);
    /* Guard against the app attempting to replace itself. */
    if (self_title && strcmp(title_id,self_title)==0) {
        out->raw_error=-8;return -1;
    }
    int exists=0;
    ret=sceAppInstUtilAppExists(title_id,&exists);
    if(ret==0 && exists) {
        out->raw_error=(int32_t)0x80990088u;
        return -1;
    }
    /* If the existence test failed, do not proceed blindly. */
    if(ret!=0) {out->raw_error=ret;return -1;}

    out->stage=4;
    if (!bgft_heap) bgft_heap=calloc(1,BGFT_HEAP_SIZE);
    if (!bgft_heap) {out->raw_error=-4;return -1;}
    OrbisBgftInitParams init={0};
    init.heap=bgft_heap;
    init.heapSize=BGFT_HEAP_SIZE;
    ret=sceBgftServiceInit(&init);
    if(ret!=0) {out->raw_error=ret;return -1;}

    /* BGFT runs in system context: /data maps to /user/data there. */
    char system_path[260];
    snprintf(system_path,sizeof(system_path),"/user%s",pkg_path);
    char display[96];
    snprintf(display,sizeof(display),"PS4 Direct - %.16s",title_id);

    OrbisBgftDownloadParamEx params={0};
    params.slot=0;
    params.params.entitlementType=5;
    params.params.id="";
    params.params.contentUrl=system_path;
    params.params.contentName=display;
    params.params.playgoScenarioId="0";
    params.params.option=ORBIS_BGFT_TASK_OPT_INVISIBLE;

    out->stage=5;
    OrbisBgftTaskId task=-1;
    ret=sceBgftServiceIntDownloadRegisterTaskByStorageEx(&params,&task);
    if(ret!=0) {out->raw_error=ret;return -1;}
    if(task<0) {out->raw_error=-5;return -1;}
    out->task_id=task;

    out->stage=6;
    ret=sceBgftServiceDownloadStartTask(task);
    if(ret!=0) {out->raw_error=ret;return -1;}

    out->stage=7;
    return 0; /* Started, NOT a promise that install will finish. */
}
