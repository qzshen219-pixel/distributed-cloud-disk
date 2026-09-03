#!/bin/bash
BASE="http://127.0.0.1:8080"

echo "=== 登录 ==="
LOGIN=$(curl -s -X POST "$BASE/login" -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}')
TOKEN=$(echo "$LOGIN" | grep -o '"token":"[^"]*"' | cut -d'"' -f4)
echo "token=$TOKEN"

echo ""
echo "=== 文件列表（带 token）==="
curl -s "$BASE/filelist?user_id=1&token=$TOKEN"

echo ""
echo "=== 上传新文件 ==="
echo "Test content 2" > /tmp/test2.txt
curl -s -X POST "$BASE/upload?token=$TOKEN" -F "user=alice" -F "file=@/tmp/test2.txt;filename=test2.txt;md5=def456;size=15"

echo ""
echo "=== 文件列表（再次）==="
curl -s "$BASE/filelist?user_id=1&token=$TOKEN"
