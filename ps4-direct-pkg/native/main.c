#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/CommonDialog.h>
#include <orbis/Http.h>
#include <orbis/ImeDialog.h>
#include <orbis/MsgDialog.h>
#include <orbis/Net.h>
#include <orbis/Ssl.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>

/*
 * PS4 Direct PKG v0.9 — safe, read-only connection test.
 * Uses system on-screen keyboard, and native sceHttp APIs.
 * This application does not download full packages or install them.
 */
#define URL_FILE "/data/ps4direct_url.txt"
#define URL_MAX 2048
#define OSK_MAX 510
#define PROBE_BYTES 1024

static void show_dialog(const char *message)
{
    OrbisMsgDialogParam p;
    OrbisMsgDialogUserMessageParam u;
    memset(&p, 0, sizeof(p));
    memset(&u, 0, sizeof(u));
    p.baseParam.size = sizeof(p.baseParam);
    p.baseParam.magic = (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER +
                                   (uint64_t)&p.baseParam);
    p.size = sizeof(p);
    p.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
    u.msg = message;
    u.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_OK;
    p.userMsgParam = &u;
    if (sceMsgDialogInitialize() < 0)
        return;
    if (sceMsgDialogOpen(&p) >= 0) {
        while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED)
            sceKernelUsleep(10000);
        sceMsgDialogClose();
    }
    sceMsgDialogTerminate();
}

static bool valid_url(const char *url)
{
    size_t n = strlen(url);
    if (n < 10 || n >= URL_MAX)
        return false;
    if (strncmp(url, "https://", 8) && strncmp(url, "http://", 7))
        return false;
    /* Reject whitespace/control bytes; URLs must be percent-encoded. */
    for (const unsigned char *p = (const unsigned char *)url; *p; ++p)
        if (*p <= 32 || *p >= 127)
            return false;
    return true;
}

/* Preferred for very long signed URLs transferred by FTP once. */
static bool load_url_file(char *url, size_t cap)
{
    FILE *f = fopen(URL_FILE, "rb");
    if (!f)
        return false;
    size_t n = fread(url, 1, cap - 1, f);
    int more = fgetc(f);
    fclose(f);
    if (more != EOF || n == cap - 1)
        return false;
    url[n] = 0;
    char *nl = strpbrk(url, "\r\n");
    if (nl)
        *nl = 0;
    return valid_url(url);
}

/* Orbis keyboard uses UTF-16 code units on PS4. Keep URLs ASCII. */
/* 0 = accepted, negatives describe the first failing IME stage. */
static int url_keyboard(char *out, size_t cap, int *api_error, int *last_status)
{
    static uint16_t ime_buffer[OSK_MAX + 1];
    static const uint16_t ime_title[] = {
        'I','n','t','r','o','d','u','c','e',' ','l','a',' ','U','R','L',0
    };
    OrbisImeDialogSetting param;
    OrbisDialogResult result;
    OrbisUserServiceInitializeParams user_params;
    int32_t uid = -1;
    int ret = 0;
    *api_error = 0;
    *last_status = -1;

    /* v0.9 incorrectly passed NULL to this API. */
    memset(&user_params, 0, sizeof(user_params));
    user_params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    ret = sceUserServiceInitialize(&user_params);
    /* This may return already-initialized, which is not fatal. */
    ret = sceUserServiceGetForegroundUser(&uid);
    if (ret < 0 || uid < 0) {
        ret = sceUserServiceGetInitialUser(&uid);
        if (ret < 0 || uid < 0) {
            *api_error = ret;
            return -1;
        }
    }

    ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);
    if (ret < 0) {
        *api_error = ret;
        return -2;
    }

    memset(ime_buffer, 0, sizeof(ime_buffer));
    memset(&param, 0, sizeof(param));
    param.userId = (uint32_t)uid;
    param.type = ORBIS_TYPE_TYPE_URL;
    param.maxTextLength = OSK_MAX;
    param.inputTextBuffer = (wchar_t *)ime_buffer;
    param.title = (const wchar_t *)ime_title;
    param.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;
    param.posx = 960;
    param.posy = 540;
    param.horizontalAlignment = ORBIS_H_CENTER;
    param.verticalAlignment = ORBIS_V_CENTER;

    /* Leave enough time for the previous message dialog to terminate. */
    sceKernelUsleep(180000);
    ret = sceImeDialogInit(&param, NULL);
    if (ret < 0) {
        *api_error = ret;
        return -3;
    }

    /* Account for the initial NONE state as well as RUNNING/STOPPED. */
    int seen_running = 0;
    int finished = 0;
    for (int ticks = 0; ticks < 18000; ticks++) {
        int state = (int)sceImeDialogGetStatus();
        *last_status = state;
        if (state == ORBIS_DIALOG_STATUS_RUNNING)
            seen_running = 1;
        if (state == ORBIS_DIALOG_STATUS_STOPPED) {
            finished = 1;
            break;
        }
        if (state == ORBIS_DIALOG_STATUS_NONE && ticks > 300 && !seen_running)
            break; /* Not shown after 3 seconds */
        sceKernelUsleep(10000);
    }
    if (!finished) {
        sceImeDialogAbort();
        sceImeDialogTerm();
        return -4;
    }

    memset(&result, 0, sizeof(result));
    ret = sceImeDialogGetResult(&result);
    sceImeDialogTerm();
    if (ret < 0) {
        *api_error = ret;
        return -5;
    }
    if (result.endstatus != ORBIS_DIALOG_OK) {
        *api_error = result.endstatus;
        return -6;
    }

    size_t i = 0;
    for (; i + 1 < cap && i < OSK_MAX && ime_buffer[i]; i++) {
        if (ime_buffer[i] < 33 || ime_buffer[i] > 126)
            return -7; /* Only ASCII allowed in URL */
        out[i] = (char)ime_buffer[i];
    }
    out[i] = 0;
    if (!valid_url(out))
        return -8;
    return 0;
}

