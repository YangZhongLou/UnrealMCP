# UnrealMCP 配置文档

## 概述

UnrealMCP 支持通过配置文件和环境变量自定义端口、地址等行为，无需修改源码即可适配不同环境。

---

## C++ 插件端口配置

编辑项目 `Config/DefaultEngine.ini`，在 `[UnrealMCP]` 段下配置：

```ini
[UnrealMCP]
; 旧协议（直接 JSON）服务器端口 — Rust MCP 客户端默认连接此端口
CommandServerPort=13377

; 新协议（MCP JSON-RPC 长度前缀）服务器端口
JsonRpcServerPort=13379
```

| 键 | 默认值 | 说明 |
|---|---|---|
| `CommandServerPort` | `13377` | 旧协议 server，处理所有 Actor/蓝图/资产/编辑器工具 |
| `JsonRpcServerPort` | `13379` | 新协议 server，处理 UMG/相机相关工具 |

> 修改后需**重启 Unreal Editor** 生效。

---

## Rust MCP Server 地址配置

Rust 端通过**环境变量** `UNREAL_MCP_ADDR` 指定连接地址：

```bash
# Windows PowerShell
$env:UNREAL_MCP_ADDR="127.0.0.1:13377"

# Windows CMD
set UNREAL_MCP_ADDR=127.0.0.1:13377

# Linux / macOS
export UNREAL_MCP_ADDR=127.0.0.1:13377
```

| 变量 | 默认值 | 说明 |
|---|---|---|
| `UNREAL_MCP_ADDR` | `127.0.0.1:13377` | Rust MCP Server 连接 Unreal Editor 的地址和端口 |

> 未设置环境变量时使用默认值，与 `DefaultEngine.ini` 中的 `CommandServerPort` 保持一致即可。

---

## Claude Code `.mcp.json` 配置

项目根目录 `.mcp.json` 示例：

```json
{
  "mcpServers": {
    "unreal": {
      "command": "D:\\Playground\\TA-Playground\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13377"
      }
    }
  }
}
```

### 多项目 / 多端口场景

如果你在同一台机器上运行多个 Unreal 项目，可以为每个项目分配不同端口：

**项目 A — `Config/DefaultEngine.ini`：**
```ini
[UnrealMCP]
CommandServerPort=13377
JsonRpcServerPort=13379
```

## HTML → UMG（主 MCP 内置）

已并入 UnrealMCP 主服务，勿单独配置 `html-umg`：

- 工具：`analyze_html_layout`、`generate_umg_from_html` 等
- 说明：[html-umg-mcp.md](html-umg-mcp.md)

详见上文「Claude Code `.mcp.json`」中的 `unreal` 服务即可。

## HTML → UMG MCP（已弃用独立进程）

~~独立 `html_umg_mcp/server.py`~~ → 逻辑在 `MCP_Server/src/html_umg.rs`。
`html_umg_mcp/` 仅保留 `smoke_parse.py` 离线对照。

**项目 B — `Config/DefaultEngine.ini`：**
```ini
[UnrealMCP]
CommandServerPort=13387
JsonRpcServerPort=13389
```

**项目 B 的 `.mcp.json`：**
```json
{
  "mcpServers": {
    "unreal": {
      "command": "...\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13387"
      }
    }
  }
}
```

---

## 防火墙 / 远程连接

默认只监听 `127.0.0.1`（本地回环）。如需远程连接，目前需要：

1. C++ 端修改 `UnrealMCP.cpp` 中 `ListenAddr->SetLoopbackAddress()` 为 `SetAnyAddress()`
2. Rust 端 `UNREAL_MCP_ADDR` 设为对应 IP
3. 确保防火墙放行对应端口

> ⚠️ 远程连接会降低安全性，建议仅在可信内网使用。

---

## 配置检查清单

| 检查项 | 位置 |
|---|---|
| C++ 端口配置 | `Config/DefaultEngine.ini` → `[UnrealMCP]` |
| Rust 连接地址 | `UNREAL_MCP_ADDR` 环境变量 |
| Claude Code MCP 配置 | 项目根目录 `.mcp.json` |
| 端口是否冲突 | `netstat -ano \| findstr 13377` |
| 配置生效 | 重启 Unreal Editor + 重新运行 MCP Server |
