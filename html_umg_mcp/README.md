# html_umg（离线烟测）

> **主入口已并入 UnrealMCP Rust MCP**（`MCP_Server`）。
> 请用主服务的 `analyze_html_layout` / `generate_umg_from_html`，
> **不要**再单独配置 `html-umg` MCP。

本目录仅保留 Python 解析器与 `smoke_parse.py`，便于不启 Rust 时做离线对照。

## 主 MCP 工具（推荐）

在已配置的 `unreal` / UnrealMCP stdio 服务中调用：

| Tool | 说明 |
| --- | --- |
| `analyze_html_layout` | HTML → 控件树 JSON |
| `generate_umg_from_html` | 解析并生成 WBP（需 Editor） |
| `generate_umg_from_json` | JSON → WBP |
| `generate_umg_widget` | 底层 UE 命令封装 |
| `list_html_umg_capabilities` | 能力列表 |

文档：[../docs/html-umg-mcp.md](../docs/html-umg-mcp.md) ·
[Documents/Design/ui/html-umg-mcp.md](../../../../Documents/Design/ui/html-umg-mcp.md)

## 离线烟测

```powershell
cd ClanSimulator\Plugins\UnrealMCP\html_umg_mcp
python smoke_parse.py `
  D:\Mine\unreal_projects\gu-dao-ren\Art\workspace\hud-html-mockup\index.html
```
