# UnrealMCP 使用指南（防踩坑版）

> 目标：让 AI 助手通过 MCP 协议控制 Unreal Editor。本指南针对 `gu-dao-ren` 项目，覆盖最常见错误。

---

## 1. 目录结构先搞清楚

`ClanSimulator/Plugins/UnrealMCP/` 本身就是完整的 Unreal 插件，**不要**再把它当成"包装目录"去复制里面的 `UnrealPlugin/`。

```text
ClanSimulator/Plugins/UnrealMCP/       ← 这就是插件根目录
├── UnrealMCP.uplugin                  ← 插件描述文件
├── Source/UnrealMCP/                  ← C++ 源码
├── MCP_Server/                        ← Rust MCP Server
│   ├── src/main.rs                    ← 程序入口
│   ├── src/server.rs                  ← MCP tools 定义
│   ├── src/unreal_client.rs           ← TCP 客户端
│   └── target/release/unreal-mcp-server.exe
├── docs/                              ← 文档
├── README.md
└── UnrealPlugin/                      ← 当前为空/占位，勿复制
```

`.mcp.json` 里指向的可执行文件是：

```text
ClanSimulator/Plugins/UnrealMCP/MCP_Server/target/release/unreal-mcp-server.exe
```

---

## 2. 双端口架构

插件启动时会同时开两个 TCP 服务：

| 服务 | 默认端口 | 协议 | 用途 | AI 客户端连哪个 |
|---|---|---|---|---|
| **Command Server** | `13377` | 行尾 `\n` 分隔的 JSON | Actor / 蓝图 / 资产 / 编辑器工具 | **Rust MCP Server 默认连这个** |
| **JSON-RPC Server** | `13379` | JSON-RPC 长度前缀 | UMG / 运行时相机 / 部分高级功能 | 其他自定义 MCP 客户端 |

> 日常通过 Rust MCP Server 使用时，只需要关心 `13377`。

---

## 3. 快速开始（5 步）

### 3.1 确保插件已启用

打开 `ClanSimulator/ClanSimulator.uproject`，确认包含：

```json
{
  "Name": "UnrealMCP",
  "Enabled": true
}
```

### 3.2 构建 Rust MCP Server

```powershell
cd "D:\Mine\unreal_projects\gu-dao-ren\ClanSimulator\Plugins\UnrealMCP\MCP_Server"
cargo build --release
```

构建产物必须是这个文件：

```text
MCP_Server/target/release/unreal-mcp-server.exe
```

如果 `.mcp.json` 指向的是 `debug` 目录，请改用 `--release` 或修改 `.mcp.json` 路径。

### 3.3 配置端口（一般保持默认即可）

在 `ClanSimulator/Config/DefaultEngine.ini` 中添加：

```ini
[UnrealMCP]
CommandServerPort=13377
JsonRpcServerPort=13379
```

> 修改后**必须重启 Unreal Editor** 才能生效。

### 3.4 配置 AI 客户端

项目根目录 `.mcp.json`：

```json
{
  "mcpServers": {
    "unreal": {
      "type": "stdio",
      "command": "D:\\Mine\\unreal_projects\\gu-dao-ren\\ClanSimulator\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13377"
      }
    }
  }
}
```

注意点：

