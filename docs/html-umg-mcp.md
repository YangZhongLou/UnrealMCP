# HTML → UMG（UnrealMCP 内置）

HTML 设计稿转 Widget Blueprint 已作为 **UnrealMCP 主 MCP** 的一组工具，
不再需要单独的 `html-umg` 服务进程。

项目侧工作流（HTML 初版 → 转 UMG → 接线）：
[Documents/Design/ui/html-umg-workflow.md](../../../../Documents/Design/ui/html-umg-workflow.md)。

## 工具

| Tool | 说明 |
| --- | --- |
| `analyze_html_layout` | HTML → widget_tree JSON（可离线） |
| `generate_umg_from_html` | 解析 HTML 并调用 UE `generate_umg_widget` |
| `generate_umg_from_json` | 已有 JSON → 生成 WBP |
| `generate_umg_widget` | 直接提交 widget_tree |
| `list_html_umg_capabilities` | 锚点 / 剖面约定 |

实现：

- Rust：`MCP_Server/src/html_umg.rs`
- UE：`Source/UnrealMCP/Private/Commands/UmgCommands.cpp`（`generate_umg_widget`）
- Python 兼容入口：`html_umg_mcp/push_main_hud.py`

## 解析契约

转换器不执行浏览器 CSS。支持两种输入：

| 模式 | 用途 | 契约 |
| --- | --- | --- |
| `hud-html-mockup` profile | 当前局内 HUD | Rust / Python profile 固定生成布局，从约定 `id` 提取文案 |
| `data-umg-*` | 通用新屏 | `type/name/x/y/w/h/anchor`；可选 `text/visibility/z` |

锚点支持 `top-left`、`top-right`、`bottom-left`、`bottom-right`、`center`、
`fill`。`visibility` 支持 `visible`、`collapsed`、`hidden`、
`hit-test-invisible`、`self-hit-test-invisible`。

生成侧命中默认值（JSON 未写 `visibility` 时）：

| 控件 | 默认 visibility |
| --- | --- |
| `CanvasPanel` / 其它 Panel | `self-hit-test-invisible`（空白区不吞地图点击） |
| `TextBlock` | `hit-test-invisible` |
| `Image` | `visible`（面板底板仍挡住地图） |
| `Button` | `visible` |

若 HTML 出现 `data-umg-type` 但解析失败（自闭合标签、缺必填属性等），
两端解析器直接返回 `status=error`，**不会**回退到内置 HUD profile。

> 生成会删除并重建同名 WBP。生成资产不得手工维护布局、动画或事件图。

TCP 请求应以单换行结尾，UE 端累计读取到完整帧后再反序列化；为兼容旧本地客户端，
无换行但已构成完整 JSON 的请求也会立即处理。响应以双换行结尾。
`generate_umg_widget` 在 GameThread 执行资产替换，并为 UE 5.8 补齐 `FAppTime`
上下文，避免删除旧 WBP 时渲染线程触发 ensure。

## 使用

确保 Cursor / Claude 只配置 **UnrealMCP**（Rust `unreal-mcp-server`），例如：

```json
{
  "mcpServers": {
    "unreal": {
      "command": "D:/Mine/unreal_projects/gu-dao-ren/ClanSimulator/Plugins/UnrealMCP/MCP_Server/target/release/unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13377"
      }
    }
  }
}
```

生成前打开 Editor。示例：

- `analyze_html_layout` → `html_path` =
  `.../Art/workspace/hud-html-mockup/index.html`
- `generate_umg_from_html` → 同上，产出 `/Game/UI/HUD/WBP_MainHud_FromHtml`

本地离线契约检查：

```powershell
python html_umg_mcp/push_main_hud.py --analyze-only
```

解析器回归：

```powershell
python -m unittest -v html_umg_mcp/test_parser.py
cd MCP_Server
cargo test html_umg
```

## 与旧独立 MCP

`html_umg_mcp/` 仅保留 Python profile、离线契约检查与 TCP 兼容入口，
**勿再注册为独立 MCP**。
