# Phase 2: 核心工具实现 (v0.2)

**目标**: 实现场景操作和编辑器控制的核心工具
**时间**: 5-7 天
**依赖**: Phase 1

## 4.2.1 Actor 操作工具

| Tool | 功能 | 优先级 |
|------|------|--------|
| `spawn_actor` | 在场景中创建 Actor | P0 |
| `destroy_actor` | 删除指定 Actor | P0 |
| `set_actor_transform` | 设置 Actor 位置/旋转/缩放 | P0 |
| `get_actor_list` | 获取场景中所有 Actor 列表 | P0 |
| `set_actor_property` | 设置 Actor 属性 | P1 |
| `get_actor_property` | 获取 Actor 属性 | P1 |
| `duplicate_actor` | 复制 Actor | P2 |

## 4.2.2 编辑器控制工具

| Tool | 功能 | 优先级 |
|------|------|--------|
| `run_console_command` | 执行 Unreal 控制台命令 | P0 |
| `save_current_level` | 保存当前关卡 | P0 |
| `play_in_editor` | 启动 PIE | P0 |
| `stop_play_in_editor` | 停止 PIE | P0 |
| `get_editor_info` | 获取编辑器信息 | P0 |
| `open_level` | 打开指定关卡 | P1 |
| `take_screenshot` | 截图并返回 | P2 |

## 4.2.3 蓝图操作工具 (Phase 2.5)

| Tool | 功能 | 优先级 |
|------|------|--------|
| `create_blueprint` | 创建新蓝图 | P1 |
| `add_blueprint_node` | 添加蓝图节点 | P1 |
| `connect_blueprint_pins` | 连接蓝图引脚 | P1 |
| `compile_blueprint` | 编译蓝图 | P1 |

## 4.2.4 测试

- [ ] **Actor 命令测试**: spawn → get_list → destroy 完整流程
- [ ] **Transform 测试**: 设置位置/旋转/缩放后验证
- [ ] **PIE 测试**: play → stop 状态切换验证
- [ ] **错误处理测试**: 无效 Actor 名、无效类名等边界情况

```rust
// tests/test_actor_tools.rs
#[tokio::test]
async fn test_spawn_and_destroy_actor() {
    let server = setup_test_server().await;
    let result = server.spawn_actor("PointLight".into(), Some("TestLight".into()), None, None, None).await;
    assert!(result.contains("Spawned"));
    let list = server.get_actor_list().await;
    assert!(list.contains("TestLight"));
    let result = server.destroy_actor("TestLight".into()).await;
    assert!(result.contains("Destroyed"));
}
```

## 退出标准
- 所有 P0 工具可用
- AI 能通过自然语言创建 Actor、控制编辑器
- 工具返回结果清晰准确
- 核心工具单元测试覆盖率 > 80%
