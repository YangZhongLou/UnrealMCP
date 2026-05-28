# 2. 架构设计

> [← 返回导航](README.md) | [← 上一页: 项目概述](01-overview.md) | [下一页: 目录结构 →](03-directory-structure.md)

## 2.1 整体架构

```text
AI Client (Claude / Cursor / Trae)
    │ stdio (MCP Protocol / JSON-RPC 2.0)
    ▼
MCP Server (Rust + rmcp SDK)
    │ TCP Socket (127.0.0.1:13377)
    ▼
Unreal Editor Plugin (C++ Editor Module)
    │ FRunnable TCP Server Thread
    │ Command Dispatch
    ▼
Unreal Editor API (Game Thread)
```

### 数据流

1. AI Client 发送 MCP tool call → Rust MCP Server
2. MCP Server 序列化为 JSON → TCP Socket → Unreal Plugin
3. Unreal Plugin 接收、解析、分发到 Command Handler
4. Handler 通过 Editor API 执行操作、返回结果
5. 结果序列化为 JSON → TCP Socket → MCP Server → AI Client

## 2.2 技术栈

| 组件 | 技术 | 理由 |
| --- | --- | --- |
| MCP Server | Rust + `rmcp` crate + tokio | 高性能、类型安全、原生 MCP SDK |
| Unreal Plugin | C++ Editor Module | 直接访问 Editor API |
| 进程间通信 | TCP Socket (localhost:13377) | 简单可靠，跨平台 |
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
  "success": true,
  "result": {
    "actor_name": "MyLight",
    "class": "APointLight"
  }
}
```

## 2.4 线程模型

- Unreal Plugin TCP Server 运行在独立线程 (`FRunnable`)
- 命令处理在主线程（Game Thread）上执行（连接处理在 FRunnable thread）
- Rust MCP Server 使用 tokio 异步运行时处理并发

## 2.5 命令分发

MCPCommandServer 收到 JSON request 后，按 `method` 字段分发到对应 Handler:

```text
ProcessCommand("spawn_actor")       → HandleSpawnActor()
ProcessCommand("create_blueprint")   → HandleCreateBlueprint()
ProcessCommand("get_asset_list")     → HandleGetAssetList()
ProcessCommand("add_blueprint_node") → HandleAddBlueprintNode()
...
```

每个 Command 文件 (.cpp) 包含同类工具的所有 Handler 函数，无需头文件（通过 forward declaration 注册）。

> [← 返回导航](README.md) | [← 上一页: 项目概述](01-overview.md) | [下一页: 目录结构 →](03-directory-structure.md)
