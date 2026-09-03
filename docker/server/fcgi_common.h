
#ifndef FCGI_COMMON_H
#define FCGI_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fcgi_stdio.h>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "fdfs_client.h"
#include "logger.h"

#define DB_HOST "localhost"
#define DB_USER "cloud_user"
#define DB_PASS "Cloud@2026#Secure"
#define DB_NAME "file_cloud"
#define SERVER_IP "localhost"
#define FDFS_CONF "/etc/fdfs/client.conf"
#define UPLOAD_DIR "/home/cloud/uploads"
#define DEFAULT_REDIS_HOST "127.0.0.1"
#define DEFAULT_REDIS_PORT 6379
#define DEFAULT_REDIS_MASTER_NAME "cloud-master"
#define DEFAULT_TOKEN_TTL 3600
#define TOKEN_KEY_PREFIX "cloud:token:"
#define USER_TOKEN_KEY_PREFIX "cloud:user_token:"

static inline int cloud_fdfs_init(void) {
    log_init();
    g_log_context.log_level = LOG_ERR;
    ignore_signal_pipe();
    return fdfs_client_init(FDFS_CONF);
}

static inline void cloud_fdfs_cleanup(void) {
    fdfs_client_destroy();
}

static inline int cloud_fdfs_upload(const char *local_path,
                                    char *file_id, size_t file_id_size) {
    ConnectionInfo *tracker;
    ConnectionInfo storage;
    char group_name[FDFS_GROUP_NAME_MAX_LEN + 1] = {0};
    char uploaded_id[128] = {0};
    int store_path_index = -1;
    int result;

    if (!local_path || !file_id || file_id_size == 0) return EINVAL;
    tracker = tracker_get_connection();
    if (!tracker) return errno ? errno : ECONNREFUSED;
    result = tracker_query_storage_store(tracker, &storage, group_name,
                                         &store_path_index);
    if (result == 0) {
        result = storage_upload_by_filename1(tracker, &storage,
            store_path_index, local_path, NULL, NULL, 0,
            group_name, uploaded_id);
    }
    tracker_disconnect_server_ex(tracker, true);
    if (result == 0) snprintf(file_id, file_id_size, "%s", uploaded_id);
    return result;
}

static inline int cloud_fdfs_download(const char *file_id,
                                      const char *local_path,
                                      int64_t *file_size) {
    ConnectionInfo *tracker;
    int result;

    if (!file_id || !local_path || !file_size) return EINVAL;
    tracker = tracker_get_connection();
    if (!tracker) return errno ? errno : ECONNREFUSED;
    result = storage_download_file_to_file1(tracker, NULL, file_id,
                                            local_path, file_size);
    tracker_disconnect_server_ex(tracker, true);
    return result;
}

static inline int cloud_fdfs_delete(const char *file_id) {
    ConnectionInfo *tracker;
    int result;

    if (!file_id) return EINVAL;
    tracker = tracker_get_connection();
    if (!tracker) return errno ? errno : ECONNREFUSED;
    result = storage_delete_file1(tracker, NULL, file_id);
    tracker_disconnect_server_ex(tracker, true);
    return result;
}

MYSQL* connect_db() {
    MYSQL *conn = mysql_init(NULL);
    if (!conn) return NULL;
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, NULL, 0)) {
        mysql_close(conn);
        return NULL;
    }
    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}

void md5_hash(const char *input, char *output) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input, strlen(input), digest);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(output + (i * 2), "%02x", digest[i]);
    output[32] = '\0';
}

void escape_string(MYSQL *conn, const char *input, char *output, size_t max_len) {
    if (!input) { output[0] = '\0'; return; }
    size_t input_len = strlen(input);
    size_t safe_len = max_len > 0 ? (max_len - 1) / 2 : 0;
    if (input_len > safe_len) input_len = safe_len;
    mysql_real_escape_string(conn, output, input, input_len);
}

