#!/bin/bash
BASE="http://127.0.0.1:8080"

echo "=== 1. 登录 ==="
LOGIN=$(curl -s -X POST "$BASE/login" -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}')
TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)
USER_ID=$(echo "$LOGIN" | grep -o '"id":[0-9]*' | head -1 | cut -d: -f2)
echo "user_id=$USER_ID token=$TOKEN"

echo ""
echo "=== 2. 上传文件 ==="
echo "Hello World Test" > /tmp/test.txt
curl -s -X POST "$BASE/upload?token=$TOKEN" -F "user=alice" -F "file=@/tmp/test.txt;filename=test.txt;md5=abc123;size=17"

echo ""
echo "=== 3. 文件列表 ==="
curl -s "$BASE/filelist?user_id=$USER_ID"

echo ""
echo "=== 4. 下载文件 ==="
curl -s -o /tmp/downloaded.txt "$BASE/download?file_id=1&token=$TOKEN"
echo "下载内容:"
cat /tmp/downloaded.txt 2>/dev/null | head -3

echo ""
echo "=== 5. 删除文件 ==="
curl -s -X POST "$BASE/delete?token=$TOKEN" -H "Content-Type: application/json" -d '{"file_id":1}'

echo ""
echo "=== 6. 验证删除 ==="
curl -s "$BASE/filelist?user_id=$USER_ID"
