# Redis Token高可用部署

该部署把登录 Token从 MySQL迁移到 Redis，并使用以下 Redis Sentinel拓扑：

```text
redis-master
  ├─ redis-replica-1
  └─ redis-replica-2

redis-sentinel-1 ┐
redis-sentinel-2 ├─ 监控 cloud-master，quorum=2
redis-sentinel-3 ┘
```

MySQL继续保存用户和文件元数据。当前高可用范围是 Token存储层；云盘业务、MySQL和 FastDFS仍是单容器开发部署。
Redis节点位于独立网络并使用固定容器IP，避免节点停止、恢复后IP变化造成Sentinel无法识别。单个Sentinel重启时会先向其他Sentinel查询当前主节点；整组重启时则从预设主节点重新建立拓扑。

> 该 Compose拓扑运行在同一台 Docker主机上，能够承受单个Redis容器故障，但不能承受整台主机故障。生产环境需要把Redis和Sentinel节点分散到至少三台主机或可用区。

## 启动

在 PowerShell中执行：

```powershell
Copy-Item .\docker\.env.ha.example .\docker\.env.ha
# 编辑 docker/.env.ha，修改 REDIS_PASSWORD
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml up -d --build
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml ps
```

如果原单机容器占用8080端口，先停止它：

```powershell
docker compose -f .\docker\docker-compose.yml down
```

## 验证主从状态

```powershell
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml exec redis-sentinel-1 redis-cli -p 26379 SENTINEL get-master-addr-by-name cloud-master
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml exec redis-master sh -c 'redis-cli -a "$REDIS_PASSWORD" INFO replication'
```

正常情况下应看到一个主节点和两个从节点。

## 验证Token

登录后，Token以以下键保存：

```text
cloud:token:<token>       -> 用户ID
cloud:user_token:<用户ID> -> 当前Token
```

两个键都有 `REDIS_TOKEN_TTL` 指定的TTL。同一用户再次登录时，旧Token会被删除。高可用模式下，登录接口会等待至少一个副本确认写入后再把Token返回给客户端，以降低主节点立即故障造成的Token丢失风险。

```powershell
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml exec redis-master sh -c 'redis-cli -a "$REDIS_PASSWORD" --scan --pattern "cloud:*"'
```

## 验证自动故障转移

1. 登录并保存Token。
2. 查询 Sentinel当前主节点。
3. 停止当前主节点。
4. 等待约10～20秒后再次查询主节点。
5. 使用原Token请求 `/filelist`，并重新登录签发新Token。

Sentinel切换期间接口可能短暂返回：

```json
{"code":"TOKEN_STORE_UNAVAILABLE","message":"Authentication service unavailable"}
```

这是服务不可用而不是凭证无效，客户端不应在收到该错误时删除本地Token。

## 停止

```powershell
docker compose --env-file .\docker\.env.ha -f .\docker\docker-compose.ha.yml down
```

保留数据卷时不要增加 `-v`。只有明确要删除数据库、文件和Redis数据时才执行 `down -v`。