static inline const unsigned char *find_bytes(const unsigned char *data,
                                               size_t data_len,
                                               const void *pattern,
                                               size_t pattern_len) {
    if (!data || !pattern || pattern_len == 0 || pattern_len > data_len)
        return NULL;
    for (size_t i = 0; i <= data_len - pattern_len; ++i) {
        if (memcmp(data + i, pattern, pattern_len) == 0) return data + i;
    }
    return NULL;
}

static inline char *duplicate_bytes(const unsigned char *data, size_t len) {
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, data, len);
    copy[len] = '\0';
    return copy;
}

static inline void print_json_string(const char *value) {
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    printf("\"");
    for (; *p; ++p) {
        switch (*p) {
            case '"': printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if (*p < 0x20) printf("\\u%04x", *p);
                else printf("%c", *p);
        }
    }
    printf("\"");
}

static inline void print_json_headers(const char *status) {
    if (status) printf("Status: %s\r\n", status);
    printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
}

char* json_get_string(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *p = strstr(json, pattern);
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ') p++;
    if (*p == '"') {
        p++;
        char *end = strchr(p, '"');
        if (!end) return NULL;
        int len = end - p;
        char *val = (char*)malloc(len + 1);
        strncpy(val, p, len);
        val[len] = '\0';
        return val;
    }
    char *end = p;
    while (*end && *end != ',' && *end != '}' && *end != ' ') end++;
    int len = end - p;
    if (len == 0) return NULL;
    char *val = (char*)malloc(len + 1);
    strncpy(val, p, len);
    val[len] = '\0';
    return val;
}

int json_get_int(const char *json, const char *key, int default_val) {
    char *val = json_get_string(json, key);
    if (!val) return default_val;
    int result = atoi(val);
    free(val);
    return result;
}

char* read_post_data(size_t max_len, size_t *out_len) {
    char *cl_str = getenv("CONTENT_LENGTH");
    char *end = NULL;
    unsigned long long parsed = cl_str ? strtoull(cl_str, &end, 10) : 0;
    if (!cl_str || end == cl_str || *end != '\0' || parsed == 0 ||
        parsed > max_len || parsed > SIZE_MAX - 1) return NULL;
    size_t content_length = (size_t)parsed;
    char *data = (char*)malloc(content_length + 1);
    if (!data) return NULL;
    size_t i = 0;
    while (i < content_length) {
        int ch = getchar();
        if (ch == EOF) break;
        data[i++] = ch;
    }
    if (i != content_length) {
        free(data);
        return NULL;
    }
    data[i] = '\0';
    if (out_len) *out_len = i;
    return data;
}

char* get_query_param(const char *param_name) {
    char *qs = getenv("QUERY_STRING");
    size_t name_len;
    const char *part;

    if (!qs || !param_name) return NULL;
    name_len = strlen(param_name);
    part = qs;
    while (*part) {
        const char *end = strchr(part, '&');
        const char *equals;
        size_t part_len = end ? (size_t)(end - part) : strlen(part);

        equals = memchr(part, '=', part_len);
        if (equals && (size_t)(equals - part) == name_len &&
            memcmp(part, param_name, name_len) == 0) {
            return duplicate_bytes((const unsigned char *)(equals + 1),
                                   part_len - name_len - 1);
        }
        if (!end) break;
        part = end + 1;
    }
    return NULL;
}

int generate_secure_token(char *token, size_t len) {
    size_t random_len;
    unsigned char random_bytes[64];

    if (!token || len < 3 || (len - 1) % 2 != 0) return 0;
    random_len = (len - 1) / 2;
    if (random_len > sizeof(random_bytes) ||
        RAND_bytes(random_bytes, (int)random_len) != 1) return 0;
    for (size_t i = 0; i < random_len; ++i)
        sprintf(token + i * 2, "%02x", random_bytes[i]);
    token[len - 1] = '\0';
    return 1;
}

