# Phase 1: 基础通信层 (v0.1)

**目标**: 建立 MCP Server 与 Unreal Plugin 之间的双向通信
**时间**: 3-5 天

## 4.1.1 MCP Server (Rust)
- [ ] 初始化 Rust 项目，配置 `rmcp` crate 依赖
- [ ] 实现 MCP Server 基础框架 (stdio transport)
- [ ] 实现 TCP Client 连接 Unreal Plugin
- [ ] 定义 JSON 消息协议格式 (serde)
- [ ] 实现连接状态管理和重连机制
- [ ] 添加日志系统 (tracing)

## 4.1.2 Unreal Plugin (C++)
- [ ] 创建 Editor Module 基础结构
- [ ] 实现 TCP Server (FRunnable 多线程)
- [ ] 实现 JSON 请求解析和响应构造
- [ ] 添加 Editor 启动/关闭时的生命周期管理

## 4.1.3 测试
- [ ] **单元测试**: `unreal_client` 连接/重连/发送命令
- [ ] **集成测试**: Rust Server ↔ Mock TCP Server 端到端通信
- [ ] **协议测试**: JSON 序列化/反序列化边界情况
- [ ] **稳定性测试**: 连接断开重连、并发连接处理

```rust
// tests/test_unreal_client.rs
#[tokio::test]
async fn test_tcp_connection_and_command() {
    let mock = MockUnrealServer::start(13378).await;
    let mut client = UnrealClient::new("127.0.0.1:13378");
    let response = client.send_command("get_editor_info", json!({})).await.unwrap();
    assert_eq!(response["success"], true);
    mock.stop().await;
}
```

## 退出标准
- MCP Server 能成功连接 Unreal Plugin
- 双向 JSON 消息通信正常
- 连接断开时能优雅处理
- `cargo test` 全部通过
