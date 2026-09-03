#!/bin/bash
echo "=== 登录测试 (从容器内部) ==="
docker exec cloud-disk curl -s --max-time 5 -X POST http://127.0.0.1:80/login -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'
echo ""
echo "=== 登录测试 (从WSL2) ==="
curl -s --max-time 5 -X POST http://127.0.0.1:8080/login -H "Content-Type: application/json" -d '{"username":"alice","password":"123456"}'
echo ""
echo "=== 容器状态 ==="
docker ps | grep cloud