int generate_token(char *token, int len) {
    if (len <= 0) return 0;
    return generate_secure_token(token, (size_t)len);
}

char* multipart_get_field(const char *data, const char *field_name) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "name=\"%s\"", field_name);
    char *p = strstr(data, pattern);
    if (!p) return NULL;
    p = strstr(p, "\r\n\r\n");
    if (!p) return NULL;
    p += 4;
    char *end = strstr(p, "\r\n--");
    if (!end) end = p + strlen(p);
    int len = end - p;
    char *val = (char*)malloc(len + 1);
    strncpy(val, p, len);
    val[len] = '\0';
    return val;
}

char* multipart_get_filename(const char *data) {
    char *p = strstr(data, "filename=\"");
    if (!p) return NULL;
    p += 10;
    char *end = strchr(p, '"');
    if (!end) return NULL;
    int len = end - p;
    char *val = (char*)malloc(len + 1);
    strncpy(val, p, len);
    val[len] = '\0';
    return val;
}

char* multipart_get_attr(const char *data, const char *attr_name) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s=\"", attr_name);
    char *p = strstr(data, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    char *end = strchr(p, '"');
    if (!end) return NULL;
    int len = end - p;
    char *val = (char*)malloc(len + 1);
    strncpy(val, p, len);
    val[len] = '\0';
    return val;
}

static inline const char *env_or_default(const char *name,
                                         const char *default_value) {
    const char *value = getenv(name);
    return value && *value ? value : default_value;
}

static inline int env_port_or_default(const char *name, int default_value) {
    const char *value = getenv(name);
    char *end = NULL;
    long port;

    if (!value || !*value) return default_value;
    port = strtol(value, &end, 10);
    if (end == value || *end != '\0' || port < 1 || port > 65535)
        return default_value;
    return (int)port;
}

static inline int token_ttl_seconds(void) {
    const char *value = getenv("REDIS_TOKEN_TTL");
    char *end = NULL;
    long ttl;

    if (!value || !*value) return DEFAULT_TOKEN_TTL;
    ttl = strtol(value, &end, 10);
    if (end == value || *end != '\0' || ttl < 60 || ttl > 604800)
        return DEFAULT_TOKEN_TTL;
    return (int)ttl;
}

typedef struct {
    char host[256];
    char sentinels[1024];
    char master_name[128];
    char password[256];
    char sentinel_password[256];
    int port;
    int token_ttl;
    int initialized;
} RedisConfig;

static RedisConfig redis_config;

static inline void copy_redis_setting(char *destination, size_t size,
                                      const char *name,
                                      const char *default_value) {
    const char *value = env_or_default(name, default_value);
    snprintf(destination, size, "%s", value);
}

// FCGI_Accept 会用请求参数替换 getenv() 可见的环境，因此必须在接收请求前缓存配置。
static inline void redis_config_init(void) {
    if (redis_config.initialized) return;
    copy_redis_setting(redis_config.host, sizeof(redis_config.host),
                       "REDIS_HOST", DEFAULT_REDIS_HOST);
    copy_redis_setting(redis_config.sentinels, sizeof(redis_config.sentinels),
                       "REDIS_SENTINELS", "");
    copy_redis_setting(redis_config.master_name,
                       sizeof(redis_config.master_name),
                       "REDIS_MASTER_NAME", DEFAULT_REDIS_MASTER_NAME);
    copy_redis_setting(redis_config.password, sizeof(redis_config.password),
                       "REDIS_PASSWORD", "");
    copy_redis_setting(redis_config.sentinel_password,
                       sizeof(redis_config.sentinel_password),
                       "REDIS_SENTINEL_PASSWORD", "");
    redis_config.port = env_port_or_default("REDIS_PORT", DEFAULT_REDIS_PORT);
    redis_config.token_ttl = token_ttl_seconds();
    redis_config.initialized = 1;
}

