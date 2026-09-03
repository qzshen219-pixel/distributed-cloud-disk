# 云盘系统 FastCGI 文件问题汇总与修复顺序

## 一、涉及文件

- `upload_fcgi.c`：文件上传
- `filelist_fcgi.c`：文件列表
- `download_fcgi.c`：文件下载
- `delete_fcgi.c`：文件删除

---

## 二、P0：必须立即修复

### 1. 下载接口缺少身份认证和文件归属校验

**涉及文件：** `download_fcgi.c`

当前接口仅根据文件ID查询：

```sql
SELECT file_id, filename
FROM files
WHERE id = ?;
```

攻击者可以枚举`file_id`下载其他用户的文件，属于越权访问漏洞。

**修复建议：**

1. 获取并验证Token；
2. 得到可信的`user_id`；
3. 查询时同时限制文件ID和用户ID。

```sql
SELECT file_id, filename, file_size, file_md5, storage_type
FROM files
WHERE id = ? AND user_id = ?;
```

---

### 2. 下载接口存在释放后使用

**涉及文件：** `download_fcgi.c`

当前代码：

```c
char *stored_id = row[0];
char *filename = row[1];

mysql_free_result(res);

/* 后面继续使用stored_id和filename */
```

`stored_id`和`filename`指向MySQL结果集内部。释放结果集后，它们成为悬空指针。

**可能后果：**

- 文件名乱码；
- 打开错误路径；
- 程序崩溃；
- 未定义行为。

**修复建议：**

```c
char *stored_id = row[0] ? strdup(row[0]) : NULL;
char *filename = row[1] ? strdup(row[1]) : strdup("download");

mysql_free_result(res);
res = NULL;
```

使用完成后释放：

```c
free(filename);
free(stored_id);
```

---

### 3. 删除接口存在释放后使用

**涉及文件：** `delete_fcgi.c`

当前代码：

```c
char *filename = row[1];
mysql_free_result(res);

printf("%s", filename);
```

释放结果集后继续访问`filename`，属于未定义行为。

**修复建议：**

```c
char *filename =
    row[1] ? strdup(row[1]) : strdup("unknown");
```

使用完成后：

```c
free(filename);
```

---

### 4. 上传接口使用字符串函数处理二进制文件

**涉及文件：** `upload_fcgi.c`

当前代码使用：

```c
strlen(post_data);
strstr(post_data, ...);
```

二进制文件中可能包含`'\0'`，导致：

- 文件被截断；
- 图片、PDF、ZIP损坏；
- 文件大小错误；
- MD5错误；
- multipart边界解析失败。

**修复建议：**

让`read_post_data()`同时返回请求体真实长度：

```c
unsigned char *read_post_data(
    size_t max_size,
    size_t *out_len
);
```

后续使用：

```c
size_t post_len;
size_t file_len;
```

不能再使用：

```c
strlen(post_data);
```

使用带长度的内存查找函数：

```c
find_bytes(data, data_len, pattern, pattern_len);
```

---

### 5. multipart文件内容定位错误

**涉及文件：** `upload_fcgi.c`

当前代码直接查找整个请求体中第一个：

```c
"\r\n\r\n"
```

第一个表单项可能是`user`或`md5`，不一定是文件。

**修复建议：**

1. 从`CONTENT_TYPE`中提取boundary；
2. 遍历multipart表单项；
3. 找到头部包含`filename=`的表单项；
4. 将该项头部结束位置作为文件起点；
5. 使用下一条boundary确定文件终点。

---

### 6. 文件保存失败后仍可能返回成功

**涉及文件：** `upload_fcgi.c`

当前代码没有检查：

```c
fopen();
fwrite();
fclose();
```

即使本地文件没有保存成功，仍可能写入数据库并返回上传成功。

**修复建议：**

```c
int storage_ok = 0;

size_t written = fwrite(file_start, 1, file_len, fp);

if (written == file_len && fclose(fp) == 0) {
    storage_ok = 1;
}
```

只有：

```c
storage_ok == 1
```

才允许写入数据库。

---

### 7. 文件名存在SQL注入风险

**涉及文件：** `upload_fcgi.c`

当前代码转义了`stored_file_id`和`md5`，但没有转义用户提交的`filename`。

**修复建议：**

优先使用MySQL预处理语句：

```sql
INSERT INTO files
    (user_id, filename, file_id, file_size, file_md5, storage_type)
VALUES
    (?, ?, ?, ?, ?, ?);
```

如果暂时无法使用预处理语句，至少通过：

```c
mysql_real_escape_string();
```

转义文件名。

---

## 三、P1：保证数据库与物理文件一致

### 1. 删除接口没有删除物理文件

**涉及文件：** `delete_fcgi.c`

当前接口只执行：

```sql
DELETE FROM files
WHERE id = ? AND user_id = ?;
```

没有删除：

- FastDFS文件；
- 本地磁盘文件。

这会产生孤儿文件。

**修复建议：**

查询时获取：

```sql
SELECT filename, file_id, storage_type
FROM files
WHERE id = ? AND user_id = ?;
```

