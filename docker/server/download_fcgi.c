#include "fcgi_common.h"

int main() {
    mysql_library_init(0, NULL, NULL);

    while (FCGI_Accept() >= 0) {
        char *file_id_str = get_query_param("file_id");
        if (!file_id_str) {
            printf("Status: 400 Bad Request\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"400\",\"message\":\"Missing file_id\"}");
            continue;
        }

        int file_id = atoi(file_id_str);
        free(file_id_str);

        if (file_id <= 0) {
            printf("Status: 400 Bad Request\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            continue;
        }

        MYSQL *conn = connect_db();
        if (!conn) {
            printf("Status: 500 Internal Server Error\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"500\",\"message\":\"DB connection failed\"}");
            continue;
        }

        char query[512];
        snprintf(query, sizeof(query),
            "SELECT file_id, filename, file_size, file_md5 FROM files WHERE id=%d", file_id);

        if (mysql_query(conn, query) != 0) {
            printf("Status: 500 Internal Server Error\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"500\",\"message\":\"Query error\"}");
            mysql_close(conn);
            continue;
        }

        MYSQL_RES *res = mysql_store_result(conn);
        MYSQL_ROW row = mysql_fetch_row(res);

        if (!row) {
            printf("Status: 404 Not Found\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"404\",\"message\":\"File not found\"}");
            mysql_free_result(res);
            mysql_close(conn);
            continue;
        }

        char *stored_path = row[0];
        char *filename = row[1];
        long file_size = row[2] ? atol(row[2]) : 0;

        snprintf(query, sizeof(query),
            "UPDATE files SET download_count=download_count+1 WHERE id=%d", file_id);
        mysql_query(conn, query);

        mysql_free_result(res);
        mysql_close(conn);

        FILE *fp = NULL;
        long actual_size = 0;

        if (stored_path[0] == '/') {
            fp = fopen(stored_path, "rb");
        }

        if (!fp) {
            char alt_path[512];
            snprintf(alt_path, sizeof(alt_path), "/home/s/uploads/%s", stored_path);
            fp = fopen(alt_path, "rb");
        }

        if (!fp) {
            printf("Status: 404 Not Found\r\n");
            printf("Content-Type: application/json\r\n\r\n");
            printf("{\"code\":\"404\",\"message\":\"File not found on disk: %s\"}", stored_path);
            continue;
        }

        fseek(fp, 0, SEEK_END);
        actual_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        const char *content_type = "application/octet-stream";
        if (filename) {
            const char *ext = strrchr(filename, '.');
            if (ext) {
                if (strcasecmp(ext, ".txt") == 0) content_type = "text/plain; charset=utf-8";
                else if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) content_type = "text/html; charset=utf-8";
                else if (strcasecmp(ext, ".json") == 0) content_type = "application/json";
                else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) content_type = "image/jpeg";
                else if (strcasecmp(ext, ".png") == 0) content_type = "image/png";
                else if (strcasecmp(ext, ".gif") == 0) content_type = "image/gif";
                else if (strcasecmp(ext, ".pdf") == 0) content_type = "application/pdf";
                else if (strcasecmp(ext, ".zip") == 0) content_type = "application/zip";
                else if (strcasecmp(ext, ".mp3") == 0) content_type = "audio/mpeg";
                else if (strcasecmp(ext, ".mp4") == 0) content_type = "video/mp4";
            }
        }

        printf("Content-Type: %s\r\n", content_type);
        printf("Content-Disposition: attachment; filename=\"%s\"\r\n", filename ? filename : "download");
        printf("Content-Length: %ld\r\n", actual_size);
        printf("Access-Control-Allow-Origin: *\r\n");
        printf("\r\n");

        char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
            fwrite(buf, 1, n, stdout);
        }
        fclose(fp);
    }

    mysql_library_end();
    return 0;
}
