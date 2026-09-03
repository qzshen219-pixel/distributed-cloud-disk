#include "fcgi_common.h"

static int delete_local_file(const char *stored_id) {
    char root[512];
    char candidate[1024];
    char resolved[1024];
    size_t root_len;

    if (!stored_id || !realpath(UPLOAD_DIR, root)) return EINVAL;
    if (stored_id[0] == '/')
        snprintf(candidate, sizeof(candidate), "%s", stored_id);
    else
        snprintf(candidate, sizeof(candidate), "%s/%s", UPLOAD_DIR, stored_id);
    if (!realpath(candidate, resolved)) return errno == ENOENT ? 0 : errno;
    root_len = strlen(root);
    if (strncmp(resolved, root, root_len) != 0 || resolved[root_len] != '/')
        return EPERM;
    return unlink(resolved) == 0 || errno == ENOENT ? 0 : errno;
}

int main(void) {
    int fdfs_initialized;

    mysql_library_init(0, NULL, NULL);
    redis_config_init();
    fdfs_initialized = (cloud_fdfs_init() == 0);
    if (!fdfs_initialized)
        fprintf(stderr, "[delete_fcgi] FastDFS unavailable\n");

    while (FCGI_Accept() >= 0) {
        char *token = NULL;
        MYSQL *conn = NULL;
        char *post_data = NULL;
        MYSQL_RES *res = NULL;
        char *filename = NULL;
        char *stored_id = NULL;
        char *storage_type = NULL;
        int user_id;
        int file_id;
        int record_found = 0;
        int storage_result = EINVAL;

        token = get_auth_token();
        if (!token) {
            print_json_headers("401 Unauthorized");
            printf("{\"code\":\"AUTH_REQUIRED\",\"message\":\"Missing token\"}");
            continue;
        }
        conn = connect_db();
        if (!conn) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
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
            printf("{\"code\":\"INVALID_TOKEN\",\"message\":\"Invalid token\"}");
            mysql_close(conn);
            continue;
        }

        post_data = read_post_data(4096, NULL);
        file_id = post_data ? json_get_int(post_data, "file_id", 0) : 0;
        free(post_data);
        if (file_id <= 0) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"BAD_FILE_ID\",\"message\":\"Invalid file_id\"}");
            mysql_close(conn);
            continue;
        }

        {
            char query[512];
            snprintf(query, sizeof(query),
                "SELECT filename, file_id, storage_type FROM files "
                "WHERE id=%d AND user_id=%d AND status='active' LIMIT 1",
                file_id, user_id);
            if (mysql_query(conn, query) != 0) {
                fprintf(stderr, "[delete_fcgi] query failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
                mysql_close(conn);
                continue;
            }
        }
        res = mysql_store_result(conn);
        if (!res) {
            fprintf(stderr, "[delete_fcgi] store result failed: %s\n",
                    mysql_error(conn));
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            mysql_close(conn);
            continue;
        }
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                record_found = 1;
                filename = row[0] ? strdup(row[0]) : strdup("unknown");
                stored_id = row[1] ? strdup(row[1]) : NULL;
                storage_type = row[2] ? strdup(row[2]) : strdup("local");
            }
        }
        mysql_free_result(res);
        res = NULL;
        if (!record_found) {
            print_json_headers("404 Not Found");
            printf("{\"code\":\"NOT_FOUND\",\"message\":\"File not found\"}");
            mysql_close(conn);
            continue;
        }
        if (!filename || !stored_id || !storage_type) {
            print_json_headers("500 Internal Server Error");
            printf("{\"code\":\"MEMORY_ERROR\",\"message\":\"Internal server error\"}");
            goto request_cleanup;
        }

        {
            char update[256];
            snprintf(update, sizeof(update),
                "UPDATE files SET status='deleting' "
                "WHERE id=%d AND user_id=%d AND status='active'",
                file_id, user_id);
            if (mysql_query(conn, update) != 0 || mysql_affected_rows(conn) != 1) {
                fprintf(stderr, "[delete_fcgi] status update failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
                goto request_cleanup;
            }
        }

        if (strcmp(storage_type, "fastdfs") == 0)
            storage_result = fdfs_initialized ? cloud_fdfs_delete(stored_id)
                                              : ECONNREFUSED;
        else if (strcmp(storage_type, "local") == 0)
            storage_result = delete_local_file(stored_id);

        {
            char update[256];
            snprintf(update, sizeof(update),
                "UPDATE files SET status='%s' WHERE id=%d AND user_id=%d",
                storage_result == 0 ? "deleted" : "delete_failed",
                file_id, user_id);
            if (mysql_query(conn, update) != 0) {
                fprintf(stderr, "[delete_fcgi] final status update failed: %s\n",
                        mysql_error(conn));
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
                goto request_cleanup;
            }
        }
        if (storage_result != 0) {
            fprintf(stderr, "[delete_fcgi] physical delete failed: %d\n",
                    storage_result);
            print_json_headers("503 Service Unavailable");
            printf("{\"code\":\"STORAGE_ERROR\",\"message\":\"File deletion pending retry\"}");
        } else {
            print_json_headers(NULL);
            printf("{\"code\":\"000\",\"message\":\"File deleted\",\"data\":{\"id\":%d,\"filename\":",
                   file_id);
            print_json_string(filename);
            printf("}}");
        }

request_cleanup:
        free(filename);
        free(stored_id);
        free(storage_type);
        mysql_close(conn);
    }

    if (fdfs_initialized) cloud_fdfs_cleanup();
    mysql_library_end();
    return 0;
}
