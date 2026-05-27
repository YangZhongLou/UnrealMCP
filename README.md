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
│           │       ├── EditorCommands.cpp
│           │       └── MaterialCommands.cpp
│           └── Public/
│               ├── UnrealMCP.h
│               └── MCPCommandServer.h
├── SkillHub/             # git submodule (技能库)
├── docs/plan/            # 开发计划文档
└── README.md
```

## 已实现功能 (57 个工具)

### Actor 操作 (9)
| Tool | 描述 |
|------|------|
| `spawn_actor` | 在场景中创建 Actor |
| `destroy_actor` | 删除指定 Actor |
| `duplicate_actor` | 复制 Actor |
| `get_actor_list` | 获取场景中所有 Actor |
| `set_actor_transform` | 设置 Actor 位置/旋转/缩放 |
| `set_actor_property` | 设置 Actor 属性值 |
| `get_actor_property` | 获取 Actor 属性值 |
| `find_actors_by_class` | 按类名搜索 Actor |
| `spawn_blueprint_actor` | 从 Blueprint 资产生成 Actor |

### 编辑器操作 (11)
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
| `get_selected_actors` | 获取当前选中的 Actor |
| `select_actor` | 选中指定 Actor |
| `get_ue_logs` | 获取 UE 编辑器输出日志 |
| `execute_editor_command` | 执行编辑器控制台命令 |
| `focus_editor_panel` | 聚焦编辑器面板 |
| `get_editor_commands` | 列出可用的编辑器命令 |

### 蓝图操作 (11)
| Tool | 描述 |
|------|------|
| `create_blueprint` | 创建 Blueprint 资产 |
| `compile_blueprint` | 编译 Blueprint |
| `get_blueprint_info` | 获取 Blueprint 信息与变量 |
| `add_blueprint_node` | 添加节点到蓝图图形 (Event/CallFunction/Variable 等) |
| `connect_blueprint_pins` | 连接两个节点之间的引脚 |
| `get_blueprint_graph` | 获取蓝图图形结构 (节点/引脚/连接) |
| `add_blueprint_variable` | 给蓝图添加变量 (支持 int/float/bool/Vector 等) |
| `remove_blueprint_variable` | 从蓝图删除变量 |
| `create_blueprint_function_graph` | 创建蓝图函数图 |
| `list_blueprint_graphs` | 列出蓝图所有图 |
| `delete_blueprint_graph` | 删除蓝图函数图 |

### 资产操作 (6)
| Tool | 描述 |
|------|------|
| `get_asset_list` | 列出资产 |
| `get_asset_info` | 获取资产信息 |
| `delete_asset` | 删除资产 |
| `rename_asset` | 重命名资产 |
| `import_asset` | 导入外部文件到 Content Browser (FBX/PNG/WAV 等) |
| `export_asset` | 导出资产到磁盘 |

### 组件操作 (3)
| Tool | 描述 |
|------|------|
| `get_actor_components` | 获取 Actor 上的所有组件 |
| `add_component` | 添加组件到 Actor |
| `remove_component` | 从 Actor 移除组件 |

### 材质操作 (3)
| Tool | 描述 |
|------|------|
| `set_material` | 应用材质到网格组件 |
| `create_material_instance` | 创建材质实例 (MIC/MID) |
| `set_material_parameter` | 设置材质参数 (标量/向量) |

### 网格 / 光照 / 特效 (3)
| Tool | 描述 |
|------|------|
| `set_static_mesh` | 设置 StaticMeshComponent 的网格 |
| `set_light_parameters` | 设置光源参数 (强度/颜色/阴影) |
| `spawn_effect` | 生成 Niagara/Cascade 粒子特效 |

### 输入 / 相机 (2)
| Tool | 描述 |
|------|------|
| `simulate_key` | 模拟键盘按键 (按下/释放/点击) |
| `get_viewport_camera` | 获取编辑器视口相机位置与旋转 |

### 视口 / 调试 (3)
| Tool | 描述 |
|------|------|
| `set_view_mode` | 设置视口渲染模式 (Lit/Unlit/Wireframe 等) |
| `show_debug` | 切换调试可视化 (碰撞/导航/边界) |
| `add_actor_tag` | 给 Actor 添加标签 |

### 关卡 / 代码 (2)
| Tool | 描述 |
|------|------|
| `open_level` | 打开关卡 |
| `generate_cpp_class` | 生成 C++ 类模板 |

### 连接 (1)
| Tool | 描述 |
|------|------|
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
- "在 BP_MyActor 的 BeginPlay 后面添加一个 PrintString 节点"

## 技术栈

- **MCP Server**: Rust, tokio, rmcp, serde
- **Unreal Plugin**: C++, Unreal Editor Module API
- **通信**: TCP Socket + JSON

## 开发计划

详见 [docs/plan/](docs/plan/)

## License

MIT
