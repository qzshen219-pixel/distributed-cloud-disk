#ifndef FCGI_COMMON_H
#define FCGI_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcgi_stdio.h>
#include <mysql/mysql.h>
#include <openssl/md5.h>
#include <time.h>

#define DB_HOST "localhost"
#define DB_USER "cloud_user"
#define DB_PASS "Cloud@2026#Secure"
#define DB_NAME "file_cloud"
#define SERVER_IP "localhost"

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

void escape_string(MYSQL *conn, const char *input, char *output, int max_len) {
    if (!input) { output[0] = '\0'; return; }
    mysql_real_escape_string(conn, output, input, strlen(input));
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

char* read_post_data(int max_len) {
    char *cl_str = getenv("CONTENT_LENGTH");
    int content_length = cl_str ? atoi(cl_str) : 0;
    if (content_length <= 0 || content_length >= max_len) return NULL;
    char *data = (char*)malloc(content_length + 1);
    if (!data) return NULL;
    int i = 0;
    while (i < content_length) {
        int ch = getchar();
        if (ch == EOF) break;
        data[i++] = ch;
    }
    data[i] = '\0';
    return data;
}

char* get_query_param(const char *param_name) {
    char *qs = getenv("QUERY_STRING");
    if (!qs) return NULL;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s=", param_name);
    char *p = strstr(qs, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    char *end = p;
    while (*end && *end != '&' && *end != ' ') end++;
    int len = end - p;
    char *val = (char*)malloc(len + 1);
    strncpy(val, p, len);
    val[len] = '\0';
    return val;
}

void generate_token(char *token, int len) {
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL) ^ (unsigned long)token);
    for (int i = 0; i < len - 1; i++)
        token[i] = chars[rand() % 62];
    token[len - 1] = '\0';
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

// Token 认证：验证请求中的 token 是否有效
// 返回用户 ID（>0 表示有效），0 表示无效
int verify_token(MYSQL *conn, const char *token) {
    if (!token || strlen(token) == 0) return 0;

    char esc_token[256];
    escape_string(conn, token, esc_token, sizeof(esc_token));

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT id FROM users WHERE token='%s' LIMIT 1", esc_token);

    if (mysql_query(conn, query) != 0) return 0;

    MYSQL_RES *res = mysql_store_result(conn);
    if (!res) return 0;

    int user_id = 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) user_id = atoi(row[0]);

    mysql_free_result(res);
    return user_id;
}

// 生成并保存 token 到数据库
char* create_token(MYSQL *conn, int user_id) {
    static char token[64];
    char rand_part[33];
    generate_token(rand_part, sizeof(rand_part));
    snprintf(token, sizeof(token), "%d_%s", user_id, rand_part);

    char esc_token[128];
    escape_string(conn, token, esc_token, sizeof(esc_token));

    char query[256];
    snprintf(query, sizeof(query),
        "UPDATE users SET token='%s' WHERE id=%d", esc_token, user_id);
    mysql_query(conn, query);

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
