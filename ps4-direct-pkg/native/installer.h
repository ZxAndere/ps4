#ifndef PS4_DIRECT_INSTALLER_H
#define PS4_DIRECT_INSTALLER_H
#include <stdint.h>
typedef struct InstallResult {
    int stage;
    int32_t raw_error;
    int32_t task_id;
    char content_id[37];
    char title_id[32];
} InstallResult;
int install_local_pkg(const char *pkg_path, const char *self_title, InstallResult *out);
#endif
