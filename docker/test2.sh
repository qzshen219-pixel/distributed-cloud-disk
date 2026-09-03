#!/bin/bash
echo "=== 登录测试 ==="
curl -s --max-time 5 -X POST http://127.0.0.1:8080/login \
  -H "Content-Type: application/json" \
  -d '{"username":"alice","password":"123456"}'
echo ""
