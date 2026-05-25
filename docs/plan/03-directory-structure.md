# 3. 目录结构

> [← 返回导航](README.md) | [← 上一页: 架构设计](02-architecture.md) | [下一页: Phase 1 →](04-phase1-foundation.md)

```
UnrealMCP/
├── MCP_Server/                    # Rust MCP Server
│   ├── Cargo.toml
│   ├── src/
│   │   ├── main.rs                # 程序入口
│   │   ├── server.rs              # MCP Server + Tools
│   │   └── unreal_client.rs       # TCP Client 连接 Unreal
│   └── tests/                     # 测试目录
│       ├── test_unreal_client.rs  # TCP Client 单元测试
│       ├── test_server.rs         # MCP Server 集成测试
│       └── mock_unreal_server.rs  # Mock TCP Server
│
├── UnrealPlugin/                  # Unreal Engine 插件
│   ├── UnrealMCP.uplugin
│   └── Source/
│       └── UnrealMCP/
│           ├── UnrealMCP.Build.cs
│           ├── Private/
│           │   ├── UnrealMCP.cpp
│           │   ├── MCPCommandServer.cpp
│           │   └── Commands/
│           │       ├── ActorCommands.cpp
│           │       ├── BlueprintCommands.cpp
│           │       ├── AssetCommands.cpp
│           │       └── EditorCommands.cpp
│           └── Public/
│               ├── UnrealMCP.h
│               └── MCPCommandServer.h
│
├── SkillHub/                      # git submodule (技能库)
├── .trae/skills/                  # 软链接到 SkillHub 技能
├── docs/
│   └── plan/                      # 开发计划文档
│       ├── 01-overview.md
│       ├── 02-architecture.md
│       ├── 03-directory-structure.md
│       ├── 04-phase1-foundation.md
│       ├── 05-phase2-core-tools.md
│       ├── 06-phase3-assets.md
│       ├── 07-phase4-advanced.md
│       ├── 08-phase5-release.md
│       ├── 09-risks.md
│       └── 10-milestones.md
├── PLAN.md                        # 本计划文档（汇总版）
└── README.md                      # 项目说明
```

> [← 返回导航](README.md) | [← 上一页: 架构设计](02-architecture.md) | [下一页: Phase 1 →](04-phase1-foundation.md)
