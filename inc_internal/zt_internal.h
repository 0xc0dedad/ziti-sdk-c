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

#ifndef ZT_SDK_ZT_INTERNAL_H
#define ZT_SDK_ZT_INTERNAL_H


#include <stdbool.h>
#include <uv_mbed/uv_mbed.h>
#include <uv_mbed/queue.h>

#include <ziti/ziti.h>
#include "buffer.h"
#include "message.h"
#include "ziti_enroll.h"
#include "ziti_ctrl.h"
#include "metrics.h"
#include "edge_protocol.h"

#include <sodium.h>

//#define SIZEOF(arr) (sizeof(arr) / sizeof((arr)[0]))

#if !defined(UUID_STR_LEN)
#define UUID_STR_LEN 37
#endif

#define TYPE_DIAL "Dial"
#define TYPE_BIND "Bind"

extern const char *APP_ID;
extern const char *APP_VERSION;

typedef struct ziti_channel ziti_channel_t;

typedef void (*reply_cb)(void *ctx, message *m, int err);

typedef void (*send_cb)(int status, void *ctx);

typedef void (*ch_connect_cb)(ziti_channel_t *ch, void *ctx, int status);

typedef void (*ch_notify_state)(ziti_channel_t *ch, ziti_router_status status, void *ctx);

typedef int ch_state;
typedef int conn_state;

typedef struct ziti_channel {
    uv_loop_t *loop;
    struct ziti_ctx *ctx;
    char *name;
    char *version;
    char *host;
    int port;

    uint32_t id;
    char token[UUID_STR_LEN];
    uv_mbed_t connection;

    // multi purpose timer:
    // - reconnect timeout if not connected
    // - connect timeout when connecting
    // - latency interval/timeout if connected
    uv_timer_t timer;

    uint64_t latency;
    struct waiter_s *latency_waiter;

    ch_state state;
    uint32_t reconnect_count;

    LIST_HEAD(conn_reqs, ch_conn_req) conn_reqs;
    uint32_t msg_seq;

    buffer *incoming;

    message *in_next;
    int in_body_offset;

    LIST_HEAD(con_list, msg_receiver) receivers;
    LIST_HEAD(waiter, waiter_s) waiters;

    ch_notify_state notify_cb;
    void *notify_ctx;
} ziti_channel_t;

struct ziti_write_req_s {
    struct ziti_conn *conn;
    uint8_t *buf;
    size_t len;

    uint8_t *payload; // internal buffer
    ziti_write_cb cb;
    uv_timer_t *timeout;

    void *ctx;
};

struct ziti_conn {
    char *token;
    char *service;
    char *source_identity;
    struct ziti_conn_req *conn_req;

    uint32_t edge_msg_seq;
    uint32_t conn_id;

    struct ziti_ctx *ziti_ctx;
    ziti_channel_t *channel;
    ziti_data_cb data_cb;
    ziti_client_cb client_cb;
    ziti_close_cb close_cb;
    conn_state state;
    bool fin_sent;
    bool fin_recv;
    bool close;
    int timeout;

    buffer *inbound;
    uv_check_t *flusher;
    uv_async_t *disconnector;
    int write_reqs;

    void *data;

    struct ziti_conn *parent;
    uint32_t dial_req_seq;

    uint8_t sk[crypto_kx_SECRETKEYBYTES];
    uint8_t pk[crypto_kx_PUBLICKEYBYTES];
    uint8_t *rx;
    uint8_t *tx;

    crypto_secretstream_xchacha20poly1305_state crypt_o;
    crypto_secretstream_xchacha20poly1305_state crypt_i;
    bool encrypted;

    LIST_ENTRY(ziti_conn) next;
};

struct process {
    char *path;
    bool is_running;
    char *sha_512_hash;
    char **signers;
    int num_signers;
};

struct posture_checks {
    uv_timer_t timer;
    double interval;

    // map<type/process_path,response>
    model_map *previous_responses;

    // map<type/process_path,response>
    model_map *current_responses;

    char *previous_session_id;
    bool must_send;
    bool must_send_every_time;
};

struct ziti_ctx {
    ziti_options *opts;
    ziti_controller controller;
    uint32_t id;

    tls_context *tlsCtx;

    int ctrl_status;
    ziti_session *session;

    // map<name,ziti_service>
    model_map services;
    // map<service_id,ziti_net_session>
    model_map sessions;

    bool no_service_updates_api; // controller API has no last-update endpoint
    bool no_bulk_posture_response_api; // controller API does not support bulk posture response submission
    bool no_current_edge_routers;

    char *last_update;

    uv_timer_t session_timer;
    uv_timer_t refresh_timer;
    uv_prepare_t reaper;

    uv_loop_t *loop;
    uv_thread_t loop_thread;

    // map<erUrl,ziti_channel>
    model_map channels;
    LIST_HEAD(conns, ziti_conn) connections;

    uint32_t conn_seq;

    /* options */
    int ziti_timeout;

    /* context wide metrics */
    rate_t up_rate;
    rate_t down_rate;

    /* posture check support */
    struct posture_checks *posture_checks;
};

#ifdef __cplusplus
extern "C" {
#endif

void ziti_invalidate_session(ziti_context ztx, ziti_net_session *session, const char *service_id, const char *type);

void ziti_on_channel_event(ziti_channel_t *ch, ziti_router_status status, ziti_context ztx);

void ziti_force_session_refresh(ziti_context ztx);

int ziti_close_channels(ziti_context ztx);

bool ziti_channel_is_connected(ziti_channel_t *ch);

int ziti_channel_connect(ziti_context ztx, const char *name, const char *url, ch_connect_cb, void *ctx);

int ziti_channel_close(ziti_channel_t *ch);

void ziti_channel_add_receiver(ziti_channel_t *ch, int id, void *receiver, void (*receive_f)(void *, message *, int));

void ziti_channel_rem_receiver(ziti_channel_t *ch, int id);

int ziti_channel_send(ziti_channel_t *ch, uint32_t content, const hdr_t *hdrs, int nhdrs, const uint8_t *body,
                      uint32_t body_len,
                      struct ziti_write_req_s *ziti_write);

struct waiter_s *
ziti_channel_send_for_reply(ziti_channel_t *ch, uint32_t content, const hdr_t *headers, int nhdrs, const uint8_t *body,
                            uint32_t body_len, reply_cb,
                            void *reply_ctx);

void ziti_channel_remove_waiter(ziti_channel_t *ch, struct waiter_s *waiter);

int load_config(const char *filename, ziti_config **);

int load_jwt(const char *filename, struct enroll_cfg_s *ecfg, ziti_enrollment_jwt_header **, ziti_enrollment_jwt **);

int load_tls(ziti_config *cfg, tls_context **tls);

int ziti_bind(ziti_connection conn, const char *service, ziti_listen_opts *listen_opts, ziti_listen_cb listen_cb,
              ziti_client_cb on_clt_cb);

void conn_inbound_data_msg(ziti_connection conn, message *msg);

int ziti_write_req(struct ziti_write_req_s *req);

void on_write_completed(struct ziti_conn *conn, struct ziti_write_req_s *req, int status);

int close_conn_internal(struct ziti_conn *conn);

const char *ziti_conn_state(ziti_connection conn);

int establish_crypto(ziti_connection conn, message *msg);

void ziti_fmt_time(char *time_str, size_t time_str_len, uv_timeval64_t *tv);

#ifdef __cplusplus
}
#endif
#endif //ZT_SDK_ZT_INTERNAL_H
