#include "fcgi_common.h"

int main() {
    mysql_library_init(0, NULL, NULL);

    while (FCGI_Accept() >= 0) {
        printf("Content-Type: application/json\r\n\r\n");

        char *token = get_auth_token();
        if (!token) {
            printf("{\"code\":\"401\",\"message\":\"Missing token\"}");
            continue;
        }

        MYSQL *conn = connect_db();
        if (!conn) {
            printf("{\"code\":\"500\",\"message\":\"DB connection failed\"}");
            free(token);
            continue;
        }

        int user_id = verify_token(conn, token);
        free(token);

        if (user_id <= 0) {
            printf("{\"code\":\"401\",\"message\":\"Invalid token\"}");
            mysql_close(conn);
            continue;
        }

        char *post_data = read_post_data(4096);
        if (!post_data) {
            printf("{\"code\":\"400\",\"message\":\"Invalid request\"}");
            mysql_close(conn);
            continue;
        }

        int file_id = json_get_int(post_data, "file_id", 0);
        int expire_days = json_get_int(post_data, "expire_days", 7);
        free(post_data);

        if (file_id <= 0) {
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            mysql_close(conn);
            continue;
        }

        if (expire_days <= 0) expire_days = 7;
        if (expire_days > 365) expire_days = 365;

        char check_query[256];
        snprintf(check_query, sizeof(check_query),
            "SELECT id, filename FROM files WHERE id=%d AND user_id=%d", file_id, user_id);

        if (mysql_query(conn, check_query) != 0) {
            printf("{\"code\":\"500\",\"message\":\"Query error\"}");
            mysql_close(conn);
            continue;
        }

        MYSQL_RES *res = mysql_store_result(conn);
        MYSQL_ROW row = mysql_fetch_row(res);

        if (!row) {
            printf("{\"code\":\"404\",\"message\":\"File not found\"}");
            mysql_free_result(res);
            mysql_close(conn);
            continue;
        }

        char *filename = row[1];
        mysql_free_result(res);

        char share_token[33];
        generate_token(share_token, sizeof(share_token));

        time_t expire_time = time(NULL) + expire_days * 24 * 3600;

        char esc_token[64];
        escape_string(conn, share_token, esc_token, sizeof(esc_token));

        char update_query[512];
        snprintf(update_query, sizeof(update_query),
            "UPDATE files SET share_token='%s', share_expire=FROM_UNIXTIME(%ld) WHERE id=%d AND user_id=%d",
            esc_token, expire_time, file_id, user_id);

        if (mysql_query(conn, update_query) == 0) {
            char expire_str[64];
            struct tm *tm_info = localtime(&expire_time);
            strftime(expire_str, sizeof(expire_str), "%Y-%m-%d %H:%M:%S", tm_info);

            printf("{\"code\":\"000\",\"data\":{\"share_url\":\"http://%s/share?token=%s\",\"filename\":\"%s\",\"expire\":\"%s\",\"expire_days\":%d}}",
                   SERVER_IP, esc_token, filename, expire_str, expire_days);
        } else {
            printf("{\"code\":\"500\",\"message\":\"Share failed\"}");
        }

        mysql_close(conn);
    }

    mysql_library_end();
    return 0;
}