根据存储类型执行：

```c
fdfs_delete_file(...);
```

或者：

```c
unlink(local_path);
```

---

### 2. 数据库插入失败会留下孤儿文件

**涉及文件：** `upload_fcgi.c`

当前流程：

```text
保存物理文件
→ 插入数据库
```

如果数据库插入失败，物理文件仍然存在。

**修复建议：**

数据库插入失败时执行补偿删除：

```c
if (used_fdfs) {
    fdfs_delete_file(FDFS_CONF, stored_file_id);
} else {
    unlink(stored_file_id);
}
```

---

### 3. 增加明确的存储类型字段

当前程序通过：

```c
strstr(stored_id, "group") == stored_id
```

判断是不是FastDFS，不够可靠。

**修复建议：**

数据库增加：

```sql
storage_type VARCHAR(20) NOT NULL;
```

可选值：

```text
fastdfs
local
minio
s3
```

下载和删除时直接根据`storage_type`选择操作。

---

### 4. 使用软删除解决跨系统一致性

数据库和FastDFS不属于同一个事务，无法保证同时成功。

建议增加文件状态：

```text
active
deleting
deleted
delete_failed
```

删除流程：

```text
active
  ↓
deleting
  ↓
删除物理文件
  ├─ 成功 → deleted
  └─ 失败 → delete_failed，后台重试
```

文件列表和下载接口只查询：

```sql
WHERE status = 'active';
```

---

### 5. 下载接口存在任意文件读取风险

**涉及文件：** `download_fcgi.c`

当前代码可能直接执行：

```c
fopen(stored_id, "rb");
```

如果数据库中的路径异常，可能读取服务器敏感文件。

**修复建议：**

1. 使用`realpath()`规范化路径；
2. 验证最终路径必须位于上传目录；
3. 拒绝包含路径穿越的记录。

允许目录：

```text
/home/cloud/uploads/
```

---

### 6. 客户端MD5不可信

**涉及文件：** `upload_fcgi.c`

客户端可以提交任意MD5，服务器不能直接使用。

**修复建议：**

服务器始终重新计算MD5：

```c
MD5(file_start, file_len, digest);
```

如果客户端提供MD5，只用于比较：

```c
if (strcasecmp(client_md5, computed_md5) != 0) {
    /* MD5不一致，拒绝上传 */
}
```

安全校验建议使用SHA-256。

---

## 四、P2：稳定性和输出安全

### 1. `mysql_store_result()`没有判空

**涉及文件：**

- `filelist_fcgi.c`
- `download_fcgi.c`
- `delete_fcgi.c`

必须增加：

```c
MYSQL_RES *res = mysql_store_result(conn);

if (!res) {
    fprintf(stderr,
            "Store result failed: %s\n",
            mysql_error(conn));

    /* 返回500并清理资源 */
}
```

否则继续执行`mysql_fetch_row(res)`可能崩溃。

---

### 2. JSON字符串没有转义

**涉及文件：**

- `upload_fcgi.c`
- `filelist_fcgi.c`
- `download_fcgi.c`
- `delete_fcgi.c`

文件名可能包含：

```text
"
\
换行符
制表符
```

直接通过`printf()`输出会生成非法JSON。

**修复建议：**

使用JSON库：

- cJSON
- json-c
- Jansson

注意：

> SQL转义、JSON转义和HTTP响应头转义是三种不同的处理，不能互相替代。

---

### 3. 下载文件名存在HTTP响应头注入

**涉及文件：** `download_fcgi.c`

当前代码：

```c
printf(
    "Content-Disposition: attachment; filename=\"%s\"\r\n",
    filename
);
```

如果文件名包含`\r\n`，可能注入额外响应头。

**修复建议：**

过滤：

```c
'\r'
'\n'
'"'
'\\'
```

中文文件名使用：

```http
filename*=UTF-8''...
```

并进行RFC 5987百分号编码。

---

### 4. 临时文件名可能冲突

**涉及文件：**

- `upload_fcgi.c`
- `download_fcgi.c`

当前使用：

```c
time(NULL);
rand();
```

生成临时文件名，可能出现：

- 同一秒冲突；
- 多进程冲突；
- 文件覆盖；
- 符号链接攻击。

**修复建议：**

使用：

```c
mkstemp();
```

所有错误路径都必须清理：

```c
unlink(temp_path);
```

---

### 5. 未检查内存分配结果

涉及：

```c
malloc();
strdup();
```

必须检查：

```c
if (!ptr) {
    /* 返回500并清理资源 */
}
```

---

### 6. 类型和格式符不匹配

建议修改：

```c
int file_len;
```

为：

```c
size_t file_len;
```

修改：

```c
int num_rows;
```

为：

```c
my_ulonglong num_rows;
```

修改：

```c
long actual_size;
```

为：

```c
off_t actual_size;
```

常用格式符：

| 类型                 | 格式符 |
| -------------------- | ------ |
| `int`                | `%d`   |
| `size_t`             | `%zu`  |
| `long`               | `%ld`  |
| `long long`          | `%lld` |
| `unsigned long long` | `%llu` |

