#include "fcgi_common.h"

typedef struct {
    const unsigned char *data;
    size_t data_len;
    char *filename;
    char *client_md5;
} MultipartFile;

static int extract_boundary(const char *content_type,
                            char *boundary, size_t boundary_size) {
    const char *p;
    const char *end;
    size_t len;

    if (!content_type || !boundary || boundary_size < 2) return 0;
    p = strstr(content_type, "boundary=");
    if (!p) return 0;
    p += strlen("boundary=");
    if (*p == '"') {
        ++p;
        end = strchr(p, '"');
    } else {
        end = p;
        while (*end && *end != ';' && !isspace((unsigned char)*end)) ++end;
    }
    if (!end) return 0;
    len = (size_t)(end - p);
    if (len == 0 || len >= boundary_size) return 0;
    memcpy(boundary, p, len);
    boundary[len] = '\0';
    return 1;
}

static char *header_parameter(const unsigned char *headers, size_t headers_len,
                              const char *name) {
    char pattern[64];
    const unsigned char *start;
    const unsigned char *end;
    int pattern_len = snprintf(pattern, sizeof(pattern), "%s=\"", name);

    if (pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) return NULL;
    start = find_bytes(headers, headers_len, pattern, (size_t)pattern_len);
    if (!start) return NULL;
    start += pattern_len;
    end = find_bytes(start, headers_len - (size_t)(start - headers), "\"", 1);
    if (!end) return NULL;
    return duplicate_bytes(start, (size_t)(end - start));
}

static int parse_multipart_file(const unsigned char *body, size_t body_len,
                                const char *boundary, MultipartFile *file) {
    char delimiter[256];
    char next_delimiter[260];
    int delimiter_len;
    int next_len;
    const unsigned char *part;
    const unsigned char *body_end = body + body_len;

    memset(file, 0, sizeof(*file));
    delimiter_len = snprintf(delimiter, sizeof(delimiter), "--%s", boundary);
    next_len = snprintf(next_delimiter, sizeof(next_delimiter), "\r\n--%s",
                        boundary);
    if (delimiter_len <= 0 || next_len <= 0 ||
        (size_t)delimiter_len >= sizeof(delimiter) ||
        (size_t)next_len >= sizeof(next_delimiter)) return 0;

    part = find_bytes(body, body_len, delimiter, (size_t)delimiter_len);
    while (part) {
        const unsigned char *headers;
        const unsigned char *headers_end;
        const unsigned char *data_start;
        const unsigned char *next;
        char *field_name;
        char *filename;

        part += delimiter_len;
        if ((size_t)(body_end - part) >= 2 && memcmp(part, "--", 2) == 0)
            break;
        if ((size_t)(body_end - part) < 2 || memcmp(part, "\r\n", 2) != 0)
            return 0;
        headers = part + 2;
        headers_end = find_bytes(headers, (size_t)(body_end - headers),
                                 "\r\n\r\n", 4);
        if (!headers_end) return 0;
        data_start = headers_end + 4;
        next = find_bytes(data_start, (size_t)(body_end - data_start),
                          next_delimiter, (size_t)next_len);
        if (!next) return 0;

        field_name = header_parameter(headers,
                                      (size_t)(headers_end - headers), "name");
        filename = header_parameter(headers,
                                    (size_t)(headers_end - headers), "filename");
        if (filename) {
            if ((!field_name || strcmp(field_name, "file") == 0) &&
                !file->filename) {
                file->data = data_start;
                file->data_len = (size_t)(next - data_start);
                file->filename = filename;
            } else {
                free(filename);
            }
        } else if (field_name && strcmp(field_name, "md5") == 0) {
            free(file->client_md5);
            file->client_md5 = duplicate_bytes(data_start,
                                                (size_t)(next - data_start));
            if (!file->client_md5) {
                free(field_name);
                return 0;
            }
        }
        free(field_name);
        part = next + 2;
    }
    return file->filename && file->data_len > 0;
}

static int write_fd(int fd, const unsigned char *data, size_t len) {
    size_t written = 0;
    int ok = 1;

    while (written < len) {
        ssize_t n = write(fd, data + written, len - written);
        if (n <= 0) {
            ok = 0;
            break;
        }
        written += (size_t)n;
    }
    if (close(fd) != 0) ok = 0;
    return ok && written == len;
}

