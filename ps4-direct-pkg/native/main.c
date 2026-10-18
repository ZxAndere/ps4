#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "installer.h"
#include <orbis/CommonDialog.h>
#include <orbis/Http.h>
#include <orbis/MsgDialog.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

/*
 * PS4 Direct PKG v1.1 - experimental download + confirmed BGFT install request.
 * Uses BGFT to request a local installation only after user confirmation. No NovaPS or PC.
 *
 * The URL is read from /data/ps4direct_url.txt.
 * Existing partial downloads are resumed only when the URL hash
 * and HTTP Content-Range indicate the same remote file.
 */
#define URL_FILE "/data/ps4direct_url.txt"
#define URL_CAP 2048
#define HEADER_CAP 256
#define CHUNK_SIZE (64u * 1024u)
#define MAX_DOWNLOAD_BYTES (16ULL * 1024ULL * 1024ULL * 1024ULL)
#define MIN_SPACE_WARNING "Comprueba antes el espacio libre de /data."
static uint8_t chunk[CHUNK_SIZE];

static void dialog_init(OrbisMsgDialogParam *p)
{
    memset(p, 0, sizeof(*p));
    p->baseParam.size = sizeof(p->baseParam);
    p->baseParam.magic =
        (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)&p->baseParam);
    p->size = sizeof(*p);
}

static void dialog_wait_finish(void)
{
    while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
        sceKernelUsleep(16000);
    sceMsgDialogClose();
    sceMsgDialogTerminate();
}

static void show_message(const char *msg)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam user;
    memset(&user, 0, sizeof(user));
    dialog_init(&param);
    user.msg = msg;
    user.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &user;
    if (sceMsgDialogInitialize() < 0) return;
    if (sceMsgDialogOpen(&param) >= 0) dialog_wait_finish();
    else sceMsgDialogTerminate();
}

/* Explicit user confirmation before downloading large files. */
static bool confirm_message(const char *msg)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam user;
    OrbisMsgDialogResult result;
    memset(&user, 0, sizeof(user));
    memset(&result, 0, sizeof(result));
    dialog_init(&param);
    user.msg = msg;
    user.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_YESNO_FOCUS_NO;
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &user;
    if (sceMsgDialogInitialize() < 0) return false;
    if (sceMsgDialogOpen(&param) < 0) {
        sceMsgDialogTerminate();
        return false;
    }
    while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
        sceKernelUsleep(16000);
    sceMsgDialogClose();
    int ok = sceMsgDialogGetResult(&result);
    sceMsgDialogTerminate();
    return ok >= 0 && result.buttonId == ORBIS_MSG_DIALOG_BUTTON_ID_YES;
}

static bool progress_start(void)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogProgressBarParam bar;
    memset(&bar, 0, sizeof(bar));
    dialog_init(&param);
    bar.barType = ORBIS_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE_CANCEL;
    bar.msg = "Descargando en la PS4. Puedes cancelar.";
    param.mode = ORBIS_MSG_DIALOG_MODE_PROGRESS_BAR;
    param.progBarParam = &bar;
    if (sceMsgDialogInitialize() < 0) return false;
    if (sceMsgDialogOpen(&param) >= 0) return true;
    sceMsgDialogTerminate();
    return false;
}

static bool progress_update(uint64_t written, uint64_t total)
{
    uint32_t percent = (uint32_t)(100ULL * written / total);
    if (percent > 100) percent = 100;
    sceMsgDialogProgressBarSetValue(0, percent);
    return sceMsgDialogUpdateStatus() == ORBIS_COMMON_DIALOG_STATUS_FINISHED;
}

static void progress_finish(void)
{
    sceMsgDialogClose();
    sceMsgDialogTerminate();
}

static bool valid_url(const char *url)
{
    size_t n = strlen(url);
    if (n < 12 || n >= URL_CAP ||
        (strncmp(url,"https://",8) && strncmp(url,"http://",7)))
        return false;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 32 || c >= 127) return false;
    }
    return true;
}

