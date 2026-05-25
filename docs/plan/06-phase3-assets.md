# Phase 3: 资产操作 (v0.3)

**目标**: 实现 Content Browser 资产操作
**时间**: 5-7 天
**依赖**: Phase 2

## 4.3.1 资产查询工具

| Tool | 功能 | 优先级 |
|------|------|--------|
| `get_asset_list` | 列出指定路径下的资产 | P0 |
| `get_asset_info` | 获取资产详细信息 | P0 |
| `search_assets` | 按名称/类型搜索资产 | P1 |

## 4.3.2 资产操作工具

| Tool | 功能 | 优先级 |
|------|------|--------|
| `rename_asset` | 重命名资产 | P1 |
| `move_asset` | 移动资产路径 | P1 |
| `delete_asset` | 删除资产 | P1 |
| `import_asset` | 导入外部文件 | P2 |
| `export_asset` | 导出资产 | P2 |

## 4.3.3 测试

- [ ] **资产列表测试**: 验证路径过滤和返回格式
- [ ] **CRUD 测试**: create → read → update → delete 完整流程
- [ ] **路径边界测试**: 无效路径、空路径、深层嵌套路径

## 退出标准
- 能查询和操作 Content Browser 中的资产
- 资产路径处理正确
- 资产操作测试通过
