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

        char *post_data = read_post_data(10 * 1024 * 1024);
        if (!post_data) {
            printf("{\"code\":\"400\",\"message\":\"Invalid request\"}");
            mysql_close(conn);
            continue;
        }

        char *username = multipart_get_field(post_data, "user");
        char *filename = multipart_get_filename(post_data);
        char *md5 = multipart_get_attr(post_data, "md5");

        char *file_start = strstr(post_data, "\r\n\r\n");
        if (!file_start) {
            printf("{\"code\":\"400\",\"message\":\"No file content\"}");
            mysql_close(conn);
            free(post_data);
            if (username) free(username);
            if (filename) free(filename);
            if (md5) free(md5);
            continue;
        }
        file_start += 4;

        char *boundary = strstr(post_data, "--");
        char *file_end = file_start;
        if (boundary) {
            file_end = strstr(file_start, boundary);
            if (!file_end || file_end <= file_start)
                file_end = post_data + strlen(post_data);
        } else {
            file_end = post_data + strlen(post_data);
        }

        while (file_end > file_start && (*(file_end - 1) == '\r' || *(file_end - 1) == '\n'))
            file_end--;

        int file_len = file_end - file_start;
        if (file_len <= 0) {
            printf("{\"code\":\"400\",\"message\":\"Empty file\"}");
            mysql_close(conn);
            free(post_data);
            if (username) free(username);
            if (filename) free(filename);
            if (md5) free(md5);
            continue;
        }

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "/home/cloud/uploads/%ld_%d",
                 time(NULL), rand());

        FILE *fp = fopen(file_path, "wb");
        if (fp) {
            fwrite(file_start, 1, file_len, fp);
            fclose(fp);
        }

        char *final_md5 = NULL;
        if (!md5 || strlen(md5) == 0) {
            unsigned char digest[MD5_DIGEST_LENGTH];
            MD5((unsigned char*)file_start, file_len, digest);
            final_md5 = (char*)malloc(33);
            for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
                sprintf(final_md5 + (i * 2), "%02x", digest[i]);
            final_md5[32] = '\0';
        } else {
            final_md5 = strdup(md5);
        }

        char esc_file_id[512], esc_md5[64];
        escape_string(conn, file_path, esc_file_id, sizeof(esc_file_id));
        escape_string(conn, final_md5, esc_md5, sizeof(esc_md5));

        char *disp_filename = filename ? filename : "unknown";

        char insert_query[1024];
        snprintf(insert_query, sizeof(insert_query),
            "INSERT INTO files (user_id, filename, file_id, file_size, file_md5) "
            "VALUES (%d, '%s', '%s', %d, '%s')",
            user_id, disp_filename, esc_file_id, file_len, esc_md5);

        if (mysql_query(conn, insert_query) == 0) {
            int new_id = mysql_insert_id(conn);
            printf("{\"code\":\"000\",\"message\":\"Upload success\",\"data\":{\"id\":%d,\"filename\":\"%s\",\"path\":\"%s\",\"size\":%d,\"md5\":\"%s\"}}",
                   new_id, disp_filename, file_path, file_len, final_md5);
        } else {
            printf("{\"code\":\"500\",\"message\":\"DB insert failed\"}");
        }

        mysql_close(conn);
        free(post_data);
        if (username) free(username);
        if (filename) free(filename);
        if (md5) free(md5);
        free(final_md5);
    }

    mysql_library_end();
    return 0;
}