int main(void) {
    int fdfs_initialized;

    mysql_library_init(0, NULL, NULL);
    redis_config_init();
    fdfs_initialized = (cloud_fdfs_init() == 0);
    if (!fdfs_initialized)
        fprintf(stderr, "[upload_fcgi] FastDFS unavailable; using local storage\n");

    while (FCGI_Accept() >= 0) {
        char *token = NULL;
        MYSQL *conn = NULL;
        char *post_data = NULL;
        size_t post_len = 0;
        MultipartFile file = {0};
        char boundary[201];
        char computed_md5[MD5_DIGEST_LENGTH * 2 + 1];
        char stored_file_id[512] = {0};
        const char *storage_type = "local";
        int storage_ok = 0;
        int user_id;

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

        if (!extract_boundary(getenv("CONTENT_TYPE"), boundary, sizeof(boundary))) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"BAD_MULTIPART\",\"message\":\"Missing multipart boundary\"}");
            mysql_close(conn);
            continue;
        }
        post_data = read_post_data(10U * 1024U * 1024U, &post_len);
        if (!post_data ||
            !parse_multipart_file((unsigned char *)post_data, post_len,
                                  boundary, &file)) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"BAD_MULTIPART\",\"message\":\"Invalid file part\"}");
            free(post_data);
            free(file.filename);
            free(file.client_md5);
            mysql_close(conn);
            continue;
        }
        if (strlen(file.filename) == 0 || strlen(file.filename) > 255) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"BAD_FILENAME\",\"message\":\"Invalid filename\"}");
            free(post_data);
            free(file.filename);
            free(file.client_md5);
            mysql_close(conn);
            continue;
        }

        {
            unsigned char digest[MD5_DIGEST_LENGTH];
            MD5(file.data, file.data_len, digest);
            for (int i = 0; i < MD5_DIGEST_LENGTH; ++i)
                sprintf(computed_md5 + i * 2, "%02x", digest[i]);
            computed_md5[32] = '\0';
        }
        if (file.client_md5 && *file.client_md5 &&
            strcasecmp(file.client_md5, computed_md5) != 0) {
            print_json_headers("400 Bad Request");
            printf("{\"code\":\"MD5_MISMATCH\",\"message\":\"File checksum mismatch\"}");
            free(post_data);
            free(file.filename);
            free(file.client_md5);
            mysql_close(conn);
            continue;
        }

        if (fdfs_initialized) {
            char temp_path[] = "/tmp/cloud_upload_XXXXXX";
            int fd = mkstemp(temp_path);
            if (fd >= 0) {
                if (write_fd(fd, file.data, file.data_len) &&
                    cloud_fdfs_upload(temp_path, stored_file_id,
                                      sizeof(stored_file_id)) == 0) {
                    storage_ok = 1;
                    storage_type = "fastdfs";
                }
                unlink(temp_path);
            }
        }
        if (!storage_ok) {
            char local_path[] = UPLOAD_DIR "/cloud_file_XXXXXX";
            int fd = mkstemp(local_path);
            if (fd >= 0) {
                if (write_fd(fd, file.data, file.data_len)) {
                    snprintf(stored_file_id, sizeof(stored_file_id), "%s",
                             local_path);
                    storage_ok = 1;
                } else {
                    unlink(local_path);
                }
            }
        }
        if (!storage_ok) {
            print_json_headers("503 Service Unavailable");
            printf("{\"code\":\"STORAGE_ERROR\",\"message\":\"File storage unavailable\"}");
            free(post_data);
            free(file.filename);
            free(file.client_md5);
            mysql_close(conn);
            continue;
        }

        {
            char esc_filename[512];
            char esc_file_id[1024];
            char insert_query[2048];
            escape_string(conn, file.filename, esc_filename,
                          sizeof(esc_filename));
            escape_string(conn, stored_file_id, esc_file_id,
                          sizeof(esc_file_id));
            snprintf(insert_query, sizeof(insert_query),
                "INSERT INTO files "
                "(user_id, filename, file_id, file_size, file_md5, storage_type, status) "
                "VALUES (%d, '%s', '%s', %zu, '%s', '%s', 'active')",
                user_id, esc_filename, esc_file_id, file.data_len,
                computed_md5, storage_type);
            if (mysql_query(conn, insert_query) != 0) {
                fprintf(stderr, "[upload_fcgi] metadata insert failed: %s\n",
                        mysql_error(conn));
                if (strcmp(storage_type, "fastdfs") == 0)
                    cloud_fdfs_delete(stored_file_id);
                else
                    unlink(stored_file_id);
                print_json_headers("500 Internal Server Error");
                printf("{\"code\":\"DB_ERROR\",\"message\":\"Internal server error\"}");
            } else {
                unsigned long long new_id = mysql_insert_id(conn);
                print_json_headers(NULL);
                printf("{\"code\":\"000\",\"message\":\"Upload success\",\"data\":{\"id\":%llu,\"filename\":",
                       new_id);
                print_json_string(file.filename);
                printf(",\"size\":%llu,\"md5\":\"%s\",\"storage\":\"%s\"}}",
                       (unsigned long long)file.data_len,
                       computed_md5, storage_type);
            }
        }

        free(post_data);
        free(file.filename);
        free(file.client_md5);
        mysql_close(conn);
    }

    if (fdfs_initialized) cloud_fdfs_cleanup();
    mysql_library_end();
    return 0;
}
