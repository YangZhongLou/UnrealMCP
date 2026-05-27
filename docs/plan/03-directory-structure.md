# 3. 目录结构

> [← 返回导航](README.md) | [← 上一页: 架构设计](02-architecture.md) | [下一页: Phase 1 →](04-phase1-foundation.md)

```text
UnrealMCP/
├── MCP_Server/                    # Rust MCP Server
│   ├── Cargo.toml
│   ├── src/
│   │   ├── main.rs                # 程序入口 (tokio main)
│   │   ├── server.rs              # MCP Server + 51 个 Tool 定义
│   │   ├── unreal_client.rs       # TCP Client (连接/发送/接收)
│   │   ├── lib.rs                 # Library root
│   │   └── tools/                 # [未使用] 废弃的模块化重构尝试
│   │       ├── mod.rs
│   │       ├── actor_tools.rs
│   │       └── editor_tools.rs
│   └── tests/
│       ├── mock_unreal_server.rs  # Mock TCP Server
│       ├── test_server.rs         # MCP Server 集成测试
│       └── test_unreal_client.rs  # TCP Client 单元测试
│
├── UnrealPlugin/                  # Unreal Engine 插件
│   ├── UnrealMCP.uplugin
│   ├── Config/
│   ├── Binaries/
│   └── Source/
│       └── UnrealMCP/
│           ├── UnrealMCP.Build.cs
│           ├── Private/
│           │   ├── UnrealMCP.cpp          # 插件入口
│           │   ├── MCPCommandServer.cpp   # TCP Server + 命令分发 (51 个 dispatch)
│           │   ├── LogCaptureDevice.cpp   # UE 日志捕获 (FOutputDevice + 环形缓冲区)
│           │   └── Commands/
│           │       ├── ActorCommands.cpp   # Actor: spawn/destroy/duplicate/transform/property/search/tag
│           │       ├── BlueprintCommands.cpp # BP: create/compile/info/node/pin/graph/variable
│           │       ├── AssetCommands.cpp   # Asset: list/info/delete/rename/import/export
│           │       ├── ComponentCommands.cpp # Component: get/add/remove
│           │       ├── EditorCommands.cpp  # Editor: PIE/screenshot/console/focus/select/camera/input/light/debug
│           │       └── MaterialCommands.cpp # Material: apply/instance/parameter
│           └── Public/
│               ├── UnrealMCP.h
│               ├── MCPCommandServer.h
│               └── LogCaptureDevice.h     # 日志捕获设备声明
│
├── SkillHub/                      # git submodule (技能库)
├── docs/
│   └── plan/                      # 开发计划文档
│       ├── README.md              # 导航索引
│       ├── 01-overview.md         # 项目概述
│       ├── 02-architecture.md     # 架构设计
│       ├── 03-directory-structure.md # 目录结构
│       ├── 04-phase1-foundation.md   # Phase 1 计划
│       ├── 05-phase2-core-tools.md   # Phase 2 计划
│       ├── 06-phase3-assets.md       # Phase 3 计划
│       ├── 07-phase4-advanced.md     # Phase 4 计划
│       ├── 08-phase5-release.md      # Phase 5 计划
│       ├── 09-risks.md               # 风险与缓解
│       └── 10-milestones.md          # 里程碑与当前进度
└── README.md                      # 项目说明 (50 工具列表 + 安装指南)
```

## 文件规模

| 文件 | 行数 (约) | 职责 |
| --- | --- | --- |
| server.rs | 1300+ | 50 个 MCP Tool 定义 |
| MCPCommandServer.cpp | 430+ | TCP Server + 命令分发 |
| BlueprintCommands.cpp | 650+ | 8 个 Blueprint 相关 Handler |
| EditorCommands.cpp | 500+ | 10 个 Editor 相关 Handler |
| ActorCommands.cpp | 400+ | 9 个 Actor 相关 Handler |
| AssetCommands.cpp | 180+ | 6 个 Asset 相关 Handler |
| ComponentCommands.cpp | 120+ | 3 个 Component 相关 Handler |
| MaterialCommands.cpp | 100+ | 3 个 Material 相关 Handler |

> [← 返回导航](README.md) | [← 上一页: 架构设计](02-architecture.md) | [下一页: Phase 1 →](04-phase1-foundation.md)
