# Status Board

> 项目当前状态一览。每个 Phase 更新一次。
> 历史进度详见 [10-milestones.md](10-milestones.md)。

| 字段 | 值 |
| --- | --- |
| **当前 Phase** | Phase 22 — GameThread 修复 + 真实 UE 测试 ✅ |
| **工作流步骤** | 6/6 — Commit |
| **分支** | `feature/unreal-mcp-init` |

## 当前任务

所有任务已完成。

## Phase 22 成果

### GameThread 修复 (15 个 Handler)

所有涉及 UE 写操作的 C++ Handler 已从 MCP 子线程派发到 GameThread：

| 文件 | Handler |
|------|---------|
| ActorCommands.cpp | SpawnActor, DestroyActor, GetActorList, SetActorTransform, SetActorProperty, GetActorProperty, DuplicateActor, FindActorsByClass, SpawnBlueprintActor |
| EditorCommands.cpp | CreateLevel, SaveCurrentLevel, GetCurrentLevel, FocusViewport, SelectActor |
| ComponentCommands.cpp | GetActorComponents, AddComponent, RemoveComponent |
| MaterialCommands.cpp | FindActor helper |

### 真实 UE 环境测试 (11 tests)

| 测试 | 覆盖工具 |
|------|----------|
| `test_real_ue_check_connection` | get_editor_info |
| `test_real_ue_create_level` | create_level |
| `test_real_ue_spawn_destroy_actor` | spawn_actor, destroy_actor |
| `test_real_ue_spawn_actor_with_defaults` | spawn_actor, destroy_actor |
| `test_real_ue_get_current_level` | get_current_level |
| `test_real_ue_transform_and_property` | set_actor_transform, get_actor_property |
| `test_real_ue_select_and_focus` | select_actor, focus_viewport, get_selected_actors |
| `test_real_ue_save_current_level` | create_level, save_current_level |
| `test_real_ue_get_actor_components` | get_actor_components |
| `test_real_ue_duplicate_actor` | duplicate_actor |
| `test_real_ue_find_actors_by_class` | find_actors_by_class |

### 流程改进

- QA 技能：强制性两层测试体系 (Mock + 真实 UE)
- 真实 UE 测试指南：线程派发、弹窗抑制、API 适配清单
- Markdown-writer 技能：补 MD022/MD031/MD032 + 强制 linter

### UE 5.7 兼容性修复

- `ANY_PACKAGE` → `FindFirstObject<T>`
- `bIsArray` → `ContainerType`
- `FConsoleObjectVisitor` lambda → delegate + `BindLambda`
- `AddFunctionGraph(nullptr)` → `(UFunction*)nullptr`
- `UEditorLevelLibrary` → `ULevelEditorSubsystem`
- `BlueprintGraph` 模块依赖补充

## 阻塞项

- 无

## 快速统计

- 工具总数: 58
- Mock 测试: 10/10 通过
- 真实 UE 测试: 11/11 通过 (单独跑)
- 最近构建: ✅ `cargo build` 通过
