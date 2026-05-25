# Unreal MCP

Unreal Engine MCP (Model Context Protocol) 集成方案，让 AI 助手能够通过自然语言直接操作 Unreal Editor。

## 架构

```
AI Client (Claude/Cursor/Trae)
    │ stdio (MCP Protocol)
    ▼
MCP Server (Rust + rmcp)
    │ TCP Socket (127.0.0.1:13377)
    ▼
Unreal Editor Plugin (C++)
    │
    ▼
Unreal Editor API
```

## 项目结构

```
UnrealMCP/
├── MCP_Server/           # Rust MCP Server
│   ├── Cargo.toml
│   └── src/
│       ├── main.rs       # 程序入口
│       ├── server.rs     # MCP Server + Tools
│       └── unreal_client.rs  # TCP Client
├── UnrealPlugin/         # Unreal Engine C++ 插件
│   ├── UnrealMCP.uplugin
│   └── Source/
│       └── UnrealMCP/
│           ├── Private/
│           │   ├── UnrealMCP.cpp
│           │   ├── MCPCommandServer.cpp
│           │   └── Commands/
│           │       ├── ActorCommands.cpp
│           │       └── EditorCommands.cpp
│           └── Public/
│               ├── UnrealMCP.h
│               └── MCPCommandServer.h
├── SkillHub/             # git submodule (技能库)
├── .trae/skills/         # 软链接到 SkillHub 技能
└── PLAN.md               # 开发计划
```

## 已实现功能

### MCP Tools (Rust Server)

| Tool | 功能 |
|------|------|
| `check_unreal_connection` | 检查与 Unreal 的连接状态 |
| `spawn_actor` | 在场景中创建 Actor |
| `destroy_actor` | 删除指定 Actor |
| `get_actor_list` | 获取场景中所有 Actor 列表 |
| `run_console_command` | 执行 Unreal 控制台命令 |
| `save_current_level` | 保存当前关卡 |
| `play_in_editor` | 启动 Play In Editor |
| `stop_play_in_editor` | 停止 Play In Editor |
| `get_editor_info` | 获取编辑器信息 |

### Unreal Plugin Commands (C++)

| Command | 功能 |
|---------|------|
| `get_editor_info` | 返回引擎版本和项目名 |
| `spawn_actor` | 在场景中创建 Actor |
| `destroy_actor` | 销毁 Actor |
| `set_actor_transform` | 设置 Actor Transform |
| `get_actor_list` | 获取 Actor 列表 |
| `run_console_command` | 执行控制台命令 |
| `save_current_level` | 保存关卡 |
| `play_in_editor` | 启动 PIE |
| `stop_play_in_editor` | 停止 PIE |

## 安装使用

### 1. 构建 MCP Server (Rust)

```bash
cd MCP_Server
cargo build
```

### 2. 安装 Unreal Plugin

将 `UnrealPlugin/` 目录复制到你的 Unreal 项目的 `Plugins/` 目录下，命名为 `UnrealMCP`。

### 3. 配置 MCP Client

在 AI 客户端（如 Claude Desktop、Cursor）的 MCP 配置中添加：

```json
{
  "mcpServers": {
    "unreal": {
      "command": "path/to/unreal-mcp-server.exe"
    }
  }
}
```

### 4. 启动 Unreal Editor

启动带有 UnrealMCP 插件的 Unreal Editor，插件会自动在端口 13377 启动 TCP Server。

### 5. 使用

在 AI 客户端中，你可以使用自然语言控制 Unreal：

- "在场景中心创建一个点光源"
- "保存当前关卡"
- "启动 Play In Editor"
- "执行控制台命令 stat fps"

## 技术栈

- **MCP Server**: Rust, tokio, rmcp, serde
- **Unreal Plugin**: C++, Unreal Editor Module API
- **通信**: TCP Socket + JSON

## 开发计划

详见 [PLAN.md](PLAN.md)

## License

MIT