static void test_url(const char *url, char *message, size_t message_cap)
{
    int net = -1, ssl = -1, http = -1, tpl = -1, conn = -1, req = -1;
    int status = 0, error = -1;
    uint8_t first[PROBE_BYTES] = {0};
    size_t got = 0;

    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SSL) < 0 ||
        sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_HTTP) < 0) {
        error = -1; goto done;
    }
    if (sceNetInit() < 0) { error = -2; goto done; }
    net = sceNetPoolCreate("PS4Direct", 128 * 1024, 0);
    if (net < 0) { error = -3; goto done; }
    ssl = sceSslInit(1024 * 1024);
    if (ssl < 0) { error = -4; goto done; }
    http = sceHttpInit(net, ssl, 1024 * 1024);
    if (http < 0) { error = -5; goto done; }

    tpl = sceHttpCreateTemplate(http, "PS4Direct/0.9",
                                ORBIS_HTTP_VERSION_1_1, 1);
    if (tpl < 0) { error = -6; goto done; }
    /* Keep TLS certificate verification enabled. */
    conn = sceHttpCreateConnectionWithURL(tpl, url, true);
    if (conn < 0) { error = -7; goto done; }
    req = sceHttpCreateRequestWithURL(conn, ORBIS_METHOD_GET, url, 0);
    if (req < 0) { error = -8; goto done; }

    if (sceHttpAddRequestHeader(req, "Range", "bytes=0-1023", 0) < 0) {
        error = -9; goto done;
    }
    sceHttpSetConnectTimeOut(req, 15000000);
    sceHttpSetSendTimeOut(req, 15000000);

    if (sceHttpSendRequest(req, NULL, 0) < 0) { error = -10; goto done; }
    if (sceHttpGetStatusCode(req, &status) < 0) { error = -11; goto done; }
    if (status != 206) { error = -12; goto done; }
    while (got < sizeof(first)) {
        int r = sceHttpReadData(req, first + got, (uint32_t)(sizeof(first) - got));
        if (r < 0) { error = -13; goto done; }
        if (r == 0)
            break;
        got += (size_t)r;
    }
    error = (got == PROBE_BYTES &&
             first[0] == 0x7f && first[1] == 'C' &&
             first[2] == 'N' && first[3] == 'T') ? 0 : -14;

done:
    snprintf(message, message_cap,
        "PS4 Direct PKG v0.9 - resultado\n\n"
        "HTTP: %d\n"
        "Bytes comprobados: %lu / 1024\n"
        "Codigo diagnostico: %d\n"
        "Cabecera PKG: %s\n\n"
        "No descarga el archivo completo ni lo instala.",
        status, (unsigned long)got, error,
        error == 0 ? "RECONOCIDA" : "NO confirmada");
    if (req >= 0) sceHttpDeleteRequest(req);
    if (conn >= 0) sceHttpDeleteConnection(conn);
    if (tpl >= 0) sceHttpDeleteTemplate(tpl);
    if (http >= 0) sceHttpTerm(http);
    if (ssl >= 0) sceSslTerm(ssl);
    if (net >= 0) sceNetPoolDestroy(net);
}

int main(void)
{
    char url[URL_MAX] = {0}, message[512];
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0 ||
        sceCommonDialogInitialize() < 0)
        return 1;

    bool used_file = load_url_file(url, sizeof(url));
    if (!used_file) {
        show_dialog("PS4 Direct PKG v0.9\n\n"
                    "La PS4 funciona sin Windows.\n"
                    "A continuacion se abrira el teclado.\n\n"
                    "Introduce la URL directa de un PKG homebrew autorizado.\n"
                    "Enlaces largos: /data/ps4direct_url.txt\n\n"
                    "Esta version solo diagnostica HTTP.");
        int api_error = 0, ime_status = -1;
        int osk_result = url_keyboard(url, sizeof(url), &api_error, &ime_status);
        if (osk_result != 0) {
            snprintf(message, sizeof(message),
                     "PS4 Direct PKG v0.9\n\n"
                     "El teclado no devolvio una URL valida.\n\n"
                     "Etapa: %d\n"
                     "Error API: 0x%08X\n"
                     "Estado IME: %d\n\n"
                     "-1 usuario / -2 modulo / -3 inicio\n"
                     "-4 espera / -5 resultado / -6 cancelado\n"
                     "-7 caracteres / -8 URL\n\n"
                     "Alternativa: /data/ps4direct_url.txt",
                     osk_result, (unsigned int)api_error, ime_status);
            show_dialog(message);
            return 0;
        }
    }

    test_url(url, message, sizeof(message));
    show_dialog(message);
    return 0;
}