## KV Store操作

经典的三种操作，GET/DEL/SET，只不过换成了 JSON。

请求格式

```json
{
    "key": "key", // 多个 key 空格分隔
    "value": "value", // GET/DEL 为空
    "method": "GET/SET/DEL", // 操作方法
    "hashKey": 12345, // 哈希后的数字 key，uint64 类型
    "groupId": 1 // 分组 ID
}
```

响应格式

```json
{
    "code": 1, // 正数成功，非正数失败；对于 DEL，code 为成功删除的个数
    "value": "value" // GET 返回的值；对于其他操作，返回空
}
```

## 集群信息

集群初始化完成后，client 可以通过 GET Key `fleet_info` 请求集群信息，以字符串的形式 encode 在 返回的 value 中。

```json
{
    "nodes": [ // IP 地址的列表
        {
            "id": [1,4,7],
            "ip": "192.168.0.1:1001",
        },
        {
            "id": [2,5,8],
            "ip": "192.168.0.2:1001",
        },
        {
            "id": [3,6,9],
            "ip": "192.168.0.3:1001"
        }
    ],
    "fleetLeader": "1", // Leader 的编号
    "groups": [
        {
            "id": 1,
            "leader": 1,
            "nodes": [1, 2] // 分组 1 的节点列表
        },
        {
            "id": 2,
            "leader": 4,
            "nodes": [4, 3] // 分组 2 的节点列表
        },
        {
            "id": 3,
            "leader": 5,
            "nodes": [5, 6] // 分组 3 的节点列表
        }
    ]
}
```

如果由于各种变动原因，当前请求的节点不是 Group Leader，响应 value 中提供当前 Group 的 leader 编号，格式为 `leader: <leader_id>`。

## 集群操作

可以通过 GET/DEL `node_active` 操作让某个节点“挂掉”，此时该节点无需为 leader。在“挂掉”的时候，其仍可响应 GET `node_active` 来恢复正常，继续参加集群内的正常活动。这样，client 也能获知哪些节点是挂掉的。

返回随便什么都行。