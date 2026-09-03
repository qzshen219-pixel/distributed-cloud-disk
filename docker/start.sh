#!/bin/bash

echo "========================================="
echo "  Distributed Cloud Disk - Starting..."
echo "========================================="

# Start MySQL
echo "[1/5] Starting MySQL..."
service mysql start 2>/dev/null || mysqld_safe --skip-grant-tables &
for i in $(seq 1 30); do
    mysqladmin ping --silent >/dev/null 2>&1 && break
    sleep 1
done
if ! mysqladmin ping --silent >/dev/null 2>&1; then
    echo "MySQL failed to start"
    exit 1
fi

# Create database and user
echo "[2/5] Setting up database..."
mysql -e "CREATE DATABASE IF NOT EXISTS file_cloud;" 2>/dev/null
mysql -e "CREATE USER IF NOT EXISTS 'cloud_user'@'localhost' IDENTIFIED BY 'Cloud@2026#Secure';" 2>/dev/null
mysql -e "GRANT ALL ON file_cloud.* TO 'cloud_user'@'localhost';" 2>/dev/null
mysql -e "FLUSH PRIVILEGES;" 2>/dev/null

# Create tables
mysql -u cloud_user -p'Cloud@2026#Secure' file_cloud << 'EOF'
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(32) NOT NULL,
    nickname VARCHAR(50),
    email VARCHAR(100),
    phone VARCHAR(20),
    token VARCHAR(64),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_login DATETIME
);

CREATE TABLE IF NOT EXISTS files (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    filename VARCHAR(255) NOT NULL,
    file_id VARCHAR(128) NOT NULL,
    file_size BIGINT,
    file_md5 VARCHAR(32),
    storage_type VARCHAR(20) NOT NULL DEFAULT 'local',
    status VARCHAR(20) NOT NULL DEFAULT 'active',
    upload_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    download_count INT DEFAULT 0,
    folder_id INT DEFAULT 0,
    share_token VARCHAR(64),
    share_expire DATETIME,
    is_public TINYINT DEFAULT 0,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

SET @add_storage_type = IF(
    EXISTS(
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'files'
          AND column_name = 'storage_type'
    ),
    'SELECT 1',
    'ALTER TABLE files ADD COLUMN storage_type VARCHAR(20) NOT NULL DEFAULT ''local'''
);
PREPARE add_storage_type_stmt FROM @add_storage_type;
EXECUTE add_storage_type_stmt;
DEALLOCATE PREPARE add_storage_type_stmt;

SET @add_status = IF(
    EXISTS(
        SELECT 1 FROM information_schema.columns
        WHERE table_schema = DATABASE()
          AND table_name = 'files'
          AND column_name = 'status'
    ),
    'SELECT 1',
    'ALTER TABLE files ADD COLUMN status VARCHAR(20) NOT NULL DEFAULT ''active'''
);
PREPARE add_status_stmt FROM @add_status;
EXECUTE add_status_stmt;
DEALLOCATE PREPARE add_status_stmt;

UPDATE files
SET storage_type = 'fastdfs'
WHERE file_id LIKE 'group%'
  AND storage_type = 'local';

-- Insert test user (password: 123456)
INSERT IGNORE INTO users (username, password, nickname) VALUES ('alice', 'e10adc3949ba59abbe56e057f20f883e', 'Alice');
EOF

# Tokens are stored in Redis now; remove legacy database copies.
mysql -u cloud_user -p'Cloud@2026#Secure' file_cloud \
    -e "UPDATE users SET token=NULL WHERE token IS NOT NULL;" 2>/dev/null || exit 1

echo "[3/5] Starting Redis..."
if [ -n "${REDIS_SENTINELS:-}" ]; then
    first_sentinel=${REDIS_SENTINELS%%,*}
    sentinel_host=${first_sentinel%:*}
    sentinel_port=${first_sentinel##*:}
    for i in $(seq 1 30); do
        redis-cli -h "$sentinel_host" -p "$sentinel_port" \
            SENTINEL get-master-addr-by-name \
            "${REDIS_MASTER_NAME:-cloud-master}" 2>/dev/null | grep -q . && break
        sleep 1
    done
    if ! redis-cli -h "$sentinel_host" -p "$sentinel_port" \
        SENTINEL get-master-addr-by-name \
        "${REDIS_MASTER_NAME:-cloud-master}" 2>/dev/null | grep -q .; then
        echo "Redis Sentinel failed to discover a master"
        exit 1
    fi
else
    redis-server --daemonize yes || exit 1
    for i in $(seq 1 30); do
        redis-cli -h "${REDIS_HOST:-127.0.0.1}" \
            -p "${REDIS_PORT:-6379}" ping 2>/dev/null | grep -q PONG && break
        sleep 1
    done
    if ! redis-cli -h "${REDIS_HOST:-127.0.0.1}" \
        -p "${REDIS_PORT:-6379}" ping 2>/dev/null | grep -q PONG; then
        echo "Redis failed to start"
        exit 1
    fi
fi

# Start FastDFS Tracker
echo "[4/5] Starting FastDFS..."
CONTAINER_IP=$(hostname -i | awk '{print $1}')
if [ -z "$CONTAINER_IP" ] || [ "$CONTAINER_IP" = "127.0.0.1" ]; then
    echo "Unable to determine a non-loopback container IP"
    exit 1
fi

mkdir -p /home/yuqing/fastdfs/tracker /home/yuqing/fastdfs/storage
sed -i 's|^base_path=.*|base_path=/home/yuqing/fastdfs/tracker|' /etc/fdfs/tracker.conf
sed -i 's|^base_path=.*|base_path=/home/yuqing/fastdfs/storage|' /etc/fdfs/storage.conf
sed -i 's|^store_path0=.*|store_path0=/home/yuqing/fastdfs/storage|' /etc/fdfs/storage.conf
sed -i "s|^tracker_server=.*|tracker_server=${CONTAINER_IP}:22122|" /etc/fdfs/storage.conf
sed -i "s|^tracker_server=.*|tracker_server=${CONTAINER_IP}:22122|" /etc/fdfs/client.conf
sed -i "s|^tracker_server=.*|tracker_server=${CONTAINER_IP}:22122|" /etc/fdfs/mod_fastdfs.conf

fdfs_trackerd /etc/fdfs/tracker.conf start || exit 1

# Wait for tracker
sleep 2

# Start FastDFS Storage
fdfs_storaged /etc/fdfs/storage.conf start || exit 1

# Wait for storage
sleep 2
if ! pgrep -x fdfs_storaged >/dev/null; then
    echo "FastDFS Storage failed to start"
    tail -n 20 /home/yuqing/fastdfs/storage/logs/storaged.log 2>/dev/null
    exit 1
fi

# Start Nginx
echo "[5/5] Starting Nginx..."
/usr/local/nginx/sbin/nginx

# Start FastCGI programs
echo "Starting FastCGI programs..."
FCGI_WORKERS=4
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9000 -f /home/cloud/server/register_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9001 -f /home/cloud/server/login_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9999 -f /home/cloud/server/upload_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9003 -f /home/cloud/server/download_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9004 -f /home/cloud/server/filelist_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9005 -f /home/cloud/server/delete_fcgi
spawn-fcgi -F "$FCGI_WORKERS" -a 127.0.0.1 -p 9006 -f /home/cloud/server/share_fcgi

echo "========================================="
echo "  All services started!"
echo "  Access: http://localhost:80"
echo "  Account: alice / 123456"
echo "========================================="

# Keep container running
exec tail -f /dev/null