static bool load_url(char *url, size_t cap)
{
    FILE *f = fopen(URL_FILE, "rb");
    if (!f) return false;
    size_t n = fread(url, 1, cap-1, f);
    int extra = fgetc(f);
    fclose(f);
    if (extra != EOF || n == cap-1) return false;
    url[n] = '\0';
    char *end = strpbrk(url, "\r\n");
    if (end) *end = '\0';
    return valid_url(url);
}

static uint64_t hash_url(const char *s)
{
    uint64_t h = 14695981039346656037ULL;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static bool file_size(const char *path, uint64_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseeko(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    off_t offset = ftello(f);
    fclose(f);
    if (offset < 0) return false;
    *len = (uint64_t)offset;
    return true;
}

/* File names are derived from the URL's path, not user-supplied directories. */
static void create_paths(const char *url, char *part, char *final, char *meta)
{
    const char *p = strchr(url, '/');
    p = p ? strchr(p+2, '/') : NULL; /* First path slash after https://hostname */
    const char *name = p ? strrchr(p, '/') : NULL;
    name = name ? name+1 : "download";
    char base[48];
    size_t n=0;
    while (*name && *name!='?' && *name!='#' && n<sizeof(base)-1) {
        unsigned char c = (unsigned char)*name++;
        if (isalnum(c) || c=='-' || c=='_' || c=='.')
            base[n++] = (char)c;
        else
            base[n++] = '_';
    }
    base[n]='\0';
    if (n>4 && !strcmp(base+n-4,".pkg")) base[n-4]='\0';
    if (!base[0] || base[0]=='.') strcpy(base,"package");
    uint64_t h = hash_url(url); /* Distinct URLs never overwrite one another. */
    char stem[128];
    snprintf(stem, sizeof(stem), "/data/ps4direct_%.40s_%08llx",
             base, (unsigned long long)(h & 0xffffffffULL));
    snprintf(part,256,"%s.part",stem);
    snprintf(final,256,"%s.pkg",stem);
    snprintf(meta,256,"%s.state",stem);
}

static bool save_state(const char *path, uint64_t source_hash, uint64_t total)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    int ret = fprintf(f,"PS4DIRECT1\n%016llx\n%llu\n",
        (unsigned long long)source_hash, (unsigned long long)total);
    bool ok = ret > 0 && fflush(f)==0;
    if (fclose(f) != 0) ok = false;
    return ok;
}

static bool load_state(const char *path, uint64_t *source_hash, uint64_t *total)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char magic[20] = {0};
    unsigned long long h=0, t=0;
    bool ok = fgets(magic,sizeof(magic),f) &&
              !strcmp(magic,"PS4DIRECT1\n") &&
              fscanf(f,"%llx\n%llu",&h,&t)==2;
    fclose(f);
    if (ok) { *source_hash=(uint64_t)h; *total=(uint64_t)t; }
    return ok;
}

static bool pkg_header_valid(const uint8_t *x)
{
    return x[0]==0x7f && x[1]=='C' && x[2]=='N' && x[3]=='T';
}

static bool check_existing_pkg_header(const char *path)
{
    FILE *f=fopen(path,"rb");
    if(!f) return false;
    uint8_t b[4] = {0};
    bool ok = fread(b,1,4,f)==4 && pkg_header_valid(b);
    fclose(f);
    return ok;
}

static bool header_value(int request, const char *key, char *dst, size_t cap)
{
    char *headers=NULL;
    size_t len=0;
    if (sceHttpGetAllResponseHeaders(request,&headers,&len)<0 || !headers)
        return false;
    size_t needle_len=strlen(key);
    size_t i=0;
    while(i<len) {
        size_t begin=i;
        while(i<len && headers[i]!='\r' && headers[i]!='\n') i++;
        size_t end=i;
        while(i<len && (headers[i]=='\r' || headers[i]=='\n')) i++;
        if(end-begin < needle_len+1 || headers[begin+needle_len]!=':')
            continue;
        bool match=true;
        for(size_t j=0;j<needle_len;j++) {
            unsigned char a=(unsigned char)headers[begin+j];
            unsigned char b=(unsigned char)key[j];
            if(tolower(a)!=tolower(b)) {match=false;break;}
        }
        if(!match) continue;
        size_t start=begin+needle_len+1;
        while(start<end && (headers[start]==' ' || headers[start]=='\t')) start++;
        while(end>start && (headers[end-1]==' ' || headers[end-1]=='\t')) end--;
        size_t n=end-start;
        if(n==0 || n>=cap) return false;
        memcpy(dst,headers+start,n);
        dst[n]=0;
        return true;
    }
    return false;
}