static inline int redis_authenticate(redisContext *context,
                                     const char *password) {
    redisReply *reply;
    int ok;

    if (!password || !*password) return 1;
    reply = (redisReply *)redisCommand(context, "AUTH %b",
                                       password, strlen(password));
    ok = reply && reply->type == REDIS_REPLY_STATUS && reply->str &&
         strcmp(reply->str, "OK") == 0;
    if (!ok) {
        fprintf(stderr, "Redis AUTH failed: %s\n",
                reply && reply->str ? reply->str : "no reply");
    }
    if (reply) freeReplyObject(reply);
    return ok;
}

static inline redisContext *redis_connect_node(const char *host, int port,
                                               const char *password) {
    struct timeval timeout = {2, 0};
    redisContext *context = redisConnectWithTimeout(host, port, timeout);

    if (!context) {
        fprintf(stderr, "Redis connection allocation failed for %s:%d\n",
                host, port);
        return NULL;
    }
    if (context->err || !redis_authenticate(context, password)) {
        if (context->err) {
            fprintf(stderr, "Redis connection failed for %s:%d: %s\n",
                    host, port, context->errstr);
        }
        redisFree(context);
        return NULL;
    }
    return context;
}

static inline redisContext *redis_connect_master(void) {
    const char *sentinel_list;
    const char *redis_password;

    redis_config_init();
    sentinel_list = redis_config.sentinels;
    redis_password = redis_config.password;

    if (!sentinel_list || !*sentinel_list) {
        return redis_connect_node(redis_config.host, redis_config.port,
                                  redis_password);
    }

    char *list_copy = strdup(sentinel_list);
    char *saveptr = NULL;
    char *entry;
    redisContext *master = NULL;
    const char *master_name = redis_config.master_name;
    const char *sentinel_password = redis_config.sentinel_password;

    if (!list_copy) return NULL;
    for (entry = strtok_r(list_copy, ",", &saveptr);
         entry && !master;
         entry = strtok_r(NULL, ",", &saveptr)) {
        char *colon;
        char *end = NULL;
        long port;
        redisContext *sentinel;
        redisReply *reply;

        while (isspace((unsigned char)*entry)) ++entry;
        colon = strrchr(entry, ':');
        if (!colon) continue;
        *colon = '\0';
        port = strtol(colon + 1, &end, 10);
        if (!*entry || end == colon + 1 || *end != '\0' ||
            port < 1 || port > 65535) continue;

        sentinel = redis_connect_node(entry, (int)port, sentinel_password);
        if (!sentinel) continue;
        reply = (redisReply *)redisCommand(
            sentinel, "SENTINEL get-master-addr-by-name %s", master_name);
        if (reply && reply->type == REDIS_REPLY_ARRAY &&
            reply->elements == 2 && reply->element[0]->str &&
            reply->element[1]->str) {
            char *master_port_end = NULL;
            long master_port = strtol(reply->element[1]->str,
                                      &master_port_end, 10);
            if (master_port_end != reply->element[1]->str &&
                *master_port_end == '\0' && master_port > 0 &&
                master_port <= 65535) {
                master = redis_connect_node(reply->element[0]->str,
                                            (int)master_port,
                                            redis_password);
            }
        } else {
            fprintf(stderr, "Redis Sentinel %s:%ld lookup failed: %s\n",
                    entry, port,
                    reply && reply->str ? reply->str : "unexpected reply");
        }
        if (reply) freeReplyObject(reply);
        redisFree(sentinel);
    }
    free(list_copy);
    return master;
}

