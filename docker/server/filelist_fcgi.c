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

        char query[1024];
        snprintf(query, sizeof(query),
            "SELECT id, filename, file_size, "
            "DATE_FORMAT(upload_time, '%%Y-%%m-%%d %%H:%%i:%%s'), "
            "download_count, file_md5 "
            "FROM files WHERE user_id=%d ORDER BY upload_time DESC", user_id);

        if (mysql_query(conn, query) != 0) {
            printf("{\"code\":\"500\",\"message\":\"Query error\"}");
            mysql_close(conn);
            continue;
        }

        MYSQL_RES *res = mysql_store_result(conn);
        int num_rows = mysql_num_rows(res);

        printf("{\"code\":\"000\",\"files\":[");

        MYSQL_ROW row;
        int first = 1;
        while ((row = mysql_fetch_row(res))) {
            if (!first) printf(",");
            first = 0;
            printf("{\"id\":%s,\"name\":\"%s\",\"size\":%s,\"time\":\"%s\",\"downloads\":%s,\"md5\":\"%s\"}",
                   row[0] ? row[0] : "0",
                   row[1] ? row[1] : "",
                   row[2] ? row[2] : "0",
                   row[3] ? row[3] : "",
                   row[4] ? row[4] : "0",
                   row[5] ? row[5] : "");
        }

        printf("],\"total\":%d}", num_rows);

        mysql_free_result(res);
        mysql_close(conn);
    }

    mysql_library_end();
    return 0;
}