/* Header must contain a concrete byte start, end and total size. */
static bool content_range(int request, uint64_t *begin,
                          uint64_t *end, uint64_t *total)
{
    char value[HEADER_CAP];
    unsigned long long b=0,e=0,t=0;
    char trailing='x';
    if(!header_value(request,"Content-Range",value,sizeof(value))) return false;
    int n=sscanf(value,"bytes %llu-%llu/%llu%c",&b,&e,&t,&trailing);
    if(n!=3 || t==0 || b>e || e>=t) return false;
    *begin=(uint64_t)b;
    *end=(uint64_t)e;
    *total=(uint64_t)t;
    return true;
}


/* Validate a completed file against the URL-bound download state.
 * A PKG magic number by itself is insufficient to trust a download.
 * Does not provide full cryptographic verification. */
static bool completed_file_matches_state(const char *pkg_path,
                                         const char *state_path, uint64_t url_hash)
{
    uint64_t disk=0, recorded_hash=0, recorded_len=0;
    if(!file_size(pkg_path,&disk) ||
       !load_state(state_path,&recorded_hash,&recorded_len))
        return false;
    if(recorded_hash!=url_hash || recorded_len!=disk ||
       disk<0x100 || disk>MAX_DOWNLOAD_BYTES)
        return false;
    return check_existing_pkg_header(pkg_path);
}

static void offer_install(const char *pkg_path,
                          const char *state_path,
                          const char *url)
{
    char msg[800];
    if(!completed_file_matches_state(pkg_path,state_path,hash_url(url))) {
        show_message("Instalacion bloqueada: el PKG no coincide\n"
                     "con el tamano y el registro de descarga.\n\n"
                     "Conservamos el archivo. No se ha modificado.");
        return;
    }
    uint64_t size=0;
    if(!file_size(pkg_path,&size)) return;
    snprintf(msg,sizeof(msg),
        "Descarga finalizada (%llu MiB).\n\n"
        "Quieres solicitar al sistema la instalacion\n"
        "de este PKG homebrew autorizado?\n\n"
        "No se desinstalara ninguna aplicacion.\n"
        "No se borrara el PKG guardado.\n\n"
        "Si aparece instalado, revisa Notificaciones.",
        (unsigned long long)((size+1024*1024-1)/(1024*1024)));
    if(!confirm_message(msg)) {
        show_message("PKG conservado en /data/.\n\n"
                     "Puedes volver a abrir esta aplicacion\n"
                     "para solicitar la instalacion mas tarde.");
        return;
    }

    InstallResult ir={0};
    int ret=install_local_pkg(pkg_path,"BREW00978",&ir);
    if(ret==0) {
        snprintf(msg,sizeof(msg),
            "Tarea de instalacion INICIADA.\n\n"
            "Titulo: %s\n"
            "Tarea BGFT: %d\n\n"
            "Comprueba el resultado en Notificaciones.\n"
            "Todavia no se ha confirmado que termine.\n\n"
            "El archivo original NO se ha borrado.",
            ir.title_id,ir.task_id);
    } else {
        snprintf(msg,sizeof(msg),
            "No se pudo iniciar la instalacion.\n\n"
            "Etapa: %d\nError: 0x%08X\n"
            "Titulo: %.16s\n\n"
            "El PKG sigue guardado.\n"
            "No se borro ninguna aplicacion.\n"
            "Si ya esta instalado, no se sobrescribe.",
            ir.stage,(unsigned int)ir.raw_error,ir.title_id);
    }
    show_message(msg);
}

