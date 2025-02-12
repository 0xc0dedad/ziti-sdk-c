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

#include <ziti/ziti.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <uv.h>


#if _WIN32
#define realpath(in, out) _fullpath(out, in, FILENAME_MAX)
#endif
#define DIE(v) do { \
int code = (v);\
if (code != ZITI_OK) {\
fprintf(stderr, "ERROR: " #v " => %s\n", ziti_errorstr(code));\
exit(code);\
}} while(0)

const char *output_file;

static int write_identity_file(ziti_config *cfg) {
    FILE *f;

    size_t len;
    char *output_buf = ziti_config_to_json(cfg, 0, &len);

    if ((f = fopen(output_file, "wb")) == NULL) {
        return (-1);
    }

    int rc = ZITI_OK;
    if (fwrite(output_buf, 1, len, f) != len) {
        rc = -1;
    }

    free(output_buf);
    fclose( f );

    return rc;
}

void on_ziti_enroll(ziti_config *cfg, int status, char *err_message, void *ctx) {

    if (status != ZITI_OK) {
        fprintf(stderr, "ERROR: => %d => %s\n", status, err_message);
        exit(status);
    }


    int rc = write_identity_file(cfg);

    DIE(rc);
}

struct enroll_cert {
    const char *key;
    const char *cert;
};
#include <ziti/zitilib.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <JWT file> <ID file> [key_file] [cert_file]\n", argv[0]);
        exit(1);
    }

#if _WIN32
    //changes the output to UTF-8 so that the windows output looks correct and not all jumbly
    SetConsoleOutputCP(65001);
#endif
#if 0
    uv_loop_t *loop = uv_default_loop();

    output_file = argv[2];

    ziti_enroll_opts opts = {0};
    struct enroll_cert cert;
    opts.jwt = argv[1];

    if (argc > 3) {
        opts.enroll_key = realpath(argv[3], NULL);
    }

    if (argc > 4) {
        opts.enroll_cert = realpath(argv[4], NULL);
    }

    DIE(ziti_enroll(&opts, loop, on_ziti_enroll, NULL));

    // loop will finish after the request is complete and ziti_shutdown is called
    uv_run(loop, UV_RUN_DEFAULT);

    printf("\nSuccess\n");
#endif
    FILE *jwt_file = fopen(argv[1], "r");
    if (jwt_file == NULL) {
        perror("failed to open JWT file");
        return 1;
    }
    char jwt[8 * 1024];
    fgets(jwt, sizeof(jwt), jwt_file);
    fclose(jwt_file);


    Ziti_lib_init();
    char *cfg;
    size_t len;
    int rc = Ziti_enroll_identity(jwt, NULL, NULL, &cfg, &len);
    if (rc == ZITI_OK) {
        printf("%.*s\n", (int)len, cfg);
    } else {
        printf("err = %d(%s)\n", rc, ziti_errorstr(rc));
    }
    Ziti_lib_shutdown();
}
