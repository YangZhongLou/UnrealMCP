# 2. 架构设计

## 2.1 整体架构

```
AI Client (Claude / Cursor / Trae)
    │ stdio (MCP Protocol / JSON-RPC 2.0)
    ▼
MCP Server (Rust + rmcp SDK)
    │ TCP Socket
    ▼
Unreal Editor Plugin (C++ Editor Module)
    │
    ▼
Unreal Editor API
```

## 2.2 技术栈

| 组件 | 技术 | 理由 |
|------|------|------|
| MCP Server | Rust + `rmcp` crate | 高性能、类型安全、原生 MCP SDK |
| Unreal Plugin | C++ Editor Module | 直接访问 Editor API |
| 进程间通信 | TCP Socket (localhost) | 简单可靠，跨平台 |
| 消息格式 | JSON | 人类可读，易调试 |

## 2.3 消息格式

### Request
```json
{
  "id": "cmd_001",
  "method": "spawn_actor",
  "params": {
    "className": "PointLight",
    "name": "MyLight",
    "location": [0, 0, 300]
  }
}
```

### Response
```json
{
  "id": "cmd_001",
  "success": true,
  "result": {
    "actor_name": "MyLight",
    "class": "APointLight"
  }
}
```

## 2.4 线程模型
- Unreal Plugin TCP Server 运行在独立线程 (FRunnable)
- 所有 Editor API 调用通过 AsyncTask 或 FFunctionGraphTask 调度到 Game Thread
- Rust MCP Server 使用 tokio 异步运行时处理并发
