# 分布式云盘 Docker 构建问题总结

## 项目信息
- **项目**: 分布式云盘系统（刘派云盘）
- **技术栈**: Nginx + FastDFS + MySQL + Redis + C FastCGI
- **构建环境**: Windows 10 + WSL2 Ubuntu 26.04 + Docker Desktop

---

## 问题 1: apt 镜像下载极慢

**现象**: Docker 构建时 `apt-get update` 下载 17MB 的 universe 包列表需要 6 分钟+

**原因**: Docker 容器内默认使用 `archive.ubuntu.com`，国内访问极慢

**解决**: 在 Dockerfile 中切换为清华 HTTP 镜像源
```dockerfile
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list
```

**注意**: 必须用 HTTP 不能用 HTTPS，Docker 容器内 SSL 证书验证会失败

---

## 问题 2: libfastcommon-dev 包不存在

**现象**: `E: Unable to locate package libfastcommon-dev`

**原因**: `libfastcommon-dev` 不在 Ubuntu 22.04 标准仓库中，需要从源码编译

**解决**: 删除 apt 中的 `libfastcommon-dev`，改为从 GitHub 下载源码编译安装

---

## 问题 3: FastDFS 6.x 需要额外依赖 server-functions

**现象**: `fatal error: sf/sf_global.h: No such file or directory`

**原因**: FastDFS 6.12.2 依赖 `server-functions` 库，该库也需要从 GitHub 下载

**解决**: 改用 **FastDFS 5.10**（不依赖 server-functions）

---

## 问题 4: FastDFS 5.10 与 libfastcommon 1.0.80 不兼容

**现象**: 
- `error: too few arguments to function 'free_queue_init_ex'`
- `'struct fast_task_info' has no member named 'length'`

**原因**: libfastcommon 1.0.80 的 API 与 FastDFS 5.10 不兼容

**解决**: 使用原项目指定的版本组合：
- **libfastcommon 1.0.36** + **FastDFS 5.10**

---

## 问题 5: libfastcommon 头文件未安装到 /usr/include/

**现象**: `fatal error: common_define.h: No such file or directory`

**原因**: libfastcommon 1.0.36 的 `make install` 将头文件安装到 `/usr/include/fastcommon/`，但 FastDFS 代码直接 `#include "common_define.h"` 不带路径前缀

**解决**: 编译后手动复制所有头文件到 `/usr/include/`
```dockerfile
RUN cd /tmp && tar xzf libfastcommon-1.0.36.tar.gz && cd libfastcommon-1.0.36 && \
    ./make.sh && ./make.sh install && \
    cp src/*.h /usr/include/ 2>/dev/null; \
    rm -rf /tmp/*
```

---

## 问题 6: nginx module 版本与 FastDFS 不兼容

**现象**: 
- V1.22: `error: 'FDFSHTTPParams' has no member named 'support_multi_range'`
- V1.21: 同样的兼容性问题

**原因**: 不同版本的 nginx module 对应不同版本的 FastDFS

**解决**: 使用 **V1.20** + sed 修复兼容性问题
```dockerfile
RUN sed -i 's/!g_http_params.support_multi_range/0/' fastdfs-nginx-module-1.20/src/common.c
```

---

## 问题 7: Nginx 编译 -Werror 导致构建失败

**现象**: `error: '%s' directive output may be truncated writing up to 510 bytes [-Werror=format-truncation=]`

**原因**: GCC 的 `-Werror` 选项将警告视为错误

**解决**: 添加编译参数禁用错误
```dockerfile
CFLAGS="-Wno-error" ./configure --add-module=...
```

---

## 问题 8: spawn-fcgi 缺少 configure 脚本

**现象**: `/bin/sh: 1: ./configure: not found`

**原因**: spawn-fcgi 使用 autotools 构建系统，需要先运行 `autogen.sh`

**解决**: 安装 autoconf/automake，并在编译前运行 autogen.sh
```dockerfile
RUN apt-get install -y autoconf automake
RUN cd /tmp && tar xzf spawn-fcgi.tar.gz && cd spawn-fcgi-1.6.3 && \
    ./autogen.sh && ./configure && make && make install
```

---

## 问题 9: start.sh 无法执行

**现象**: `exec /start.sh: no such file or directory`

**原因**: Windows 系统的 CRLF 换行符导致 `#!/bin/bash\r` 被识别为 `/bin/bash\r`

**解决**: 在 Dockerfile 中添加 sed 修复
```dockerfile
COPY docker/start.sh /start.sh
RUN sed -i 's/\r$//' /start.sh && chmod +x /start.sh
```

