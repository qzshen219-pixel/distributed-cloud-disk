#!/bin/bash
BASE="http://localhost:8080"
echo "=== 登录测试 ==="
curl -s --max-time 5 -X POST "$BASE/login" \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"123456"}'
echo ""
echo "=== 页面测试 ==="
curl -s --max-time 5 "$BASE/" | head -3
