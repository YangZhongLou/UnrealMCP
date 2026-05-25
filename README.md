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
│           │       ├── AssetCommands.cpp
│           │       ├── BlueprintCommands.cpp
│           │       ├── ComponentCommands.cpp
│           │       └── EditorCommands.cpp
│           └── Public/
│               ├── UnrealMCP.h
│               └── MCPCommandServer.h
├── SkillHub/             # git submodule (技能库)
├── .trae/skills/         # 软链接到 SkillHub 技能
└── PLAN.md               # 开发计划
```

## 已实现功能 (24 个工具)

### Actor 操作
| Tool | 描述 |
|------|------|
| `spawn_actor` | 在场景中创建 Actor |
| `destroy_actor` | 删除指定 Actor |
| `duplicate_actor` | 复制 Actor |
| `get_actor_list` | 获取场景中所有 Actor |
| `set_actor_transform` | 设置 Actor 位置/旋转/缩放 |
| `set_actor_property` | 设置 Actor 属性值 |
| `get_actor_property` | 获取 Actor 属性值 |

### 编辑器操作
| Tool | 描述 |
|------|------|
| `get_editor_info` | 获取编辑器/引擎信息 |
| `run_console_command` | 执行控制台命令 |
| `save_current_level` | 保存当前关卡 |
| `play_in_editor` | 启动 PIE |
| `stop_play_in_editor` | 停止 PIE |
| `take_screenshot` | 截取视口截图 |
| `focus_viewport` | 聚焦视口到 Actor 或坐标 |
| `get_current_level` | 获取当前关卡信息 |

### 蓝图操作
| Tool | 描述 |
|------|------|
| `create_blueprint` | 创建 Blueprint 资产 |
| `compile_blueprint` | 编译 Blueprint |
| `get_blueprint_info` | 获取 Blueprint 信息 |

### 资产操作
| Tool | 描述 |
|------|------|
| `get_asset_list` | 列出资产 |
| `get_asset_info` | 获取资产信息 |
| `delete_asset` | 删除资产 |
| `rename_asset` | 重命名资产 |

### 组件操作
| Tool | 描述 |
|------|------|
| `get_actor_components` | 获取 Actor 上的所有组件 |
| `add_component` | 添加组件到 Actor |
| `remove_component` | 从 Actor 移除组件 |

### 代码生成
| Tool | 描述 |
|------|------|
| `generate_cpp_class` | 生成 C++ 类模板 |
| `check_unreal_connection` | 检查连接状态 |

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
