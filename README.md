# 分布式云盘系统 — 项目技术文档

---

## 一、项目概述

本项目是一个**分布式云存储系统**（刘派云盘），实现了用户注册登录、文件上传下载、文件管理、文件分享等核心功能。系统采用前后端分离架构，前端为 Web 页面，后端为 C 语言编写的 FastCGI 程序，数据存储使用 MySQL + Redis + FastDFS，Web 服务使用 Nginx 反向代理。

### 核心功能

| 功能 | 说明 |
|------|------|
| 用户注册 | 用户名+密码注册，MD5 加密存储，重复检测 |
| 用户登录 | 密码验证，返回用户信息和 token |
| 文件上传 | 支持拖拽/点击上传，显示进度条，自动计算 MD5 |
| 文件列表 | 按上传时间倒序显示，显示文件名、大小、时间、下载次数 |
| 文件下载 | 自动识别文件类型，浏览器触发下载 |
| 文件删除 | 单个/批量删除 |
| 文件分享 | 生成带 token 的分享链接，支持设置有效期 |

---

## 二、技术栈

### 整体架构

```
浏览器 (HTML/CSS/JS)
    ↓ HTTP
Nginx (反向代理 + 静态资源 + FastDFS 模块)
    ↓ FastCGI 协议
FastCGI 程序 (C 语言 × 7个)
    ↓
MySQL 8.0 (用户数据 + 文件元信息)
Redis 4.0 (缓存)
FastDFS 5.10 (分布式文件存储)
```

### 各组件详情

| 组件 | 版本 | 作用 | 端口 |
|------|------|------|------|
| **Nginx** | 1.10.1 | Web 服务器、反向代理、静态资源、FastDFS 模块 | 80 |
| **FastCGI 程序** | 自研 C | 处理业务逻辑（login/register/upload/download/filelist/delete/share） | 9000-9006, 9999 |
| **MySQL** | 8.0 | 存储用户信息和文件元数据 | 3306 |
| **Redis** | 4.0.8 | 缓存（预留） | 6379 |
| **FastDFS** | 5.10 | 分布式文件存储 | 22122(tracker) / 23000(storage) |
| **spawn-fcgi** | — | FastCGI 进程管理器 | — |
| **hiredis** | — | Redis C 客户端库 | — |
| **OpenSSL** | — | MD5 哈希计算 | — |

### 前端技术

| 技术 | 说明 |
|------|------|
| HTML5 | 页面结构 |
| CSS3 | 样式（渐变、圆角、动画） |
| JavaScript (ES6+) | 交互逻辑、API 调用 |
| Fetch API | RESTful 接口请求 |
| XMLHttpRequest | 文件上传（支持进度回调） |
| 拖拽 API | 文件拖拽上传 |
| localStorage | 本地存储登录状态 |

### 后端技术

| 技术 | 说明 |
|------|------|
| C 语言 | FastCGI 程序开发 |
| FastCGI 协议 | 与 Nginx 通信 |
| MySQL C API | 数据库操作 |
| OpenSSL | MD5 密码哈希 |
| multipart 解析 | 文件上传数据解析 |
| JSON 手动解析 | 请求/响应数据处理 |

---

## 三、系统架构详解

> 完整架构文档、Nginx 配置、数据库设计、安全设计 → [docs/架构设计.md](docs/架构设计.md)

---

## 四、API 接口文档

### 基础信息

- **Base URL**: `http://<服务器IP>:80`
- **Content-Type**: `application/json`
- **响应格式**: JSON

### 接口列表

#### POST /register — 用户注册

请求：
```json
{
    "username": "zhangsan",
    "password": "123456",
    "nickname": "张三",
    "email": "test@test.com",
    "phone": "13800138000"
}
```

响应（成功）：
```json
{
    "code": "002",
    "message": "Register success",
    "data": {"id": 8, "username": "zhangsan", "nickname": "张三"}
}
```

响应（用户名已存在）：
```json
{"code": "409", "message": "Username already exists"}
```

#### POST /login — 用户登录

请求：
```json
{"username": "zhangsan", "password": "123456"}
```

响应（成功）：
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

响应（密码错误）：
```json
{"code": "401", "message": "Invalid username or password"}
```

#### GET /filelist?user_id={id} — 文件列表

响应：
```json
{
    "code": "000",
    "files": [
        {
            "id": 10,
            "name": "hello.txt",
            "size": 231,
            "time": "2026-06-28 09:51:24",
            "downloads": 0,
            "md5": "d41d8cd98f00b204"
        }
    ],
    "total": 1
}
```

#### POST /upload — 文件上传

