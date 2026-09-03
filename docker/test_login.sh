#!/bin/bash
echo "=== 容器状态 ==="
docker ps | grep cloud

echo ""
echo "=== 从容器内部测试登录 ==="
docker exec cloud-disk curl -s --max-time 5 -X POST http://127.0.0.1:80/login -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'

echo ""
echo "=== 从WSL2测试登录 ==="
curl -s --max-time 5 -X POST http://127.0.0.1:8080/login -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'

echo ""
echo "=== 从Windows测试 ==="
curl.exe -s --max-time 5 -X POST http://localhost:8080/login -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'
