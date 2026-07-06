#!/bin/bash

echo "========================================="
echo "  Distributed Cloud Disk - Starting..."
echo "========================================="

# Start MySQL
echo "[1/5] Starting MySQL..."
service mysql start 2>/dev/null || mysqld_safe --skip-grant-tables &
sleep 3

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
    upload_time DATETIME DEFAULT CURRENT_TIMESTAMP,
    download_count INT DEFAULT 0,
    folder_id INT DEFAULT 0,
    share_token VARCHAR(64),
    share_expire DATETIME,
    is_public TINYINT DEFAULT 0,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

-- Insert test user (password: 123456)
INSERT IGNORE INTO users (username, password, nickname) VALUES ('alice', 'e10adc3949ba59abbe56e057f20f883e', 'Alice');
EOF

echo "[3/5] Starting Redis..."
redis-server --daemonize yes

# Start FastDFS Tracker
echo "[4/5] Starting FastDFS..."
fdfs_trackerd /etc/fdfs/tracker.conf start

# Wait for tracker
sleep 2

# Start FastDFS Storage
fdfs_storaged /etc/fdfs/storage.conf start

# Wait for storage
sleep 2

# Start Nginx
echo "[5/5] Starting Nginx..."
/usr/local/nginx/sbin/nginx

# Start FastCGI programs
echo "Starting FastCGI programs..."
spawn-fcgi -a 127.0.0.1 -p 9000 -f /home/cloud/server/register_fcgi
spawn-fcgi -a 127.0.0.1 -p 9001 -f /home/cloud/server/login_fcgi
spawn-fcgi -a 127.0.0.1 -p 9999 -f /home/cloud/server/upload_fcgi
spawn-fcgi -a 127.0.0.1 -p 9003 -f /home/cloud/server/download_fcgi
spawn-fcgi -a 127.0.0.1 -p 9004 -f /home/cloud/server/filelist_fcgi
spawn-fcgi -a 127.0.0.1 -p 9005 -f /home/cloud/server/delete_fcgi
spawn-fcgi -a 127.0.0.1 -p 9006 -f /home/cloud/server/share_fcgi

echo "========================================="
echo "  All services started!"
echo "  Access: http://localhost:80"
echo "  Account: alice / 123456"
echo "========================================="

# Keep container running
exec tail -f /dev/null