请求格式：`multipart/form-data`

| 字段 | 类型 | 说明 |
|------|------|------|
| user | string | 用户名 |
| file | file | 文件内容 |
| (扩展) | — | filename, md5, size 在 Content-Disposition 中 |

响应：
```json
{
    "code": "000",
    "message": "Upload success",
    "data": {
        "id": 13,
        "filename": "test.txt",
        "path": "/home/s/uploads/1782611800_1714636915",
        "size": 196,
        "md5": "d41d8cd98f00b204"
    }
}
```

#### GET /download?file_id={id} — 文件下载

返回：二进制文件流 + HTTP 头
```
Content-Type: text/plain; charset=utf-8
Content-Disposition: attachment; filename="test.txt"
Content-Length: 196
```

#### POST /delete — 文件删除

请求：
```json
{"file_id": 10}
```

响应：
```json
{"code": "000", "message": "File deleted", "data": {"id": 10, "filename": "test.txt"}}
```

#### POST /share — 创建分享

请求：
```json
{"file_id": 5, "expire_days": 7}
```

响应：
```json
{
    "code": "000",
    "data": {
        "share_url": "http://192.168.226.128/share?token=QCjXgNbbwo7D6AzRRbjFXaBfFEk3NUOr",
        "filename": "test.txt",
        "expire": "2026-07-01 09:49:58",
        "expire_days": 7
    }
}
```

### 错误码

| code | 含义 |
|------|------|
| 000 | 成功 |
| 001 | 失败 |
| 002 | 注册成功 |
| 400 | 请求参数错误 |
| 401 | 认证失败（用户名/密码错误） |
| 404 | 资源不存在 |
| 409 | 资源冲突（用户名已存在） |
| 500 | 服务器内部错误 |

---

## 五、从零搭建指南

### 5.1 环境准备

需要：
- VMware Workstation + Ubuntu 22.04 虚拟机
- Windows 宿主机（运行 Qt 客户端）
- 虚拟机网络模式：NAT（IP 段 192.168.226.0/24）

### 5.2 安装依赖

```bash
# Ubuntu VM 中执行
sudo apt update
sudo apt install -y build-essential libmysqlclient-dev libssl-dev \
    libevent-dev libpcre3-dev zlib1g-dev libfastcommon-dev

# 安装 MySQL
sudo apt install -y mysql-server
sudo mysql -e "CREATE DATABASE file_cloud;"
sudo mysql -e "CREATE USER 'cloud_user'@'localhost' IDENTIFIED BY 'Cloud@2026#Secure';"
sudo mysql -e "GRANT ALL ON file_cloud.* TO 'cloud_user'@'localhost';"

# 创建数据表（见上方数据库设计部分）
```

### 5.3 安装 FastDFS

```bash
# 编译安装 libfastcommon
unzip libfastcommon-1.36.zip && cd libfastcommon-master
./make.sh && sudo ./make.sh install

# 编译安装 FastDFS
tar xzf fastdfs-5.10.tar.gz && cd fastdfs-5.10
./make.sh && sudo ./make.sh install

# 配置 Tracker
sudo cp /etc/fdfs/tracker.conf.sample /etc/fdfs/tracker.conf
sudo vim /etc/fdfs/tracker.conf  # 设置 base_path
sudo fdfs_trackerd /etc/fdfs/tracker.conf start

# 配置 Storage
sudo cp /etc/fdfs/storage.conf.sample /etc/fdfs/storage.conf
sudo vim /etc/fdfs/storage.conf  # 设置 base_path, tracker_server
sudo fdfs_storaged /etc/fdfs/storage.conf start
```

### 5.4 安装 Nginx + FastDFS 模块

```bash
# 解压 fastdfs-nginx-module
tar xzf fastdfs-nginx-module-1.15.tar.gz

# 编译 Nginx（带 FastDFS 模块）
cd nginx-1.10.1
./configure --add-module=../fastdfs-nginx-module-1.15/src \
    --prefix=/usr/local/nginx
make && sudo make install

# 配置 nginx.conf（见上方配置详解）
sudo cp mod_fastdfs.conf /etc/fdfs/
sudo /usr/local/nginx/sbin/nginx
```

### 5.5 安装 spawn-fcgi

```bash
# 编译安装
tar xzf spawn-fcgi-1.6.3.tar.gz && cd spawn-fcgi-1.6.3
./configure && make && sudo make install
```

### 5.6 编写 FastCGI 程序

