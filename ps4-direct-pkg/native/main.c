#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <orbis/Http.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>

/* Experimental PS4-native network diagnostic. No package installation. */
#define URL_FILE "/data/ps4direct_url.txt"
static void show_dialog(const char *message) {
    OrbisMsgDialogParam p;
    OrbisMsgDialogUserMessageParam u;
    memset(&p,0,sizeof(p));
    memset(&u,0,sizeof(u));
    p.baseParam.size=sizeof(p.baseParam);
    p.baseParam.magic=(uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER+(uint64_t)&p.baseParam);
    p.size=sizeof(p);
    p.mode=ORBIS_MSG_DIALOG_MODE_USER_MSG;
    u.msg=message;
    u.buttonType=ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    p.userMsgParam=&u;
    if(sceMsgDialogInitialize()<0) return;
    if(sceMsgDialogOpen(&p)>=0) {
        while(sceMsgDialogUpdateStatus()!=ORBIS_COMMON_DIALOG_STATUS_FINISHED) {}
        sceMsgDialogClose();
    }
    sceMsgDialogTerminate();
}
static int check_url(char *out,size_t cap) {
    FILE *f=fopen(URL_FILE,"rb");
    if(!f) return -1;
    size_t n=fread(out,1,cap-1,f);
    fclose(f);
    out[n]=0;
    char *newline=strpbrk(out,"\r\n");
    if(newline) *newline=0;
    if(!n || n==cap-1) return -2;
    if(strncmp(out,"https://",8)!=0 && strncmp(out,"http://",7)!=0) return -3;
    return 0;
}
int main(void) {
    char url[4096],message[512];
    int netid=-1,sslid=-1,httpid=-1,tpl=-1,conn=-1,req=-1;
    int status=0,code=-1;
    size_t got=0;
    uint8_t bytes[1024];
    if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG)<0 ||
       sceCommonDialogInitialize()<0) return 1;
    if(check_url(url,sizeof(url))!=0) {
        show_dialog("PS4 Direct PKG - diagnostico 0.7\n\nEsta prueba es nativa de PS4.\nPara probar una URL, coloca un archivo de texto en /data/ps4direct_url.txt\n\nTodavia NO instala PKG.");
        return 0;
    }
    if(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET)<0 ||
       sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL)<0 ||
       sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP)<0) {code=-1;goto done;}
    if(sceNetInit()<0) {code=-2;goto done;}
    netid=sceNetPoolCreate("PS4Direct",128*1024,0);
    if(netid<0) {code=-3;goto done;}
    sslid=sceSslInit(1024*1024);
    if(sslid<0) {code=-4;goto done;}
    httpid=sceHttpInit(netid,sslid,1024*1024);
    if(httpid<0) {code=-5;goto done;}
    tpl=sceHttpCreateTemplate(httpid,"PS4Direct/0.7",ORBIS_HTTP_VERSION_1_1,1);
    if(tpl<0) {code=-6;goto done;}
    /* Do not install an always-accept certificate callback. */
    conn=sceHttpCreateConnectionWithURL(tpl,url,1);
    if(conn<0) {code=-7;goto done;}
    req=sceHttpCreateRequestWithURL(conn,ORBIS_METHOD_GET,url,0);
    if(req<0) {code=-8;goto done;}
    if(sceHttpAddRequestHeader(req,"Range","bytes=0-1023",0)<0) {code=-9;goto done;}
    sceHttpSetConnectTimeOut(req,15000000);
    if(sceHttpSendRequest(req,NULL,0)<0) {code=-10;goto done;}
    if(sceHttpGetStatusCode(req,&status)<0) {code=-11;goto done;}
    if(status!=206) {code=-12;goto done;}
    while(got<sizeof(bytes)) {
        int n=sceHttpReadData(req,bytes+got,(uint32_t)(sizeof(bytes)-got));
        if(n<0) {code=-13;goto done;}
        if(n==0) break;
        got+=(size_t)n;
    }
    code=(got==1024 && bytes[0]==0x7f && bytes[1]=='C' && bytes[2]=='N' && bytes[3]=='T')?0:-14;
done:
    snprintf(message,sizeof(message),"PS4 Direct PKG - diagnostico nativo\n\nHTTP: %d\nBytes: %lu\nCodigo: %d\nCabecera: %s\n\nNO instala paquetes.",status,(unsigned long)got,code,code==0?"PKG reconocible":"no confirmada");
    if(req>=0) sceHttpDeleteRequest(req);
    if(conn>=0) sceHttpDeleteConnection(conn);
    if(tpl>=0) sceHttpDeleteTemplate(tpl);
    if(httpid>=0) sceHttpTerm(httpid);
    if(sslid>=0) sceSslTerm(sslid);
    if(netid>=0) sceNetPoolDestroy(netid);
    show_dialog(message);
    return 0;
}