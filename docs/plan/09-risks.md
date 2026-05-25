# 5. 风险与缓解

> [← 返回导航](README.md) | [← 上一页: Phase 5](08-phase5-release.md) | [下一页: 里程碑与规范 →](10-milestones.md)

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| Unreal Editor API 不稳定 | 中 | 高 | 使用稳定的 Editor Utility API，做好版本兼容层 |
| 多线程安全问题 | 高 | 高 | 所有 Editor API 调用必须在 Game Thread 上执行 |
| 大场景操作性能问题 | 中 | 中 | 添加超时机制，支持异步操作，分批处理 |
| 安全性问题 | 低 | 高 | 默认仅监听 localhost，不暴露到公网 |
| 版本兼容性 | 中 | 中 | 明确支持 Unreal 5.x，定期测试，条件编译 |
| MCP 协议变更 | 低 | 中 | 跟踪官方 SDK 更新，保持依赖版本可控 |
| Rust MCP SDK 成熟度 | 中 | 中 | 关注 rmcp crate 更新，必要时自行实现协议层 |

> [← 返回导航](README.md) | [← 上一页: Phase 5](08-phase5-release.md) | [下一页: 里程碑与规范 →](10-milestones.md)