// Token 认证：验证请求中的 token 是否有效
// 返回用户 ID（>0 表示有效），0 表示无效，-1 表示 Redis 不可用
int verify_token(MYSQL *conn, const char *token) {
    char key[256];
    char *end = NULL;
    long parsed_user_id;
    int user_id = 0;
    redisContext *redis;
    redisReply *reply;

    (void)conn;
    if (!token || !*token || strlen(token) > 128) return 0;
    if (snprintf(key, sizeof(key), "%s%s", TOKEN_KEY_PREFIX, token) >=
        (int)sizeof(key)) return 0;

    redis = redis_connect_master();
    if (!redis) return -1;
    reply = (redisReply *)redisCommand(redis, "GET %b", key, strlen(key));
    if (!reply) {
        redisFree(redis);
        return -1;
    }
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        parsed_user_id = strtol(reply->str, &end, 10);
        if (end != reply->str && *end == '\0' && parsed_user_id > 0 &&
            parsed_user_id <= INT32_MAX)
            user_id = (int)parsed_user_id;
    } else if (reply->type == REDIS_REPLY_ERROR) {
        user_id = -1;
    }
    freeReplyObject(reply);
    redisFree(redis);
    return user_id;
}

// 生成 token 并保存到 Redis；同一用户再次登录会撤销旧 token
char* create_token(MYSQL *conn, int user_id) {
    static char token[64];
    char rand_part[33];
    char token_key[256];
    char user_key[128];
    char user_id_text[32];
    redisContext *redis;
    redisReply *reply;
    int ttl;
    static const char replace_token_script[] =
        "local old=redis.call('GET',KEYS[1]);"
        "if old then redis.call('DEL',ARGV[1]..old) end;"
        "redis.call('SET',KEYS[2],ARGV[2],'EX',ARGV[3]);"
        "redis.call('SET',KEYS[1],ARGV[4],'EX',ARGV[3]);"
        "return 1";

    (void)conn;
    redis_config_init();
    ttl = redis_config.token_ttl;
    if (user_id <= 0) return NULL;
    if (!generate_token(rand_part, sizeof(rand_part))) return NULL;
    snprintf(token, sizeof(token), "%d_%s", user_id, rand_part);
    if (snprintf(token_key, sizeof(token_key), "%s%s",
                 TOKEN_KEY_PREFIX, token) >= (int)sizeof(token_key) ||
        snprintf(user_key, sizeof(user_key), "%s%d",
                 USER_TOKEN_KEY_PREFIX, user_id) >= (int)sizeof(user_key) ||
        snprintf(user_id_text, sizeof(user_id_text), "%d", user_id) >=
            (int)sizeof(user_id_text)) return NULL;

    redis = redis_connect_master();
    if (!redis) return NULL;
    reply = (redisReply *)redisCommand(
        redis, "EVAL %b 2 %b %b %b %b %d %b",
        replace_token_script, strlen(replace_token_script),
        user_key, strlen(user_key), token_key, strlen(token_key),
        TOKEN_KEY_PREFIX, strlen(TOKEN_KEY_PREFIX),
        user_id_text, strlen(user_id_text), ttl, token, strlen(token));
    if (!reply || reply->type != REDIS_REPLY_INTEGER || reply->integer != 1) {
        fprintf(stderr, "Redis token write failed: %s\n",
                reply && reply->str ? reply->str : redis->errstr);
        if (reply) freeReplyObject(reply);
        redisFree(redis);
        return NULL;
    }
    freeReplyObject(reply);

    // Sentinel 部署下至少等待一个副本确认，避免主节点刚写入就故障时丢失 Token。
    if (redis_config.sentinels[0]) {
        reply = (redisReply *)redisCommand(redis, "WAIT 1 2000");
        if (!reply || reply->type != REDIS_REPLY_INTEGER ||
            reply->integer < 1) {
            fprintf(stderr, "Redis token replication failed\n");
            if (reply) freeReplyObject(reply);
            redisFree(redis);
            return NULL;
        }
        freeReplyObject(reply);
    }
    redisFree(redis);
    return token;
}

// 从请求头获取 token
char* get_auth_token() {
    char *token = getenv("HTTP_X_AUTH_TOKEN");
    if (token && strlen(token) > 0) return strdup(token);

    char *qs = getenv("QUERY_STRING");
    if (qs) return get_query_param("token");

    return NULL;
}

#endif