static void cleanup_http(int request, int conn, int tpl,
                         int http, int ssl, int net)
{
    if(request>=0) sceHttpDeleteRequest(request);
    if(conn>=0) sceHttpDeleteConnection(conn);
    if(tpl>=0) sceHttpDeleteTemplate(tpl);
    if(http>=0) sceHttpTerm(http);
    if(ssl>=0) sceSslTerm(ssl);
    if(net>=0) sceNetPoolDestroy(net);
}

static void download_pkg(const char *url)
{
    char part[256],final[256],state[256],range[100],prompt[480],summary[640];
    int net=-1,ssl=-1,http=-1,tpl=-1,conn=-1,req=-1;
    int http_code=0,error=0,last_http_error=0;
    uint64_t local_bytes=0, total=0, saved_hash=0, saved_total=0, received=0;
    uint64_t source_hash=hash_url(url), b=0,e=0,t=0;
    int content_length_type=0;
    size_t content_length=0;
    bool resumed=false, bar=false, cancelled=false, complete=false;
    FILE *file=NULL;

    create_paths(url,part,final,state);
    if(file_exists(final)) {
        offer_install(final,state,url);
        return;
    }
    resumed=file_size(part,&local_bytes);
    if(resumed) {
        if(local_bytes<4 || !check_existing_pkg_header(part) ||
           !load_state(state,&saved_hash,&saved_total) ||
           saved_hash!=source_hash || local_bytes>=saved_total) {
            show_message("Hay una descarga parcial que no es segura "
                         "para reanudar.\nNo se ha modificado.\n"
                         "Revisa sus archivos .part y .state en /data/.");
            return;
        }
    }

    if(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET)<0 ||
       sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL)<0 ||
       sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP)<0) {
        error=1; goto end;
    }
    if(sceNetInit()<0) {error=2;goto end;}
    net=sceNetPoolCreate("PS4DirectDL",128*1024,0);
    if(net<0) {error=3;goto end;}
    ssl=sceSslInit(1024*1024);
    if(ssl<0) {error=4;goto end;}
    http=sceHttpInit(net,ssl,1024*1024);
    if(http<0) {error=5;goto end;}
    tpl=sceHttpCreateTemplate(http,"PS4Direct/1.1",ORBIS_HTTP_VERSION_1_1,1);
    if(tpl<0) {error=6;goto end;}
    conn=sceHttpCreateConnectionWithURL(tpl,url,true);
    if(conn<0) {error=7;goto end;}
    req=sceHttpCreateRequestWithURL(conn,ORBIS_METHOD_GET,url,0);
    if(req<0) {error=8;goto end;}
    snprintf(range,sizeof(range),"bytes=%llu-",(unsigned long long)local_bytes);
    if(sceHttpAddRequestHeader(req,"Range",range,0)<0 ||
       sceHttpAddRequestHeader(req,"Accept-Encoding","identity",0)<0) {
        error=9;goto end;
    }
    sceHttpSetConnectTimeOut(req,15000000);
    sceHttpSetSendTimeOut(req,15000000);
    if(sceHttpSendRequest(req,NULL,0)<0) {
        sceHttpGetLastErrno(req,&last_http_error);
        error=10;goto end;
    }
    if(sceHttpGetStatusCode(req,&http_code)<0) {error=11;goto end;}

    if(http_code==206) {
        if(!content_range(req,&b,&e,&t) || b!=local_bytes) {
            error=12;goto end;
        }
        total=t;
    } else if(http_code==200 && local_bytes==0) {
        if(sceHttpGetResponseContentLength(req,&content_length_type,&content_length)<0 ||
           content_length_type!=ORBIS_HTTP_CONTENTLEN_EXIST) {
            error=13;goto end;
        }
        total=(uint64_t)content_length;
    } else {
        /* Never append HTTP 200, redirection pages or error bodies to .part. */
        error=14;goto end;
    }
    if(total<4 || total>MAX_DOWNLOAD_BYTES || local_bytes>=total ||
       (resumed && saved_total!=total)) {
        error=15;goto end;
    }

    snprintf(prompt,sizeof(prompt),
        "%s paquete homebrew autorizado?\n\n"
        "Tamano total: %llu MiB\n"
        "Ya guardado: %llu MiB\n"
        "Destino: /data/ps4direct_...\n\n"
        "Necesitas suficiente espacio libre.\n"
        "Instalacion posterior solo si confirmas.",
        resumed?"Reanudar":"Descargar",
        (unsigned long long)((total+1024*1024-1)/(1024*1024)),
        (unsigned long long)(local_bytes/(1024*1024)));
    if(!confirm_message(prompt)) {cancelled=true;goto end;}

    /* Never begin a large transfer if progress/cancel UI could not open. */
    bar=progress_start();
    if(!bar) {error=25;goto end;}
    if(!resumed && !save_state(state,source_hash,total)) {
        error=16;goto end;
    }
    file=fopen(part,resumed?"ab":"wb");
    if(!file) {error=17;goto end;}
    uint64_t downloaded=local_bytes;
    unsigned int update_counter=0;
    while(downloaded < total) {
        uint32_t ask=CHUNK_SIZE;
        if(total-downloaded<CHUNK_SIZE) ask=(uint32_t)(total-downloaded);
        int n=sceHttpReadData(req,chunk,ask);
        if(n<0) {error=18;break;}
        if(n==0) {error=19;break;}
        if(!resumed && downloaded<4) {
            if((uint64_t)n < 4 || !pkg_header_valid(chunk)) {
                error=20;break;
            }
        }
        if(fwrite(chunk,1,(size_t)n,file)!=(size_t)n) {error=21;break;}
        downloaded+=(uint64_t)n;
        received+=(uint64_t)n;
        update_counter++;
        if(bar && ((update_counter%8)==0 || downloaded==total)) {
            if(progress_update(downloaded,total)) {
                cancelled=true;
                break;
            }
        }
    }
    if(fflush(file)!=0 && error==0) error=22;
    if(fclose(file)!=0 && error==0) error=23;
    file=NULL;
    if(bar) progress_finish();
    bar=false;

    if(!error && !cancelled && downloaded==total) {
        if(rename(part,final)!=0) error=24;
        else complete=true;
    }

