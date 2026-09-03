FROM m.daocloud.io/docker.io/library/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 2. 替换 Ubuntu 软件源为清华大学镜像源（加速 apt 下载）
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list

# 3. 安装 C/C++ 编译工具链及 MySQL、Redis、FastCGI 开发依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    libmysqlclient-dev \
    libhiredis-dev \
    libssl-dev \
    libevent-dev \
    libpcre3-dev \
    zlib1g-dev \
    libfcgi-dev \
    libfcgi0ldbl \
    mysql-server \
    mysql-client \
    redis-server \
    wget \
    curl \
    autoconf \
    automake \
    && rm -rf /var/lib/apt/lists/*

# 4. 编译安装 FastDFS 基础公共库 libfastcommon
COPY docker/sources/libfastcommon-1.0.36.tar.gz /tmp/
RUN cd /tmp && tar xzf libfastcommon-1.0.36.tar.gz && cd libfastcommon-1.0.36 && \
    ./make.sh && ./make.sh install && \
    cp src/*.h /usr/include/ 2>/dev/null&& \
    rm -rf /tmp/*

# 5. 编译安装 FastDFS 核心服务 (Tracker & Storage)
COPY docker/sources/fastdfs-v5.10.tar.gz /tmp/fastdfs.tar.gz
RUN cd /tmp && tar xzf fastdfs.tar.gz && cd fastdfs-5.10 && \
    ./make.sh && ./make.sh install && rm -rf /tmp/*

# 6. 编译安装带 fastdfs-nginx-module 模块的 Nginx
COPY docker/sources/nginx.tar.gz /tmp/
COPY docker/sources/nginx-module-v1.20.tar.gz /tmp/
RUN cd /tmp && tar xzf nginx-module-v1.20.tar.gz && tar xzf nginx.tar.gz && \
    cp fastdfs-nginx-module-1.20/src/mod_fastdfs.conf /etc/fdfs/ && \
    sed -i 's/!g_http_params.support_multi_range/0/' fastdfs-nginx-module-1.20/src/common.c && \
    cd nginx-1.24.0 && \
    CFLAGS="-Wno-error" ./configure --add-module=/tmp/fastdfs-nginx-module-1.20/src \
        --prefix=/usr/local/nginx --with-http_ssl_module && \
    make && make install && rm -rf /tmp/*

# 7. 编译安装 spawn-fcgi 进程管理器
COPY docker/sources/spawn-fcgi.tar.gz /tmp/
RUN cd /tmp && tar xzf spawn-fcgi.tar.gz && cd spawn-fcgi-1.6.3 && \
    ./autogen.sh && ./configure && make && make install && rm -rf /tmp/*

# 8. 创建运行所需目录并初始化 FastDFS 配置文件
RUN mkdir -p /home/cloud/uploads /home/yuqing/fastdfs /usr/local/nginx/html /var/log/nginx /var/run/fdfs /etc/fdfs

RUN cp /etc/fdfs/tracker.conf.sample /etc/fdfs/tracker.conf && \
    cp /etc/fdfs/storage.conf.sample /etc/fdfs/storage.conf && \
    cp /etc/fdfs/client.conf.sample /etc/fdfs/client.conf && \
    sed -i 's|^#*tracker_server=.*|tracker_server=127.0.0.1:22122|' /etc/fdfs/storage.conf && \
    sed -i 's|^#*tracker_server=.*|tracker_server=127.0.0.1:22122|' /etc/fdfs/client.conf && \
    touch /etc/fdfs/http.conf && \
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

# 9. 拷贝配置文件、前端页面与后端 C 源码
COPY docker/nginx.conf /usr/local/nginx/conf/nginx.conf
COPY docker/start.sh /start.sh
RUN sed -i 's/\r$//' /start.sh && chmod +x /start.sh
COPY web/index.html /usr/local/nginx/html/index.html
COPY docker/server/ /home/cloud/server/

# 10. 编译 7 个核心 C 语言 FastCGI 业务程序
RUN cd /home/cloud/server && \
    gcc -o login_fcgi login_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o register_fcgi register_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o upload_fcgi upload_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -lfdfsclient -lfastcommon -lpthread -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o download_fcgi download_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -lfdfsclient -lfastcommon -lpthread -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o filelist_fcgi filelist_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o delete_fcgi delete_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -lfdfsclient -lfastcommon -lpthread -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    gcc -o share_fcgi share_fcgi.c -lfcgi -lmysqlclient -lhiredis -lssl -lcrypto -I/usr/include/fastdfs -I/usr/include/fastcommon && \
    chmod +x *_fcgi

# 11. 暴露 Web 80 端口，并设定启动入口脚本
EXPOSE 80
CMD ["/start.sh"]
