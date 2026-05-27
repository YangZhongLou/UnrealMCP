# CLAUDE.md — UnrealMCP Project Guidelines

## 开发流程 (MUST FOLLOW)

7 阶段流水线，不得跳过，任一阶段发现计划不合理则打回阶段 1。

```
1.Plan → 2.Architect → 3.Implement → 4.Review → 5.Test → 6.Document → 7.Commit
  ↑         │               │             │          │            │
  └─────────┴───────────────┴─────────────┴──────────┴────────────┘
```

**详细流程、每阶段 gates、3-step 新工具添加模板 → `/dev-flow` 技能。**

### 阶段速查

| 阶段 | 技能 | 核心产物 | Gate |
|------|------|----------|------|
| 1. Plan | `/pm` | 任务列表 + 估算 + 退出标准 | <1天/任务，binary done 条件 |
| 2. Architect | `/architect` | API 签名 + 数据流 + 文件清单 | UE API 可行 |
| 3. Implement | `/programmer` | 3 文件代码 + cargo build 通过 | 全部 3 文件修改，编译通过 |
| 4. Review | `/code-review` | 审查修复 | 命名/安全/null 检查 |
| 5. Test | `/qa-engineer` | 测试通过 | 必填/可选/无效参数覆盖 |
| 6. Document | `/md-writer` | 7 文档更新 + md-lint | 文档完整 |
| 7. Commit | `/git-flow` | git push | 推送成功 |

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
- **51 tools** across 11 categories — see README.md for full list
- Current branch: `feature/unreal-mcp-init`
- server.rs is ~1300+ lines (monolith — refactoring deferred)

## Key Constraints

- TCP only on localhost (127.0.0.1:13377) — never expose to network
- All UE Editor API calls execute on the main thread (Game Thread)
- JSON request/response pattern: one request, one response per connection
- No persistent TCP connections — connect, send command, receive response, disconnect
