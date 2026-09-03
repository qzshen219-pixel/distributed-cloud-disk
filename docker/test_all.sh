#!/bin/bash
BASE="http://127.0.0.1:8080"

echo "=== 1. 登录 ==="
curl -s --max-time 5 -X POST "$BASE/login" -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'
echo ""

echo "=== 2. 注册 ==="
curl -s --max-time 5 -X POST "$BASE/register" -H "Content-Type: application/json" -d '{"username":"test3","password":"test123","nickname":"Test3"}'
echo ""

echo "=== 3. 登录 test3 ==="
LOGIN=$(curl -s --max-time 5 -X POST "$BASE/login" -H "Content-Type: application/json" -d '{"username":"test3","password":"test123"}')
echo "$LOGIN"
TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)
USER_ID=$(echo "$LOGIN" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
echo "token=$TOKEN user_id=$USER_ID"

echo ""
echo "=== 4. 上传文件 ==="
echo "Hello Test" > /tmp/upload.txt
curl -s --max-time 5 -X POST "$BASE/upload?token=$TOKEN" -F "user=test3" -F "file=@/tmp/upload.txt;filename=hello.txt;md5=abc;size=11"
echo ""

echo "=== 5. 文件列表 ==="
curl -s --max-time 5 "$BASE/filelist?user_id=$USER_ID&token=$TOKEN"
echo ""

echo "=== 6. 下载文件 ==="
curl -s --max-time 5 -o /tmp/download.txt "$BASE/download?file_id=1&token=$TOKEN"
echo "下载内容:"
cat /tmp/download.txt 2>/dev/null
echo ""

echo "=== 7. 容器状态 ==="
docker ps | grep cloud
