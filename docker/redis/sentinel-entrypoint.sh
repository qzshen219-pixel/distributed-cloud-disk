#!/bin/sh
set -eu

: "${REDIS_PASSWORD:?REDIS_PASSWORD is required}"

master_ip=""
old_ifs=$IFS
IFS=,
for peer in ${REDIS_SENTINEL_PEERS:-}; do
    peer_host=${peer%:*}
    peer_port=${peer##*:}
    candidate=$(redis-cli -h "$peer_host" -p "$peer_port" --raw \
        SENTINEL get-master-addr-by-name \
        "${REDIS_MASTER_NAME:-cloud-master}" 2>/dev/null | sed -n '1p')
    if [ -n "$candidate" ]; then
        master_ip=$candidate
        break
    fi
done
IFS=$old_ifs

if [ -z "$master_ip" ]; then
    master_host=${REDIS_MASTER_HOST:-redis-master}
    for attempt in $(seq 1 30); do
        master_ip=$(getent hosts "$master_host" | awk 'NR==1 {print $1}')
        [ -n "$master_ip" ] && break
        sleep 1
    done
    if [ -z "$master_ip" ]; then
        echo "Cannot resolve Redis master: $master_host" >&2
        exit 1
    fi
fi

cat > /tmp/sentinel.conf <<EOF
port 26379
dir /tmp
sentinel monitor ${REDIS_MASTER_NAME:-cloud-master} ${master_ip} 6379 2
sentinel auth-pass ${REDIS_MASTER_NAME:-cloud-master} ${REDIS_PASSWORD}
sentinel down-after-milliseconds ${REDIS_MASTER_NAME:-cloud-master} 5000
sentinel failover-timeout ${REDIS_MASTER_NAME:-cloud-master} 15000
sentinel parallel-syncs ${REDIS_MASTER_NAME:-cloud-master} 1
EOF

exec redis-server /tmp/sentinel.conf --sentinel
