# 云盘系统接口文档

> Base URL: `http://<服务器IP>:80`  
> Content-Type: `application/json`  
> 所有响应均为 JSON 格式

---

## 1. 用户注册

### POST /register

用户注册新账号。

**请求体：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名，3-16位，支持字母、数字、下划线 |
| password | string | 是 | 密码，6-20位 |
| nickname | string | 否 | 昵称，为空则不设置 |
| email | string | 否 | 邮箱，为空则不设置 |
| phone | string | 否 | 手机号，为空则不设置 |

**请求示例：**

```json
{
    "username": "zhangsan",
    "password": "123456",
    "nickname": "张三",
    "email": "zhangsan@example.com",
    "phone": "13800138000"
}
```

**响应：**

成功（code: 002）：

```json
{
    "code": "002",
    "message": "Register success",
    "data": {
        "id": 8,
        "username": "zhangsan",
        "nickname": "张三"
    }
}
```

用户名已存在（code: 409）：

```json
{
    "code": "409",
    "message": "Username already exists"
}
```

参数错误（code: 400）：

```json
{
    "code": "400",
    "message": "Missing username or password"
}
```

数据库异常（code: 500）：

```json
{
    "code": "500",
    "message": "Register failed"
}
```

**状态码说明：**

| code | 含义 |
|------|------|
| 002 | 注册成功 |
| 400 | 参数缺失或无效 |
| 409 | 用户名已存在 |
| 500 | 服务端内部错误 |

---

## 2. 用户登录

### POST /login

用户登录验证。

**请求体：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 用户名 |
| password | string | 是 | 密码（明文传输，服务端做 MD5 比对） |

**请求示例：**

```json
{
    "username": "zhangsan",
    "password": "123456"
}
```

**响应：**

成功（code: 000）：

```json
{
    "code": "000",
    "message": "Login success",
    "data": {
        "id": 8,
        "username": "zhangsan",
        "nickname": "张三",
        "token": "8_1782551685"
    }
}
```

用户名或密码错误（code: 401）：

```json
{
    "code": "401",
    "message": "Invalid username or password"
}
```

参数错误（code: 400）：

```json
{
    "code": "400",
    "message": "Invalid request"
}
```

**状态码说明：**

| code | 含义 |
|------|------|
| 000 | 登录成功 |
| 400 | 请求参数缺失或无效 |
| 401 | 用户名不存在或密码错误 |
| 500 | 服务端内部错误 |

**注意：**

- 登录成功后服务端自动更新 `last_login` 时间
- `token` 字段可用于后续文件操作的身份验证
- 密码错误和用户不存在统一返回 401，不区分具体原因（防止用户名枚举）

---

## 3. 文件列表

### GET /filelist?user_id={id}

获取指定用户的文件列表。

**请求参数：**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| user_id | int | 是 | 用户ID |

**请求示例：**

```
GET http://192.168.226.128/filelist?user_id=2
```

**响应：**

成功（code: 000）：

```json
{
    "code": "000",
    "files": [
        {
            "id": 8,
            "name": "test.txt",
            "size": 3269666,
            "time": "2026-04-10 14:44:20",
            "downloads": 1
        },
        {
            "id": 5,
            "name": "test.txt",
            "size": 19748,
            "time": "2026-04-09 19:32:55",
            "downloads": 0
        }
    ],
    "total": 2
}
```

---

## 4. 文件上传

### POST /upload

上传文件到服务器。

**请求格式：** `multipart/form-data`

**请求字段：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file | file | 是 | 文件内容（multipart） |
| user | string | 是 | 上传用户名 |

**Content-Disposition 扩展字段：**

| 字段 | 说明 |
|------|------|
| filename | 文件名 |
| md5 | 文件MD5值 |
| size | 文件大小（字节） |

**请求示例：**

```
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary

------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="photo.jpg"; md5="abc123"; size=102400

<二进制文件内容>
------WebKitFormBoundary
Content-Disposition: form-data; name="user"

zhangsan
------WebKitFormBoundary--
```

**响应：**

成功（code: 000）：

```json
{
    "code": "000",
    "message": "Upload success",
    "path": "/home/s/uploads/1782551685_18136243",
    "size": 73
}
```

失败（code: 001）：

```json
{
    "code": "001",
    "message": "Extract content failed"
}
```

---

## 5. 文件下载

### GET /download?file_id={id}

下载指定文件。

**请求参数：**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_id | int | 是 | 文件ID |

**响应：**

返回重定向 URL 和文件名：

```
Location: http://192.168.226.128/file_1775699480
X-Filename: test.txt
```

---

## 6. 文件删除

### POST /delete

删除指定文件。

**请求体：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_id | int | 是 | 文件ID |

**请求示例：**

```json
{
    "file_id": 5
}
```

**响应：**

成功（code: 000）：

```json
{
    "code": "000",
    "message": "File deleted"
}
```

---

## 7. 文件分享

### POST /share

创建文件分享链接。

**请求体：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| file_id | int | 是 | 文件ID |
| expire_days | int | 否 | 有效期（天），默认7天 |

**请求示例：**

```json
{
    "file_id": 5,
    "expire_days": 7
}
```

**响应：**

成功（code: 000）：

```json
{
    "code": "000",
    "share_url": "http://192.168.226.128/share?token=0kBF54TQvWy38b4cgMaWTVk2tkuLhXd6"
}
```

---

## 错误码汇总

| code | 含义 | 使用场景 |
|------|------|----------|
| 000 | 成功 | 通用成功 |
| 001 | 失败 | 通用失败 |
| 002 | 注册成功 | 注册接口专用 |
| 400 | 请求无效 | 参数缺失、格式错误 |
| 401 | 认证失败 | 用户名或密码错误 |
| 409 | 资源冲突 | 用户名已存在 |
| 500 | 服务器错误 | 数据库连接失败等内部异常 |

---

## 数据库表结构

### users 表

| 字段 | 类型 | 说明 |
|------|------|------|
| id | int (PK, AI) | 用户ID |
| username | varchar(50), UNIQUE | 用户名 |
| password | varchar(32) | MD5哈希密码 |
| nickname | varchar(50) | 昵称 |
| email | varchar(100) | 邮箱 |
| phone | varchar(20) | 手机号 |
| token | varchar(64) | 登录token |
| created_at | datetime | 注册时间 |
| last_login | datetime | 最后登录时间 |

### files 表

| 字段 | 类型 | 说明 |
|------|------|------|
| id | int (PK, AI) | 文件记录ID |
| user_id | int (FK) | 所属用户ID |
| filename | varchar(255) | 文件名 |
| file_id | varchar(128) | FastDFS文件ID |
| file_size | bigint | 文件大小（字节） |
| file_md5 | varchar(32) | 文件MD5 |
| upload_time | datetime | 上传时间 |
| download_count | int | 下载次数 |
| folder_id | int | 所属文件夹ID |
| share_token | varchar(64) | 分享token |
| share_expire | datetime | 分享过期时间 |
| is_public | tinyint | 是否公开 |

---

## 技术栈

| 组件 | 版本/说明 |
|------|-----------|
| Web Server | Nginx 1.10.1 |
| FastCGI | spawn-fcgi + 自定义C程序 |
| Database | MySQL 8.0 |
| Cache | Redis 4.0.8 |
| 分布式存储 | FastDFS 5.10 |
| 客户端 | Qt 6.11 (C++) |
