#include "fcgi_common.h"

int main() {
    mysql_library_init(0, NULL, NULL);

    while (FCGI_Accept() >= 0) {
        printf("Content-Type: application/json\r\n\r\n");

        char *post_data = read_post_data(4096);
        if (!post_data) {
            printf("{\"code\":\"400\",\"message\":\"Invalid request\"}");
            continue;
        }

        char *username = json_get_string(post_data, "username");
        char *password = json_get_string(post_data, "password");

        if (!username || !password) {
            printf("{\"code\":\"400\",\"message\":\"Missing username or password\"}");
            free(post_data);
            if (username) free(username);
            if (password) free(password);
            continue;
        }

        MYSQL *conn = connect_db();
        if (!conn) {
            printf("{\"code\":\"500\",\"message\":\"DB connection failed\"}");
            free(post_data);
            free(username);
            free(password);
            continue;
        }

        char esc_user[256];
        escape_string(conn, username, esc_user, sizeof(esc_user));

        char hashed_pwd[33];
        md5_hash(password, hashed_pwd);

        char query[512];
        snprintf(query, sizeof(query),
            "SELECT id, username, nickname FROM users WHERE username='%s' AND password='%s' LIMIT 1",
            esc_user, hashed_pwd);

        if (mysql_query(conn, query) != 0) {
            printf("{\"code\":\"500\",\"message\":\"Query error\"}");
            mysql_close(conn);
            free(post_data);
            free(username);
            free(password);
            continue;
        }

        MYSQL_RES *result = mysql_store_result(conn);
        if (mysql_num_rows(result) > 0) {
            MYSQL_ROW row = mysql_fetch_row(result);
            int user_id = atoi(row[0]);
            char *db_username = row[1];
            char *db_nickname = row[2] ? row[2] : "";

            char *token = create_token(conn, user_id);

            printf("{\"code\":\"000\",\"message\":\"Login success\",\"data\":{\"id\":%d,\"username\":\"%s\",\"nickname\":\"%s\",\"token\":\"%s\"}}",
                   user_id, db_username, db_nickname, token);

            char update_query[256];
            snprintf(update_query, sizeof(update_query),
                "UPDATE users SET last_login=NOW() WHERE id=%d", user_id);
            mysql_query(conn, update_query);
        } else {
            printf("{\"code\":\"401\",\"message\":\"Invalid username or password\"}");
        }

        mysql_free_result(result);
        mysql_close(conn);
        free(post_data);
        free(username);
        free(password);
    }

    mysql_library_end();
    return 0;
}
