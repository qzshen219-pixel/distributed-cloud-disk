#include "fcgi_common.h"
#include <sys/types.h>

static int open_safe_local_file(const char *stored_id) {
    char root[512];
    char candidate[1024];
    char resolved[1024];
    size_t root_len;

    if (!stored_id || !realpath(UPLOAD_DIR, root)) return -1;
    if (stored_id[0] == '/')
        snprintf(candidate, sizeof(candidate), "%s", stored_id);
    else
        snprintf(candidate, sizeof(candidate), "%s/%s", UPLOAD_DIR, stored_id);
    if (!realpath(candidate, resolved)) return -1;
    root_len = strlen(root);
    if (strncmp(resolved, root, root_len) != 0 || resolved[root_len] != '/')
        return -1;
    return open(resolved, O_RDONLY);
}

static void print_content_disposition(const char *filename) {
    const unsigned char *p = (const unsigned char *)(filename ? filename : "download");
    printf("Content-Disposition: attachment; filename=\"download\"; filename*=UTF-8''");
    for (; *p; ++p) {
        if (isalnum(*p) || *p == '-' || *p == '.' || *p == '_' || *p == '~')
            putchar(*p);
        else
            printf("%%%02X", *p);
    }
    printf("\r\n");
}

static const char *content_type_for(const char *filename) {
    const char *ext = filename ? strrchr(filename, '.') : NULL;
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".json") == 0) return "application/json";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".png") == 0) return "image/png";
    if (strcasecmp(ext, ".gif") == 0) return "image/gif";
    if (strcasecmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcasecmp(ext, ".zip") == 0) return "application/zip";
    if (strcasecmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcasecmp(ext, ".mp4") == 0) return "video/mp4";
    return "application/octet-stream";
}

