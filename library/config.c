/*
Copyright 2019-2020 NetFoundry, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <utils.h>
#include "zt_internal.h"

const char* ZITI_SDK_CONFIG = "ZITI_SDK_CONFIG";

const char* APP_ID = NULL;
const char* APP_VERSION = NULL;

void ziti_set_app_info(const char *app_id, const char *app_version) {
    FREE(APP_ID);
    FREE(APP_VERSION);
    APP_ID = strdup(app_id);
    APP_VERSION = strdup(app_version);
}

int load_config_file(const char *filename, ziti_config **cfg) {
    struct stat stats;
    int s = stat(filename, &stats);
    if (s == -1) {
        ZITI_LOG(ERROR, "%s - %s", filename, strerror(errno));
        return ZITI_CONFIG_NOT_FOUND;
    }

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        ZITI_LOG(ERROR, "%s - %s", filename, strerror(errno));
        return ZITI_CONFIG_NOT_FOUND;
    }

    size_t config_len = (size_t) stats.st_size;
    char *config = malloc(config_len);
    size_t rc;
    if ((rc = fread(config, 1, config_len, file)) != config_len) {
        ZITI_LOG(WARN, "failed to read config in full [%zd/%zd]: %s(%d)", rc, config_len, strerror(errno), errno);
    }
    fclose(file);

    ziti_config *c;
    if (parse_ziti_config_ptr(&c, config, config_len) < 0) {
        free_ziti_config(c);
        free(config);
        FREE(c);
        return ZITI_INVALID_CONFIG;
    }

    *cfg = c;
    free(config);

    return ZITI_OK;
}

int load_config(const char *filename, ziti_config **cfg) {
    if (filename != NULL) {
        return load_config_file(filename, cfg);
    }

    char *fn = getenv(ZITI_SDK_CONFIG);
    if (fn != NULL) {
        return load_config_file(fn, cfg);
    }

    char def[1024];
    sprintf(def, "%s/.netfoundry/ziti/id.json", getenv("HOME"));
    return load_config_file(def, cfg);
}
