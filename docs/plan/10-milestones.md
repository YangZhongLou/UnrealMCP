# 6. 里程碑计划

> [← 返回导航](README.md) | [← 上一页: 风险与缓解](09-risks.md)

## 实际进度

| 阶段 | 版本 | 目标 | 工具数 | 状态 |
|------|------|------|--------|------|
| Phase 1 | v0.1 | 基础通信 + 测试框架 | 3 | ✅ 完成 |
| Phase 2 | v0.2 | Actor 操作 + 编辑器控制 + 蓝图创建 | 9 | ✅ 完成 |
| Phase 3 | v0.3 | Content Browser 资产操作 | 4 | ✅ 完成 |
| Phase 4 | v0.3 | 截图 + C++ 代码生成 | 2 | ✅ 完成 |
| Phase 5 | v0.3 | Component 和 Level 操作 | 3 | ✅ 完成 |
| Phase 6 | v0.3 | 移除组件 + Focus viewport | 2 | ✅ 完成 |
| Phase 7 | v0.3 | Selection + Static Mesh 操作 | 3 | ✅ 完成 |
| Phase 8 | v0.4 | Material 操作 | 3 | ✅ 完成 |
| Phase 9 | v0.4 | Actor 搜索 + Blueprint 生成 | 2 | ✅ 完成 |
| Phase 10 | v0.4 | Input 模拟 + Camera 查询 | 2 | ✅ 完成 |
| Phase 11 | v0.4 | Lights + Particle Effects | 2 | ✅ 完成 |
| Phase 12 | v0.4 | Tags + View modes + Debug 可视化 | 3 | ✅ 完成 |
| Phase 13 | v0.5 | Blueprint Graph 编辑 (节点添加/连接/查询) | 3 | ✅ 完成 |
| Phase 14 | v0.5 | Blueprint Variable 管理 (添加/删除变量) | 2 | ✅ 完成 |
| Phase 15 | v0.5 | Asset Import/Export | 2 | ✅ 完成 |
| Phase 16 | v0.6 | 实时日志推送 | 1 | ✅ 完成 |
| Phase 17 | v0.6 | UI 自动化 | 3 | ✅ 完成 |
| Phase 18 | v0.7 | 蓝图函数图操作 | 3 | ✅ 完成 |
| Phase 19 | v0.8 | API 文档 | 0 | ✅ 完成 |
| **累计** | | | **57** | |

## 工具分布

| 类别 | 工具数 | 工具 |
|------|--------|------|
| Actor 操作 | 9 | spawn, destroy, duplicate, list, transform, property get/set, find by class, spawn blueprint |
| 编辑器操作 | 14 | editor info, console command, save level, PIE start/stop, screenshot, focus viewport, current level, selected actors, select actor, get ue logs, **execute editor command**, **focus editor panel**, **get editor commands** |
| 蓝图操作 | 11 | create, compile, info, add node, connect, get graph, add/remove variable, **create/delete function graph**, **list graphs** |
| 资产操作 | 6 | list, info, delete, rename, import, export |
| 组件操作 | 3 | get components, add, remove |
| 材质操作 | 3 | set material, create instance, set parameter |
| 网格/光照/特效 | 3 | set static mesh, light parameters, spawn effect |
| 输入/相机 | 2 | simulate key, viewport camera |
| 视口/调试 | 3 | view mode, show debug, add actor tag |
| 关卡/代码 | 2 | open level, generate C++ class |
| 连接 | 1 | check connection |

## 待定功能

| 功能 | 优先级 | 说明 |
|------|--------|------|
| 实时日志推送 | 高 | UE 编辑器日志实时推送到 MCP Server |
| UI 自动化 | 中 | Slate 菜单命令、编辑器面板操作 |
| 蓝图函数图操作 | 中 | 在蓝图中创建/编辑 Function Graph |
| 性能/压力测试 | 中 | 大场景操作响应时间、并发连接、内存泄漏测试 |
| API 文档 | 中 | 所有 Tools 的详细说明文档 |
| API 文档 | 中 | 所有 Tools 的详细说明文档 | ✅ 完成 |
| monolith 重构 | 低 | 拆分 server.rs 为模块化 tool 文件 |

## 版本规划

| 版本 | 目标 | 预计 |
|------|------|------|
| v0.5 | 当前 — 15 个 Phase 完成，50 个工具 | ✅ 完成 |
| v0.6 | 实时日志推送 + UI 自动化 | TBD |
| v1.0 | 全量测试 + 文档完善 + 正式发布 | TBD |

# 7. 开发规范

## 7.1 代码规范

- C++: 遵循 Unreal Engine 编码规范
- Rust: 遵循 Rust API Guidelines + clippy 规范
- 所有公共 API 必须添加文档注释

## 7.2 提交规范

- 使用语义化提交信息: `feat:`, `fix:`, `docs:`, `refactor:`
- 每个 Phase 完成后打 tag

## 7.3 测试规范

- 新功能必须附带测试
- 提交前运行 `cargo test` 和 `cargo clippy`
- 关键路径必须有集成测试覆盖

> [← 返回导航](README.md) | [← 上一页: 风险与缓解](09-risks.md)