```bash
# 将源码文件复制到 VM
# fcgi_common.h, login_fcgi_new.c, register_fcgi_new.c,
# upload_fcgi_new.c, download_fcgi_new.c, filelist_fcgi_new.c,
# delete_fcgi_new.c, share_fcgi_new.c

# 编译
cd /home/s
FLAGS="-lfcgi -lmysqlclient -lssl -lcrypto"

gcc -o login_fcgi login_fcgi_new.c $FLAGS
gcc -o register_fcgi register_fcgi_new.c $FLAGS
gcc -o uploadFile_fcgi upload_fcgi_new.c $FLAGS
gcc -o download_fcgi download_fcgi_new.c $FLAGS
gcc -o filelist_fcgi filelist_fcgi_new.c $FLAGS
gcc -o delete_fcgi delete_fcgi_new.c $FLAGS
gcc -o share_fcgi share_fcgi_new.c $FLAGS

# 启动
spawn-fcgi -a 127.0.0.1 -p 9000 -f /home/s/register_fcgi
spawn-fcgi -a 127.0.0.1 -p 9001 -f /home/s/login_fcgi
spawn-fcgi -a 127.0.0.1 -p 9999 -f /home/s/uploadFile_fcgi
spawn-fcgi -a 127.0.0.1 -p 9003 -f /home/s/download_fcgi
spawn-fcgi -a 127.0.0.1 -p 9004 -f /home/s/filelist_fcgi
spawn-fcgi -a 127.0.0.1 -p 9005 -f /home/s/delete_fcgi
spawn-fcgi -a 127.0.0.1 -p 9006 -f /home/s/share_fcgi
```

### 5.7 部署前端

```bash
# 将 index.html 复制到 nginx 目录
sudo cp index.html /usr/local/nginx/yundisk/index.html
sudo cp index.html /usr/local/nginx/zyFile2/index.html
```

### 5.8 验证

```bash
# 检查所有服务
ps aux | grep -E 'nginx|redis|fdfs|mysql|fcgi'

# 测试接口
curl -X POST http://localhost/login -d '{"username":"test","password":"123456"}'
curl http://localhost/filelist?user_id=1
```

---

## 六、项目文件结构

```
分布式服务器编程/
├── 01 fastDFS/                  # FastDFS 学习资料
├── 02 redis/                    # Redis 学习资料
├── 03 nginx/                    # Nginx 学习资料
│   └── nginx-1.12.0/conf/nginx.conf
├── 04 fastCGI/                  # FastCGI 学习资料
├── 05 nginx+fastDFS/            # Nginx+FastDFS 集成
├── 06 login/                    # 登录注册 Qt 客户端
│   └── 03-代码/LoginDemo/
├── 07 css-upload/               # CSS 样式 + 上传 Qt 客户端
│   └── 04-代码/LoginDemo/
├── 08 upload-filelist-download/ # 完整功能 Qt 客户端
│   └── 03-代码/UploadFile/
├── 09 总结/                     # 项目总结
├── web/                         # 前端页面
│   └── index.html               # 云盘 Web 前端（单文件）
└── API文档.md                   # 接口文档
```

### VM 内文件结构

```
/home/s/                         # 用户主目录
├── fcgi_common.h                # FastCGI 公共头文件
├── login_fcgi_new.c             # 登录程序源码
├── register_fcgi_new.c          # 注册程序源码
├── upload_fcgi_new.c            # 上传程序源码
├── download_fcgi_new.c          # 下载程序源码
├── filelist_fcgi_new.c          # 文件列表源码
├── delete_fcgi_new.c            # 删除程序源码
├── share_fcgi_new.c             # 分享程序源码
├── login_fcgi                   # 编译后的登录程序
├── register_fcgi                # 编译后的注册程序
├── uploadFile_fcgi              # 编译后的上传程序
├── download_fcgi                # 编译后的下载程序
├── filelist_fcgi                # 编译后的文件列表程序
├── delete_fcgi                  # 编译后的删除程序
├── share_fcgi                   # 编译后的分享程序
└── uploads/                     # 上传文件存储目录

/usr/local/nginx/
├── sbin/nginx                   # Nginx 可执行文件
├── conf/nginx.conf              # Nginx 配置文件
├── yundisk/index.html           # 前端页面
└── zyFile2/                     # Nginx root 目录

/etc/fdfs/
├── tracker.conf                 # FastDFS Tracker 配置
├── storage.conf                 # FastDFS Storage 配置
├── client.conf                  # FastDFS 客户端配置
└── mod_fastdfs.conf             # Nginx FastDFS 模块配置
```

---

## 七、编译参数

