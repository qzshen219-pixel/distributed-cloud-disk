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
        free(post_data);

        if (file_id <= 0) {
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            mysql_close(conn);
            continue;
        }

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

        char del_query[256];
        snprintf(del_query, sizeof(del_query), "DELETE FROM files WHERE id=%d AND user_id=%d", file_id, user_id);

        if (mysql_query(conn, del_query) == 0 && mysql_affected_rows(conn) > 0) {
            printf("{\"code\":\"000\",\"message\":\"File deleted\",\"data\":{\"id\":%d,\"filename\":\"%s\"}}",
                   file_id, filename);
        } else {
            printf("{\"code\":\"500\",\"message\":\"Delete failed\"}");
        }

        mysql_close(conn);
    }

    mysql_library_end();
    return 0;
}
