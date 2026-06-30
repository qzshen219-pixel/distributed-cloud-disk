FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libmysqlclient-dev \
    libssl-dev \
    libevent-dev \
    libpcre3-dev \
    zlib1g-dev \
    libfastcommon-dev \
    mysql-server \
    mysql-client \
    redis-server \
    spawn-fcgi \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install FastDFS
RUN cd /tmp && \
    wget -q https://github.com/happyfish100/libfastcommon/archive/refs/tags/V1.0.80.tar.gz -O libfastcommon.tar.gz && \
    tar xzf libfastcommon.tar.gz && cd libfastcommon-1.0.80 && \
    ./make.sh && ./make.sh install && \
    cd /tmp && \
    wget -q https://github.com/happyfish100/fastdfs/archive/refs/tags/V6.12.2.tar.gz -O fastdfs.tar.gz && \
    tar xzf fastdfs.tar.gz && cd fastdfs-6.12.2 && \
    ./make.sh && ./make.sh install && \
    rm -rf /tmp/*

# Install Nginx with FastDFS module
RUN cd /tmp && \
    wget -q https://github.com/happyfish100/fastdfs-nginx-module/archive/refs/tags/V1.22.tar.gz -O nginx-module.tar.gz && \
    tar xzf nginx-module.tar.gz && \
    wget -q https://nginx.org/download/nginx-1.24.0.tar.gz -O nginx.tar.gz && \
    tar xzf nginx.tar.gz && cd nginx-1.24.0 && \
    ./configure --add-module=/tmp/fastdfs-nginx-module-1.22/src \
        --prefix=/usr/local/nginx \
        --with-http_ssl_module && \
    make && make install && \
    rm -rf /tmp/*

# Create directories
RUN mkdir -p /home/cloud/uploads \
    /usr/local/nginx/html \
    /var/log/nginx \
    /var/run/fdfs \
    /etc/fdfs

# Copy FastDFS configs
RUN cp /etc/fdfs/tracker.conf.sample /etc/fdfs/tracker.conf && \
    cp /etc/fdfs/storage.conf.sample /etc/fdfs/storage.conf && \
    cp /etc/fdfs/client.conf.sample /etc/fdfs/client.conf

# Copy nginx module config
RUN cp /tmp/fastdfs-nginx-module-1.22/src/mod_fastdfs.conf /etc/fdfs/ 2>/dev/null; \
    cp /usr/local/nginx/conf/mime.types /etc/fdfs/ 2>/dev/null; true

# Copy project files
COPY docker/nginx.conf /usr/local/nginx/conf/nginx.conf
COPY docker/start.sh /start.sh
COPY web/index.html /usr/local/nginx/html/index.html

# Copy server source code
COPY docker/server/ /home/cloud/server/

# Build FastCGI programs
RUN cd /home/cloud/server && \
    gcc -o login_fcgi login_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o register_fcgi register_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o upload_fcgi upload_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o download_fcgi download_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o filelist_fcgi filelist_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o delete_fcgi delete_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    gcc -o share_fcgi share_fcgi.c -lfcgi -lmysqlclient -lssl -lcrypto && \
    chmod +x *_fcgi

RUN chmod +x /start.sh

EXPOSE 80

CMD ["/start.sh"]