```bash
# 所有 FastCGI 程序的编译参数
gcc -o <程序名> <源码.c> -lfcgi -lmysqlclient -lssl -lcrypto

# 依赖库说明
# -lfcgi          FastCGI 库
# -lmysqlclient   MySQL 客户端库
# -lssl           OpenSSL SSL 库（用于 MD5）
# -lcrypto        OpenSSL 加密库
```

---

## 八、服务管理命令

```bash
# 启动所有服务
sudo /usr/local/nginx/sbin/nginx              # Nginx
sudo /usr/bin/fdfs_trackerd /etc/fdfs/tracker.conf start  # FastDFS Tracker
sudo /usr/bin/fdfs_storaged /etc/fdfs/storage.conf start  # FastDFS Storage
redis-server --daemonize yes                   # Redis
# MySQL 通常开机自启

# 启动 FastCGI 进程
spawn-fcgi -a 127.0.0.1 -p 9000 -f /home/s/register_fcgi
spawn-fcgi -a 127.0.0.1 -p 9001 -f /home/s/login_fcgi
spawn-fcgi -a 127.0.0.1 -p 9999 -f /home/s/uploadFile_fcgi
spawn-fcgi -a 127.0.0.1 -p 9003 -f /home/s/download_fcgi
spawn-fcgi -a 127.0.0.1 -p 9004 -f /home/s/filelist_fcgi
spawn-fcgi -a 127.0.0.1 -p 9005 -f /home/s/delete_fcgi
spawn-fcgi -a 127.0.0.1 -p 9006 -f /home/s/share_fcgi

# 停止服务
sudo /usr/local/nginx/sbin/nginx -s stop
killall login_fcgi register_fcgi uploadFile_fcgi download_fcgi \
    filelist_fcgi delete_fcgi share_fcgi

# 重新加载 Nginx
sudo /usr/local/nginx/sbin/nginx -t && sudo /usr/local/nginx/sbin/nginx -s reload

# 检查服务状态
ps aux | grep -E 'nginx|redis|fdfs|mysql|fcgi'
netstat -tlnp | grep -E ':80|:6379|:3306|:22122|:23000|:900[0-6]|:9999'
```

---

## 九、已知问题与改进方向

### 当前已知问题

| 问题 | 说明 |
|------|------|
| 旧文件无法下载 | 早期通过 FastDFS 上传的文件（file_id 为 FastDFS 格式）本地无副本 |
| Redis 未实际使用 | 预留了 Redis 接口但未集成到业务逻辑 |
| 无 HTTPS | 生产环境需要配置 SSL |
| 密码明文传输 | 客户端到服务端为 HTTP，建议改为 HTTPS |
| 无 JWT | token 机制较简单，建议改用 JWT |

### 改进方向

1. **安全增强**：HTTPS、JWT 认证、密码加盐
2. **功能扩展**：文件夹管理、文件预览、批量操作、断点续传
3. **性能优化**：Redis 缓存热点数据、FastDFS 分布式存储、CDN 加速
4. **运维完善**：systemd 服务管理、日志系统、监控告警
5. **代码质量**：使用 cJSON 库替代手动 JSON 解析、单元测试

---

## 十、技术要点总结

### 10.1 为什么用 FastCGI 而不是 CGI？

- **CGI**：每个请求 fork 一个新进程，开销大
- **FastCGI**：常驻进程，通过 socket 通信，性能高 10 倍以上
- **spawn-fcgi**：管理 FastCGI 进程的生命周期

### 10.2 为什么用 MD5 存储密码？

- 不存储明文密码，即使数据库泄露也无法直接获取密码
- MD5 是单向哈希，无法反向推导
- 生产环境应加盐（salt）+ bcrypt/scrypt

### 10.3 文件上传的 multipart 解析

```
------WebKitFormBoundary
Content-Disposition: form-data; name="user"

alice
------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="test.txt"; md5="abc123"; size=100
Content-Type: application/octet-stream

<文件二进制内容>
------WebKitFormBoundary--
```

服务端需要：
1. 解析 boundary 分隔符
2. 提取各字段名和值
3. 区分文件字段和普通字段
4. 提取文件内容（两个 `\r\n\r\n` 之后到下一个 boundary 之前）

### 10.4 Nginx 反向代理 vs 直连

- **反向代理**：客户端只访问 Nginx，Nginx 转发到后端 FastCGI
- **优点**：负载均衡、静态资源加速、SSL 终止、安全隔离
- **配置**：`fastcgi_pass 127.0.0.1:端口` 将请求转发到 FastCGI 进程

---

## 十一、更新日志

### v1.1.0 (2026-06-28)

#### 新增功能

