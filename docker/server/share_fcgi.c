#include "fcgi_common.h"
#include <limits.h>

static int get_strict_json_int(const char *json, const char *key, int *value) {
    char *text = json_get_string(json, key);
    char *end = NULL;
    long parsed;

    if (!text) return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < INT_MIN || parsed > INT_MAX) {
        free(text);
        return -1;
    }
    *value = (int)parsed;
    free(text);
    return 1;
}

static const char *share_base_url(void) {
    const char *configured = getenv("SHARE_BASE_URL");
    return configured && *configured ? configured : "http://localhost:8080";
}

int main(void) {
    mysql_library_init(0, NULL, NULL);
    redis_config_init();

    while (FCGI_Accept() >= 0) {
        char *token = NULL;
        MYSQL *conn = NULL;
        char *post_data = NULL;
        MYSQL_RES *res = NULL;
        char *filename = NULL;
        int user_id;
        int file_id = 0;
        int expire_days = 7;
        int record_found = 0;

        if (!getenv("REQUEST_METHOD") ||
            strcmp(getenv("REQUEST_METHOD"), "POST") != 0) {
            print_json_headers("405 Method Not Allowed");
            printf("{\"code\":\"405\",\"message\":\"POST required\"}");
            continue;
        }
        if (!getenv("CONTENT_TYPE") ||
            strncmp(getenv("CONTENT_TYPE"), "application/json", 16) != 0) {
            print_json_headers("415 Unsupported Media Type");
            printf("{\"code\":\"415\",\"message\":\"application/json required\"}");
            continue;
        }

        token = get_auth_token();
        if (!token) {
            print_json_headers("401 Unauthorized");
            printf("{\"code\":\"401\",\"message\":\"Missing token\"}");
            continue;
        }
        conn = connect_db();
        if (!conn) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"500\",\"message\":\"Internal server error\"}");
            free(token);
            continue;
        }
        user_id = verify_token(conn, token);
        free(token);
        if (user_id < 0) {
            print_json_headers("503 Service Unavailable");
            printf("{\"code\":\"TOKEN_STORE_UNAVAILABLE\",\"message\":\"Authentication service unavailable\"}");
            mysql_close(conn);
            continue;
        }
        if (user_id == 0) {
            print_json_headers("401 Unauthorized");
            printf("{\"code\":\"401\",\"message\":\"Invalid token\"}");
            mysql_close(conn);
            continue;
        }

        post_data = read_post_data(4096, NULL);
        if (!post_data || get_strict_json_int(post_data, "file_id", &file_id) != 1) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            free(post_data);
            mysql_close(conn);
            continue;
        }
        {
            int expire_result = get_strict_json_int(post_data, "expire_days",
                                                    &expire_days);
            if (expire_result < 0 || expire_days < 1 || expire_days > 365) {
                print_json_headers("400 Bad Request");
                printf("{\"code\":\"400\",\"message\":\"expire_days must be between 1 and 365\"}");
                free(post_data);
                mysql_close(conn);
                continue;
            }
        }
        free(post_data);
        post_data = NULL;
        if (file_id <= 0) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            mysql_close(conn);
            continue;
        }

        {
            char query[512];
            snprintf(query, sizeof(query),
                "SELECT filename FROM files "
                "WHERE id=%d AND user_id=%d AND status='active' LIMIT 1",
                file_id, user_id);
            if (mysql_query(conn, query) != 0) {
                fprintf(stderr, "[share_fcgi] ownership query failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"500\",\"message\":\"Internal server error\"}");
                mysql_close(conn);
                continue;
            }
        }
        res = mysql_store_result(conn);
        if (!res) {
            fprintf(stderr, "[share_fcgi] store result failed: %s\n",
                    mysql_error(conn));
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"500\",\"message\":\"Internal server error\"}");
            mysql_close(conn);
            continue;
        }
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                record_found = 1;
                filename = row[0] ? strdup(row[0]) : strdup("unknown");
            }
        }
        mysql_free_result(res);
        res = NULL;
        if (!filename) {
            if (record_found) {
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"500\",\"message\":\"Internal server error\"}");
            } else {
                print_json_headers("404 Not Found");
                printf("{\"code\":\"404\",\"message\":\"File not found\"}");
            }
            mysql_close(conn);
            continue;
        }

        {
            char share_token[33];
            char update_query[512];
            char expire_str[64];
            char share_url[512];
            time_t expire_time = time(NULL) + (time_t)expire_days * 24 * 3600;
            struct tm tm_info;

            if (!generate_token(share_token, sizeof(share_token))) {
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"500\",\"message\":\"Token generation failed\"}");
                free(filename);
                mysql_close(conn);
                continue;
            }
            if (!localtime_r(&expire_time, &tm_info) ||
                strftime(expire_str, sizeof(expire_str),
                         "%Y-%m-%dT%H:%M:%S%z", &tm_info) == 0) {
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"500\",\"message\":\"Time conversion failed\"}");
                free(filename);
                mysql_close(conn);
                continue;
            }
            snprintf(update_query, sizeof(update_query),
                "UPDATE files SET share_token='%s', "
                "share_expire=FROM_UNIXTIME(%lld) "
                "WHERE id=%d AND user_id=%d AND status='active'",
                share_token, (long long)expire_time, file_id, user_id);
            if (mysql_query(conn, update_query) != 0) {
                fprintf(stderr, "[share_fcgi] update failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"500\",\"message\":\"Internal server error\"}");
            } else if (mysql_affected_rows(conn) != 1) {
                print_json_headers("404 Not Found");
                printf("{\"code\":\"404\",\"message\":\"File not found\"}");
            } else {
                const char *base_url = share_base_url();
                size_t base_len = strlen(base_url);
                snprintf(share_url, sizeof(share_url), "%.*s/download?share_token=%s",
                         (int)(base_len > 0 && base_url[base_len - 1] == '/'
                               ? base_len - 1 : base_len),
                         base_url, share_token);
                print_json_headers(NULL);
                printf("{\"code\":\"000\",\"data\":{\"share_url\":");
                print_json_string(share_url);
                printf(",\"filename\":");
                print_json_string(filename);
                printf(",\"expire\":");
                print_json_string(expire_str);
                printf(",\"expire_days\":%d}}", expire_days);
            }
        }

        free(filename);
        mysql_close(conn);
    }

    mysql_library_end();
    return 0;
}