- `command` 必须是**绝对路径**，且路径中每个 `\` 都要写成 `\\`。
- `UNREAL_MCP_ADDR` 必须与 `CommandServerPort` 一致。
- 不需要 `"type": "stdio"` 的客户端可省略，Claude Code / Kimi Code CLI 等常用配置支持该字段。

### 3.5 启动使用

1. 启动 Unreal Editor 并打开 `ClanSimulator.uproject`。
2. 等待编辑器完全加载（出现视口）。
3. 在 AI 客户端中调用 `check_unreal_connection` 验证连接。
4. 正常调用工具，例如：

```text
get_actor_list
run_console_command stat fps
spawn_actor PointLight name=MyLight location=[0,0,200]
```

---

## 4. 验证连接

首选工具：

```text
check_unreal_connection
```

成功返回示例：

```text
Connected: {"engineVersion":"5.7.0","projectName":"ClanSimulator",...}
```

失败返回示例：

```text
Not connected to Unreal Engine. Run the Unreal Editor with the UnrealMCP plugin.
```

也可以在 UE 编辑器 **Output Log** 里搜索 `LogUnrealMCP`，应能看到：

```text
LogUnrealMCP: MCP JSON-RPC Server started on port 13379
LogUnrealMCP: MCP Command Server started on port 13377
```

如果只有 `JSON-RPC Server` 而**没有** `Command Server`，说明你运行在 **Game/Standalone** 模式，Command Server 只在 **Editor** 模式下启动。

---

## 5. 常见错误排查

| 现象 | 最可能原因 | 解决办法 |
|---|---|---|
| `command not found` / 找不到 `unreal-mcp-server.exe` | Rust Server 没构建，或 `.mcp.json` 路径错误 | 运行 `cargo build --release`，检查 `.mcp.json` 绝对路径和双反斜杠 |
| `Not connected to Unreal Engine` | UE 编辑器未启动，或插件未启用 | 启动 UE Editor，确认 `.uproject` 中 `UnrealMCP` 已启用 |
| `Failed to bind socket to port 13377` | 端口被占用 | 关闭另一个 UE 实例，或修改 `DefaultEngine.ini` 换一个端口，同时修改 `.mcp.json` 的 `UNREAL_MCP_ADDR` |
| `Connection closed by Unreal` / 工具无响应 | 请求发到了 JSON-RPC 端口（13379），或工具名拼错 | Rust Server 默认连 `13377`；确认工具名是 `snake_case`（如 `spawn_actor`） |
| `Failed: ...` | 工具参数错误 | 对照 `docs/api-reference.md` 检查参数名和类型 |
| 工具返回成功但场景没变化 | PIE 模式下部分编辑器工具不生效 | 先 `stop_play_in_editor`，再执行操作 |
| 只有 `MCP JSON-RPC Server started`，没有 `Command Server` | 运行在 Standalone / Packaged 模式 | Command Server 只在 **WITH_EDITOR** 构建中启动，使用 Editor 模式 |
| 修改端口后仍连不上 | 未重启 UE Editor | 任何 `[UnrealMCP]` 配置修改后必须重启编辑器 |

---

## 6. 重要使用约定

### 6.1 Actor 名称

- 很多工具通过 **Actor Label**（场景大纲里显示的名字）查找 Actor。
- 如果创建时没指定 `name`，UE 会自动生成如 `PointLight_0`。
- 名称重复时，通常只匹配第一个。

### 6.2 资产路径格式

Content Browser 里的 `/Game/Foo/Bar` 对应磁盘路径 `Content/Foo/Bar.uasset`。调用工具时使用 `/Game` 形式：

```text
spawn_blueprint_actor /Game/Blueprints/BP_MyActor
```

### 6.3 JSON 数组顺序

位置/旋转/缩放数组统一是 `[x, y, z]`，不是 `[y, x, z]`：

```text
location=[0,0,200]
rotation=[0,0,90]   ; pitch=0, yaw=0, roll=90
```

### 6.4 Editor 模式 vs Runtime 模式

- **Command Server（13377）**：只在编辑器内可用，用于场景编辑、资产操作、蓝图修改。
- **JSON-RPC Server（13379）**：部分工具可在 PIE / Runtime 下工作，如运行时相机控制。
- 如果 AI 客户端主要做关卡编辑，确保 UE 处于编辑器模式，而不是 PIE。

### 6.5 不要手动运行 Rust Server

`.mcp.json` 的 `stdio` 模式由 AI 客户端在启动时**自动拉起** Rust Server。不需要在命令行里手动执行 `unreal-mcp-server.exe`。

如果手动运行，它会因为没有 stdio 输入而立即退出。

---

## 7. 修改端口示例

如果 `13377` 被占用：

**`ClanSimulator/Config/DefaultEngine.ini`**：

```ini
[UnrealMCP]
CommandServerPort=13387
JsonRpcServerPort=13389
```

**`.mcp.json`**：

```json
{
  "mcpServers": {
    "unreal": {
      "type": "stdio",
      "command": "D:\\Mine\\unreal_projects\\gu-dao-ren\\ClanSimulator\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13387"
      }
    }
  }
}
```

然后重启 Unreal Editor 和 AI 客户端。

---

## 8. 客户端配置示例

### Claude Code / Cursor / Kimi Code CLI

使用项目根目录的 `.mcp.json` 即可（见 3.4）。

### Claude Desktop

在 `claude_desktop_config.json` 中添加：

```json
{
  "mcpServers": {
    "unreal": {
      "command": "D:\\Mine\\unreal_projects\\gu-dao-ren\\ClanSimulator\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13377"
      }
    }
  }
}
```

---

## 9. 开发/调试小贴士

1. **看 UE Output Log**：连接失败时，这里的信息最准。
2. **手动测试 TCP**：用 PowerShell 的 `Test-NetConnection -ComputerName 127.0.0.1 -Port 13377` 确认端口在监听。
3. **Rebuild Rust Server**：修改 Rust 代码后需要 `cargo build --release`；C++ 代码修改后需要重新编译 UE Editor。
4. **清理锁**：如果 AI 客户端卡死，Rust Server 进程可能还活着，用任务管理器结束 `unreal-mcp-server.exe`。

---

## 10. 相关文档

- 完整 API 参数：`docs/api-reference.md`
- 端口/环境变量配置：`docs/configuration.md`
- 开发流程：`CLAUDE.md`
- 工具列表：`README.md`