**Token 认证机制**
- 登录接口生成 token 并存入数据库（格式：`user_id_random`）
- filelist/upload/delete/share 接口验证 `X-Auth-Token` 请求头
- 无 token 请求返回 HTTP 401
- 前端自动存储 token（localStorage）并随请求发送
- download 接口通过 URL 参数 `?token=xxx` 传递

**Docker 支持**
- `docker/Dockerfile`：单容器部署（MySQL + Redis + Nginx + FastDFS + FastCGI）
- `docker/docker-compose.yml`：一键启动
- `docker/start.sh`：自动初始化数据库、创建表、启动所有服务
- `docker/server/`：FastCGI 源码（编译进容器）

#### Bug 修复

**登录验证**
- 修复：登录不验证密码，任何密码都返回"Login success"
- 修复：注册不检查用户名重复，可重复注册
- 现在：密码错误返回 `code:401`，用户名已存在返回 `code:409`

**文件下载**
- 修复：download 返回 JSON 元信息而非文件内容
- 修复：download 对不存在的文件返回 HTTP 200
- 现在：download 直接返回文件流 + 正确的 HTTP 状态码（404/400/200）

**Nginx 配置**
- 修复：nginx.conf 有两个重复的 HTTPS server 块
- 修复：server 块嵌套错误导致语法报错
- 修复：`/hello/` 和 `/upload-page/` 路径指向旧的 login.html

**FastCGI 程序**
- 重写全部 7 个 FastCGI 程序，使用公共头文件 `fcgi_common.h`
- upload：修复 10MB 栈分配导致的栈溢出，改用 malloc
- upload：修复 multipart 解析，正确提取 user 字段
- 所有接口添加 SQL 注入防护（mysql_real_escape_string）
- delete：只能删除自己的文件（验证 user_id）
- share：只能分享自己的文件（验证 user_id）

#### 代码重构

**公共头文件 fcgi_common.h**
- 新增 `verify_token()` — token 验证函数
- 新增 `create_token()` — 生成并保存 token
- 新增 `get_auth_token()` — 从请求头获取 token
- 新增 `multipart_get_field()` — multipart 表单字段解析
- 新增 `multipart_get_filename()` — 提取文件名
- 新增 `multipart_get_attr()` — 提取扩展属性

**前端重构**
- 新增登录页/注册页切换
- 新增拖拽上传支持
- 新增上传进度条
- 新增文件选择/批量删除
- 新增分享链接弹窗+复制功能
- 新增退出登录
- localStorage 自动登录

#### 文档

- 新增 `README.md`（项目总览）
- 新增 `docs/架构设计.md`（架构详解、Nginx 配置、数据库设计）
- 新增 `API文档.md`（接口文档）
- 新增 `docker/`（Docker 部署文档）

### v1.1.0 (2026-07-01)

新增 Docker 容器化部署：
- 新增 `docker/Dockerfile`（多阶段构建）
- 新增 `docker/docker-compose.yml`（一键部署）
- 新增 `docker/start.sh`（容器启动脚本）
- 新增 `docker/BUILD_TROUBLESHOOTING.md`（构建问题总结，14 个问题及解决方案）
- 修复文件上传路径问题（`/home/s/` → `/home/cloud/`）
- 修复 `mod_fastdfs.conf` 配置参数缺失
- 适配 FastDFS 5.10 + libfastcommon 1.0.36 + nginx module V1.20

### v1.0.0 (2026-06-28)

初始版本，包含：
- 7 个 FastCGI 服务端程序
- Qt/C++ 桌面客户端
- Web 前端页面
- 完整的登录/注册/上传/下载/删除/分享功能

---

## Docker 部署指南

### 环境要求
- Docker Desktop (Windows) 或 Docker Engine (Linux)
- 至少 2GB 可用内存

### 一键启动

```bash
cd distributed-cloud-disk
docker compose -f docker/docker-compose.yml up -d
```

访问地址：`http://localhost:8080`
测试账号：`alice / 123456`

### 常用命令

```bash
# 查看状态
docker ps

# 查看日志
docker logs -f cloud-disk

# 停止服务
docker compose -f docker/docker-compose.yml down

# 重新构建
docker compose -f docker/docker-compose.yml up -d --build

# 进入容器
docker exec -it cloud-disk bash
```

### 数据持久化
上传的文件通过 Docker Volume `cloud-data` 持久化存储，容器重建后数据不丢失。

### 已知问题
- Windows 10 Home + Docker Desktop 的 localhost 端口转发可能不稳定
- 解决方案：使用 WSL2 IP 地址访问（`wsl -d Ubuntu -- hostname -I`）

---

*文档更新时间：2026年7月1日*
*项目环境：Ubuntu 22.04 LTS (Docker) + Windows 10*
