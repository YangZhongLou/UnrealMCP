# CLAUDE.md — UnrealMCP Project Guidelines

## 开发流程 (MUST FOLLOW)

6 阶段流水线，不得跳过。每个阶段分为 3 个子步骤：**Plan → Review → Work**。
任一子步骤发现计划不合理，打回该阶段的 Plan 重新计划。

```
1.Plan → 2.Architect → 3.Implement → 4.Test → 5.Document → 6.Commit
  ↑         │               │             │            │
  └─────────┴───────────────┴─────────────┴────────────┘
             任一阶段子步骤失败，打回该阶段 Plan
```

每个阶段子步骤：

```
[Phase]
  ├── Plan:   计划本阶段要完成什么
  ├── Review: 审查计划是否合理、可行
  └── Work:   执行（实现/测试/写文档/提交）
```

**详细流程、每阶段 gates、3-step 新工具添加模板 → `/dev-flow` 技能。**

### 阶段速查

| 阶段 | 技能 | Plan | Review | Work | Gate |
|------|------|------|--------|------|------|
| 1. Plan | `/pm` | 明确目标+范围 | 审查任务拆分合理性 | 输出任务列表 | <1天/任务，binary done |
| 2. Architect | `/architect` | 确定影响范围 | 审查 UE API 可行性 | 输出技术方案 | UE API 可用，无冲突 |
| 3. Implement | `/programmer` | 搭建函数骨架 | 审查签名+参数 | 填充实现+编译 | 3 文件全改，cargo build 过 |
| 4. Test | `/qa-engineer` | 列出测试用例 | 审查覆盖完整性 | 执行测试 | 必填/可选/无效全测 |
| 5. Document | `/md-writer` | 确认需更新文档 | 审查范围是否遗漏 | 更新文档+lint | 7 文档完整 |
| 6. Commit | `/git-flow` | 确认变更文件 | 审查 diff 范围 | stage+commit+push | 推送成功 |

---

## Build

```bash
# Rust MCP Server
cd MCP_Server && cargo build          # debug
cd MCP_Server && cargo build --release # release

# Unreal Plugin — build inside UE Editor via .uproject with plugin enabled
# No standalone C++ build; compile errors found via UE Editor compilation
```

## Project Architecture

```
AI Client ──stdio──▶ Rust MCP Server ──TCP:13377──▶ Unreal Plugin ──▶ UE Editor API
```

- **Rust side**: `server.rs` defines all `#[tool]` functions, sends JSON to Unreal via `unreal_client.rs`
- **C++ side**: `MCPCommandServer.cpp` runs a TCP server on port 13377, dispatches commands to handler functions
- **Command flow**: Rust `#[tool]` → `client.send_command("method_name", params)` → TCP → C++ `ProcessCommand()` → `HandleXxx()` → JSON response back

## Adding a New Tool

**必须同时修改 3 个文件**（详见 `/dev-flow`）：

1. `<Category>Commands.cpp` — C++ Handler (forward declare, JSON `"success"` required)
2. `MCPCommandServer.cpp` — forward declaration + `else if (Method == TEXT("..."))` dispatch
3. `server.rs` — `#[tool]` async fn (snake_case fn, camelCase JSON keys, lock client)

命名约定：C++ `HandleXxx()` / `TEXT("camelCase")`，Rust `snake_case` fn / `json!({"camelCase": v})`。

## Code Conventions

### C++
- Use `TEXT()` macro for all string literals passed to UE API
- JSON serialization: `FJsonSerializer::Serialize` + `TJsonWriterFactory<>`
- JSON parsing: `Params->GetStringField()`, `HasField()` before optional fields
- No header files for command handlers — forward declare in MCPCommandServer.cpp

### Rust
- Monolithic `server.rs` — all tools in one `impl` block (do NOT create new modules without approval)
- `json!({})` macro for building params maps
- Each tool is `async fn` taking `&self` + params returning `String`
- Error format: `format!("Error: {}", e)` for network errors

### Commit Messages
```
feat: Complete Phase <N> - <Description>
```
Use `feat:`, `fix:`, `docs:`, `refactor:`, `chore:` prefixes.

## Project State

- **Status Board**: [`docs/plan/current-status.md`](docs/plan/current-status.md) — 当前 Phase、活动任务、阻塞项的唯一真相源
- **54 tools** across 11 categories — see README.md for full list
- Current branch: `feature/unreal-mcp-init`
- server.rs is ~1300+ lines (monolith — refactoring deferred)

## Key Constraints

- TCP only on localhost (127.0.0.1:13377) — never expose to network
- All UE Editor API calls execute on the main thread (Game Thread)
- JSON request/response pattern: one request, one response per connection
- No persistent TCP connections — connect, send command, receive response, disconnect
