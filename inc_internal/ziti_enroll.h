/*
Copyright (c) 2019-2020 NetFoundry, Inc.

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


#ifndef ZITI_SDK_ENROLL_H
#define ZITI_SDK_ENROLL_H

#include <uv_mbed/queue.h>

#include "internal_model.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct ziti_controller_s ziti_controller;

typedef struct enroll_cfg_s {

    ziti_enroll_cb external_enroll_cb;
    void *external_enroll_ctx;

    ziti_enrollment_jwt_header *zejh;
    ziti_enrollment_jwt *zej;
    char *raw_jwt;
    unsigned char *jwt_signing_input;
    char *jwt_sig;
    size_t jwt_sig_len;

    tls_context *tls;
    ziti_controller *ctrl;

    char *CA;

    const char *private_key;
    tls_private_key pk;
    const char *own_cert;
    const char *name;

    char *csr_pem;
} enroll_cfg;


struct ziti_enroll_req {
    ziti_enroll_cb enroll_cb;
    void *external_enroll_ctx;
    uv_loop_t *loop;
    ziti_controller *controller;
    enroll_cfg *ecfg;
};


#ifdef __cplusplus
}
#endif

#endif //ZITI_SDK_ENROLL_H
