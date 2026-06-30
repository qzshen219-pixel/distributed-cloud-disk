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
        char *nickname = json_get_string(post_data, "nickname");
        char *email = json_get_string(post_data, "email");
        char *phone = json_get_string(post_data, "phone");

        if (!username || !password) {
            printf("{\"code\":\"400\",\"message\":\"Missing username or password\"}");
            free(post_data);
            if (username) free(username);
            if (password) free(password);
            if (nickname) free(nickname);
            if (email) free(email);
            if (phone) free(phone);
            continue;
        }

        MYSQL *conn = connect_db();
        if (!conn) {
            printf("{\"code\":\"500\",\"message\":\"DB connection failed\"}");
            free(post_data);
            free(username);
            free(password);
            if (nickname) free(nickname);
            if (email) free(email);
            if (phone) free(phone);
            continue;
        }

        char esc_user[256], esc_nick[256], esc_email[256], esc_phone[64];
        escape_string(conn, username, esc_user, sizeof(esc_user));
        escape_string(conn, nickname ? nickname : "", esc_nick, sizeof(esc_nick));
        escape_string(conn, email ? email : "", esc_email, sizeof(esc_email));
        escape_string(conn, phone ? phone : "", esc_phone, sizeof(esc_phone));

        char check_query[512];
        snprintf(check_query, sizeof(check_query),
            "SELECT id FROM users WHERE username='%s' LIMIT 1", esc_user);

        if (mysql_query(conn, check_query) != 0) {
            printf("{\"code\":\"500\",\"message\":\"Query error\"}");
            mysql_close(conn);
            free(post_data);
            free(username);
            free(password);
            if (nickname) free(nickname);
            if (email) free(email);
            if (phone) free(phone);
            continue;
        }

        MYSQL_RES *check_result = mysql_store_result(conn);
        if (mysql_num_rows(check_result) > 0) {
            printf("{\"code\":\"409\",\"message\":\"Username already exists\"}");
            mysql_free_result(check_result);
            mysql_close(conn);
            free(post_data);
            free(username);
            free(password);
            if (nickname) free(nickname);
            if (email) free(email);
            if (phone) free(phone);
            continue;
        }
        mysql_free_result(check_result);

        char hashed_pwd[33];
        md5_hash(password, hashed_pwd);

        char insert_query[1024];
        snprintf(insert_query, sizeof(insert_query),
            "INSERT INTO users (username, password, nickname, email, phone) VALUES ('%s','%s','%s','%s','%s')",
            esc_user, hashed_pwd, esc_nick, esc_email, esc_phone);

        if (mysql_query(conn, insert_query) == 0) {
            int new_id = mysql_insert_id(conn);
            printf("{\"code\":\"002\",\"message\":\"Register success\",\"data\":{\"id\":%d,\"username\":\"%s\",\"nickname\":\"%s\"}}",
                   new_id, esc_user, esc_nick);
        } else {
            printf("{\"code\":\"500\",\"message\":\"Register failed: %s\"}", mysql_error(conn));
        }

        mysql_close(conn);
        free(post_data);
        free(username);
        free(password);
        if (nickname) free(nickname);
        if (email) free(email);
        if (phone) free(phone);
    }

    mysql_library_end();
    return 0;
}
