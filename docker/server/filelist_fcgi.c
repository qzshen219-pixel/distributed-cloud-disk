#include "fcgi_common.h"

int main() {
    mysql_library_init(0, NULL, NULL);
    redis_config_init();

    while (FCGI_Accept() >= 0) {
        char *token = get_auth_token();
        if (!token) {
            print_json_headers("401 Unauthorized");
            printf("{\"code\":\"AUTH_REQUIRED\",\"message\":\"Missing token\"}");
            continue;
        }

        MYSQL *conn = connect_db();
        if (!conn) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            free(token);
            continue;
        }

        int user_id = verify_token(conn, token);
        free(token);

        if (user_id < 0) {
            print_json_headers("503 Service Unavailable");
            printf("{\"code\":\"TOKEN_STORE_UNAVAILABLE\",\"message\":\"Authentication service unavailable\"}");
            mysql_close(conn);
            continue;
        }
        if (user_id == 0) {
            print_json_headers("401 Unauthorized");
            printf("{\"code\":\"INVALID_TOKEN\",\"message\":\"Invalid token\"}");
            mysql_close(conn);
            continue;
        }

        char query[1024];
        snprintf(query, sizeof(query),
            "SELECT id, filename, file_size, "
            "DATE_FORMAT(upload_time, '%%Y-%%m-%%d %%H:%%i:%%s'), "
            "download_count, file_md5 "
            "FROM files WHERE user_id=%d AND status='active' "
            "ORDER BY upload_time DESC", user_id);

        if (mysql_query(conn, query) != 0) {
            fprintf(stderr, "[filelist_fcgi] query failed: %s\n",
                    mysql_error(conn));
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            mysql_close(conn);
            continue;
        }

        MYSQL_RES *res = mysql_store_result(conn);
        if (!res) {
            fprintf(stderr, "[filelist_fcgi] store result failed: %s\n",
                    mysql_error(conn));
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            mysql_close(conn);
            continue;
        }
        my_ulonglong num_rows = mysql_num_rows(res);

        print_json_headers(NULL);
        printf("{\"code\":\"000\",\"files\":[");

        MYSQL_ROW row;
        int first = 1;
        while ((row = mysql_fetch_row(res))) {
            if (!first) printf(",");
            first = 0;
            printf("{\"id\":%s,\"name\":", row[0] ? row[0] : "0");
            print_json_string(row[1]);
            printf(",\"size\":%s,\"time\":", row[2] ? row[2] : "0");
            print_json_string(row[3]);
            printf(",\"downloads\":%s,\"md5\":",
                   row[4] ? row[4] : "0");
            print_json_string(row[5]);
            printf("}");
        }

        printf("],\"total\":%llu}", (unsigned long long)num_rows);

        mysql_free_result(res);
        mysql_close(conn);
    }

    mysql_library_end();
    return 0;
}