end:
    if(file) {
        fflush(file);
        fclose(file);
    }
    if(bar) progress_finish();
    cleanup_http(req,conn,tpl,http,ssl,net);
    if(complete) {
        snprintf(summary,sizeof(summary),
            "Descarga completada en la PS4!\n\n"
            "Total: %llu MiB\n"
            "Archivo: %.230s\n\n"
            "A continuacion puedes solicitar\n"
            "su instalacion en la consola.",
            (unsigned long long)((total+1024*1024-1)/(1024*1024)),
            final);
        show_message(summary);
        offer_install(final,state,url);
        return;
    } else if(cancelled) {
        snprintf(summary,sizeof(summary),
            "Operacion cancelada.\n\n"
            "Archivo parcial conservado para reanudar,\n"
            "si se habian recibido datos.\n\n"
            "No se ha instalado nada.");
    } else {
        snprintf(summary,sizeof(summary),
            "Descarga no completada.\n\n"
            "Etapa error: %d\nHTTP: %d\n"
            "Error red: 0x%08X\n"
            "Recibido en esta sesion: %llu KiB\n\n"
            "El archivo parcial se conserva.\n"
            "No se ha instalado nada.",
            error,http_code,(unsigned int)last_http_error,
            (unsigned long long)(received/1024));
    }
    show_message(summary);
}

int main(void)
{
    char url[URL_CAP] = {0};
    if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG)<0 ||
       sceCommonDialogInitialize()<0) return 1;
    if(!load_url(url,sizeof(url))) {
        show_message("PS4 Direct PKG v1.1\n\n"
                     "Falta una URL valida en:\n"
                     "/data/ps4direct_url.txt\n\n"
                     "Esta version permite descargar PKG homebrew "
                     "autorizados y reanudarlos.\n\n"
                     "Una vez completado, permite solicitar\n"
                     "la instalacion mediante BGFT.");
        return 0;
    }
    download_pkg(url);
    return 0;
}
