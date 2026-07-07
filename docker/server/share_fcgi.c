#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcgi_stdio.h>
#include <mysql/mysql.h>
#include <time.h>

#define DB_HOST "localhost"
#define DB_USER "cloud_user"
#define DB_PASS "Cloud@2026#Secure"
#define DB_NAME "file_cloud"

// 生成分享 Token
void generate_token(char *token, int len) {
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand(time(NULL));
    for (int i = 0; i < len - 1; i++) {
        token[i] = chars[rand() % 62];
    }
    token[len - 1] = '\0';
}

MYSQL* connect_db() {
    MYSQL *conn = mysql_init(NULL);
    if (!conn) return NULL;
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, NULL, 0)) {
        mysql_close(conn);
        return NULL;
    }
    return conn;
}

int main() {
    mysql_library_init(0, NULL, NULL);
    
    while (FCGI_Accept() >= 0) {
        char *content_length_str = getenv("CONTENT_LENGTH");
        int len = content_length_str ? atoi(content_length_str) : 0;
        
        printf("Content-Type: application/json\r\n");
        printf("\r\n");
        
        if (len <= 0) {
            printf("{\"code\":\"400\",\"message\":\"No data\"}");
            continue;
        }
        
        char *data = (char*)malloc(len + 1);
        fread(data, 1, len, stdin);
        data[len] = '\0';
        
        int file_id = 0;
        int expire_days = 7;
        sscanf(data, "{\"file_id\":%d,\"expire_days\":%d}", &file_id, &expire_days);
        free(data);
        
        if (file_id <= 0) {
            printf("{\"code\":\"400\",\"message\":\"Invalid file_id\"}");
            continue;
        }
        
        MYSQL *conn = connect_db();
        if (!conn) {
            printf("{\"code\":\"500\",\"message\":\"DB connection failed\"}");
            continue;
        }
        
        // 生成分享 Token
        char share_token[33] = {0};
        generate_token(share_token, 33);
        
        // 计算过期时间
        time_t expire_time = time(NULL) + expire_days * 24 * 3600;
        
        char query[512];
        snprintf(query, sizeof(query),
                 "INSERT INTO file_shares (file_id, share_token, expire_time) "
                 "VALUES (%d, '%s', FROM_UNIXTIME(%ld))",
                 file_id, share_token, expire_time);
        
        if (mysql_query(conn, query) == 0) {
            printf("{\"code\":\"000\",\"share_url\":\"http://192.168.226.128/share?token=%s\"}", share_token);
        } else {
            printf("{\"code\":\"500\",\"message\":\"Share failed\"}");
        }
        
        mysql_close(conn);
    }
    
    mysql_library_end();
    return 0;
}