---

## 问题 10: nginx tail -f 失败导致容器退出

**现象**: 容器启动后立即退出

**原因**: `tail -f /var/log/nginx/access.log` 时日志文件不存在

**解决**: 在 start.sh 中先创建日志文件
```bash
touch /var/log/nginx/access.log /var/log/nginx/error.log
tail -f /var/log/nginx/access.log
```

---

## 问题 11: mod_fastdfs.conf 缺少 http.* 配置参数

**现象**: 
- `include file "http.conf" not exists`
- `param "http.mime_types_filename" not exist or is empty`
- `param "http.default_content_type" not exist or is empty`
- `host "tracker" is invalid`

**原因**: FastDFS nginx module 需要多个 HTTP 配置参数

**解决**: 在 Dockerfile 中追加配置
```dockerfile
RUN touch /etc/fdfs/http.conf && \
    echo 'http.mime_types_filename=mime.types' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.log_with_nginx=true' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.default_content_type=application/octet-stream' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.need_find_content_type=false' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.trunk_size=256KB' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.server_ip=127.0.0.1' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'http.server_port=80' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'load_fdfs_parameters_from_tracker=false' >> /etc/fdfs/mod_fastdfs.conf && \
    echo 'tracker_server=127.0.0.1:22122' >> /etc/fdfs/mod_fastdfs.conf && \
    cp /usr/local/nginx/conf/mime.types /etc/fdfs/mime.types
```

---

## 问题 12: ghfast.top 代理失效

**现象**: `wget` 下载 GitHub 源码返回 0 字节

**原因**: ghfast.top 代理在国内 Docker 容器内无法访问

**解决**: 
1. 在 WSL2 中先下载源码到本地 `docker/sources/` 目录
2. 使用 `COPY docker/sources/xxx.tar.gz /tmp/` 复制到容器中
3. 备用代理: `gh-proxy.com`（部分文件可用）

---

## 问题 13: nginx module V1.19 下载失败

**现象**: `nginx-module-v1.19.tar.gz` 文件 0 字节

**原因**: gh-proxy.com 不提供 V1.19 版本

**解决**: 改用 V1.20 + sed 修复 `support_multi_range` 兼容性问题

---

## 问题 14: 文件上传路径不匹配

**现象**: `{"code":"404","message":"File not found on disk: /home/s/uploads/xxx"}`

**原因**: C 源码中硬编码了 `/home/s/uploads/`，但 Docker 容器中路径是 `/home/cloud/uploads/`

**解决**: 修改 `upload_fcgi.c` 和 `download_fcgi.c` 中的路径
```c
// upload_fcgi.c:79
snprintf(file_path, sizeof(file_path), "/home/cloud/uploads/%ld_%d", ...)

// download_fcgi.c:77
snprintf(alt_path, sizeof(alt_path), "/home/cloud/uploads/%s", stored_path);
```

---

## 最终构建配置

### 版本组合
| 组件 | 版本 | 说明 |
|------|------|------|
| 基础镜像 | Ubuntu 22.04 | docker.m.daocloud.io |
| libfastcommon | 1.0.36 | 源码编译 |
| FastDFS | 5.10 | 源码编译 |
| Nginx | 1.24.0 | 源码编译 |
| nginx module | 1.20 | 源码编译 + sed 修复 |
| spawn-fcgi | 1.6.3 | autotools 编译 |
| MySQL | 8.0 | apt 安装 |
| Redis | 6.0 | apt 安装 |

### 源码下载地址（gh-proxy.com 代理）
- libfastcommon: https://gh-proxy.com/https://github.com/happyfish100/libfastcommon/archive/refs/tags/V1.0.36.tar.gz
- FastDFS: https://gh-proxy.com/https://github.com/happyfish100/fastdfs/archive/refs/tags/V5.10.tar.gz
- nginx module: https://gh-proxy.com/https://github.com/happyfish100/fastdfs-nginx-module/archive/refs/tags/V1.20.tar.gz
- Nginx: https://gh-proxy.com/https://nginx.org/download/nginx-1.24.0.tar.gz
- spawn-fcgi: https://gh-proxy.com/https://github.com/lighttpd/spawn-fcgi/archive/refs/tags/v1.6.3.tar.gz

### 启动命令
```bash
cd E:\distributed-cloud-disk
docker compose -f docker/docker-compose.yml up -d
```

### 访问地址
- **URL**: http://localhost:8080
- **测试账号**: alice / 123456