---

### 7. 返回真正的HTTP状态码

不要只返回：

```json
{"code":"401"}
```

应该同时输出：

```c
printf("Status: 401 Unauthorized\r\n");
printf("Content-Type: application/json; charset=utf-8\r\n\r\n");
```

常用状态码：

| 场景               |                  HTTP状态码 |
| ------------------ | --------------------------: |
| 参数错误           |           `400 Bad Request` |
| Token缺失或失效    |          `401 Unauthorized` |
| 没有权限           |             `403 Forbidden` |
| 文件不存在         |             `404 Not Found` |
| 数据库或服务器错误 | `500 Internal Server Error` |
| 存储服务暂时不可用 |   `503 Service Unavailable` |

---

## 五、P3：性能和工程优化

### 1. 文件列表增加分页

**涉及文件：** `filelist_fcgi.c`

当前一次查询用户所有文件。

建议增加：

```text
page
page_size
```

查询：

```sql
SELECT ...
FROM files
WHERE user_id = ?
ORDER BY upload_time DESC
LIMIT ? OFFSET ?;
```

总数单独查询：

```sql
SELECT COUNT(*)
FROM files
WHERE user_id = ?;
```

---

### 2. 下载支持Range断点续传

**涉及文件：** `download_fcgi.c`

支持请求头：

```http
Range: bytes=1000000-
```

返回：

```http
206 Partial Content
Accept-Ranges: bytes
Content-Range: bytes 1000000-1999999/文件总大小
```

这对视频、大型ZIP和断点续传很重要。

---

### 3. 完整检查文件读写结果

上传端需要检查：

```c
fwrite();
fclose();
```

下载端需要检查：

```c
fread();
fwrite();
ferror();
fseek();
ftell();
```

下载时记录：

```c
total_sent
```

并验证：

```c
total_sent == actual_size
```

---

### 4. 调整下载次数更新时机

不要在文件尚未打开时增加下载次数。

建议至少在文件成功打开后更新。

更准确的方式是在：

```c
total_sent == actual_size
```

时增加下载次数。

---

### 5. 保存FastDFS初始化状态

```c
int fdfs_initialized = (fdfs_init() == 0);
```

调用FastDFS上传、下载、删除和清理函数前，都应检查该状态。

---

### 6. 增加服务器错误日志

对外返回：

```json
{"code":"DB_ERROR","message":"Internal server error"}
```

服务器内部记录：

```c
fprintf(stderr,
        "[filelist_fcgi] MySQL error: %s\n",
        mysql_error(conn));
```

不要向客户端暴露：

- MySQL错误详情；
- FastDFS内部ID；
- 服务器绝对路径；
- 数据库结构。

---

### 7. 使用统一资源清理出口

请求开始时：

```c
MYSQL *conn = NULL;
MYSQL_RES *res = NULL;
char *token = NULL;
char *filename = NULL;
FILE *fp = NULL;
```

错误时：

```c
goto request_cleanup;
```

统一清理：

```c
request_cleanup:
    if (fp) {
        fclose(fp);
    }

    if (res) {
        mysql_free_result(res);
    }

    if (conn) {
        mysql_close(conn);
    }

    free(filename);
    free(token);
```

---

# 六、建议修复顺序

## 第一阶段：安全与崩溃问题

1. 为`download_fcgi.c`增加Token和文件归属验证；
2. 修复`download_fcgi.c`的释放后使用；
3. 修复`delete_fcgi.c`的释放后使用；
4. 给所有`mysql_store_result()`增加判空；
5. 修复上传multipart二进制解析；
6. 修复上传文件名SQL注入；
7. 限制本地下载路径范围。

## 第二阶段：存储一致性

8. 增加`storage_type`字段；
9. 文件存储失败时禁止写数据库；
10. 数据库插入失败时补偿删除物理文件；
11. 删除接口同时删除物理文件；
12. 增加`active/deleting/deleted`状态和重试机制；
13. 服务端重新计算文件MD5。

## 第三阶段：接口规范

14. 引入JSON库；
15. 返回真正的HTTP状态码；
16. 修复`Content-Disposition`响应头注入；
17. 不向客户端暴露内部文件路径；
18. 统一错误响应格式和服务器日志。

## 第四阶段：性能优化

19. 使用`mkstemp()`管理临时文件；
20. 完整检查文件读写结果；
21. 文件列表增加分页；
22. 下载增加Range断点续传；
23. 优化下载次数统计；
24. 使用统一资源清理出口；
25. 根据并发规模考虑数据库连接管理。

---

# 七、最先修复的五项

如果当前时间有限，应优先完成：

1. 下载接口增加鉴权与文件归属校验；
2. 修复下载和删除接口的释放后使用；
3. 修复上传接口的二进制multipart解析；
4. 修复上传文件名SQL注入；
5. 删除接口同步处理物理文件。

这五项直接关系到：

- 用户文件越权；
- 程序崩溃；
- 文件损坏；
- 数据库安全；
- 存储空间泄漏。