int main(void) {
    int fdfs_initialized;

    mysql_library_init(0, NULL, NULL);
    redis_config_init();
    fdfs_initialized = (cloud_fdfs_init() == 0);
    if (!fdfs_initialized)
        fprintf(stderr, "[download_fcgi] FastDFS unavailable\n");

    while (FCGI_Accept() >= 0) {
        char *token = NULL;
        char *share_token = NULL;
        char *file_id_str = NULL;
        MYSQL *conn = NULL;
        MYSQL_RES *res = NULL;
        char *stored_id = NULL;
        char *filename = NULL;
        char *storage_type = NULL;
        int file_fd = -1;
        char temp_path[] = "/tmp/cloud_download_XXXXXX";
        int temp_created = 0;
        int record_found = 0;
        int user_id = 0;
        int file_id = 0;
        off_t actual_size;

        share_token = get_query_param("share_token");
        if (!share_token) {
            token = get_auth_token();
            if (!token) {
                print_json_headers("401 Unauthorized");
                printf("{\"code\":\"AUTH_REQUIRED\",\"message\":\"Missing token\"}");
                continue;
            }
        }
        conn = connect_db();
        if (!conn) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            free(token);
            free(share_token);
            continue;
        }
        if (!share_token) {
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
                printf("{\"code\":\"INVALID_TOKEN\",\"message\":\"Invalid token\"}");
                mysql_close(conn);
                continue;
            }

            file_id_str = get_query_param("file_id");
            file_id = file_id_str ? atoi(file_id_str) : 0;
            free(file_id_str);
            if (file_id <= 0) {
                print_json_headers("400 Bad Request");
                printf("{\"code\":\"BAD_FILE_ID\",\"message\":\"Invalid file_id\"}");
                mysql_close(conn);
                continue;
            }
        }

        {
            char query[768];
            if (share_token) {
                char esc_share_token[129];
                if (strlen(share_token) != 32) {
                    print_json_headers("404 Not Found");
                    printf("{\"code\":\"NOT_FOUND\",\"message\":\"Share not found or expired\"}");
                    free(share_token);
                    mysql_close(conn);
                    continue;
                }
                escape_string(conn, share_token, esc_share_token,
                              sizeof(esc_share_token));
                snprintf(query, sizeof(query),
                    "SELECT id, file_id, filename, storage_type FROM files "
                    "WHERE share_token='%s' AND share_expire>NOW() "
                    "AND status='active' LIMIT 1",
                    esc_share_token);
            } else {
                snprintf(query, sizeof(query),
                    "SELECT id, file_id, filename, storage_type FROM files "
                    "WHERE id=%d AND user_id=%d AND status='active' LIMIT 1",
                    file_id, user_id);
            }
            if (mysql_query(conn, query) != 0) {
                fprintf(stderr, "[download_fcgi] query failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
                free(share_token);
                mysql_close(conn);
                continue;
            }
        }
        res = mysql_store_result(conn);
        if (!res) {
            fprintf(stderr, "[download_fcgi] store result failed: %s\n",
                    mysql_error(conn));
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            free(share_token);
            mysql_close(conn);
            continue;
        }
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                record_found = 1;
                file_id = row[0] ? atoi(row[0]) : 0;
                stored_id = row[1] ? strdup(row[1]) : NULL;
                filename = row[2] ? strdup(row[2]) : strdup("download");
                storage_type = row[3] ? strdup(row[3]) : strdup("local");
            }
        }
        mysql_free_result(res);
        res = NULL;
        free(share_token);
        share_token = NULL;
        if (!stored_id || !filename || !storage_type) {
            if (!record_found) {
                print_json_headers("404 Not Found");
                printf("{\"code\":\"NOT_FOUND\",\"message\":\"File not found\"}");
            } else {
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"MEMORY_ERROR\",\"message\":\"Internal server error\"}");
            }
            free(stored_id);
            free(filename);
            free(storage_type);
            mysql_close(conn);
            continue;
        }

        if (strcmp(storage_type, "fastdfs") == 0) {
            int fd;
            int64_t fdfs_size = 0;
            if (!fdfs_initialized) {
                print_json_headers("503 Service Unavailable");
                printf("{\"code\":\"STORAGE_ERROR\",\"message\":\"File storage unavailable\"}");
                goto request_cleanup;
            }
            fd = mkstemp(temp_path);
            if (fd >= 0) {
                close(fd);
                temp_created = 1;
                if (cloud_fdfs_download(stored_id, temp_path, &fdfs_size) == 0)
                    file_fd = open(temp_path, O_RDONLY);
            }
        } else if (strcmp(storage_type, "local") == 0) {
            file_fd = open_safe_local_file(stored_id);
        }
        if (file_fd < 0) {
            print_json_headers("404 Not Found");
            printf("{\"code\":\"NOT_FOUND\",\"message\":\"File not found\"}");
            goto request_cleanup;
        }
        if ((actual_size = lseek(file_fd, 0, SEEK_END)) < 0 ||
            lseek(file_fd, 0, SEEK_SET) < 0) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"READ_ERROR\",\"message\":\"Internal server error\"}");
            goto request_cleanup;
        }

        printf("Content-Type: %s\r\n", content_type_for(filename));
        print_content_disposition(filename);
        printf("Content-Length: %lld\r\n", (long long)actual_size);
        printf("Access-Control-Allow-Origin: *\r\n\r\n");
        {
            unsigned char buffer[8192];
            off_t total_sent = 0;
            ssize_t n;
            int write_failed = 0;
            while ((n = read(file_fd, buffer, sizeof(buffer))) > 0) {
                if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n) {
                    write_failed = 1;
                    break;
                }
                total_sent += (off_t)n;
            }
            if (!write_failed && n == 0 && total_sent == actual_size) {
                char update[256];
                snprintf(update, sizeof(update),
                    "UPDATE files SET download_count=download_count+1 "
                    "WHERE id=%d", file_id);
                if (mysql_query(conn, update) != 0)
                    fprintf(stderr, "[download_fcgi] counter update failed: %s\n",
                            mysql_error(conn));
            }
        }

request_cleanup:
        if (file_fd >= 0) close(file_fd);
        if (temp_created) unlink(temp_path);
        free(stored_id);
        free(filename);
        free(storage_type);
        mysql_close(conn);
    }

    if (fdfs_initialized) cloud_fdfs_cleanup();
    mysql_library_end();
    return 0;
}
