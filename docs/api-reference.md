# UnrealMCP API Reference

57 个 MCP 工具，11 个类别。

---

## 1. 连接 (1)

### check_unreal_connection
检查与 Unreal Editor 的连接状态。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| (无参数) | — | — | — | — |

**返回**: 连接状态文本。

---

## 2. Actor 操作 (9)

### spawn_actor
在场景中创建 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| class_name | string | ✅ | — | Actor 类名，如 `StaticMeshActor`, `PointLight` |
| name | string | | — | 可选的 Actor 名称 |
| location | [f64;3] | | [0,0,0] | 位置 [x, y, z] |
| rotation | [f64;3] | | [0,0,0] | 旋转 [pitch, yaw, roll] |
| scale | [f64;3] | | [1,1,1] | 缩放 [x, y, z] |

### destroy_actor
删除指定 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |

### duplicate_actor
复制 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | 源 Actor 名称 |
| new_name | string | | — | 新 Actor 名称（可选） |

### get_actor_list
获取场景中所有 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| (无参数) | — | — | — | — |

### set_actor_transform
设置 Actor 位置/旋转/缩放。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| location | [f64;3] | | — | 位置 [x, y, z] |
| rotation | [f64;3] | | — | 旋转 [pitch, yaw, roll] |
| scale | [f64;3] | | — | 缩放 [x, y, z] |

### set_actor_property
设置 Actor 属性值。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| property | string | ✅ | — | 属性名 |
| value | string | ✅ | — | 属性值（JSON 字符串） |

### get_actor_property
获取 Actor 属性值。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| property | string | ✅ | — | 属性名 |

### find_actors_by_class
按类名搜索 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| class_name | string | ✅ | — | Actor 类名 |

### spawn_blueprint_actor
从 Blueprint 资产生成 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| name | string | | — | 可选的 Actor 名称 |
| location | [f64;3] | | [0,0,0] | 位置 |
| rotation | [f64;3] | | [0,0,0] | 旋转 |

---

## 3. 编辑器操作 (14)

### get_editor_info
获取引擎版本、项目名称等编辑器信息。无参数。

### run_console_command
执行 Unreal 控制台命令。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| command | string | ✅ | — | 控制台命令，如 `stat fps` |

### save_current_level
保存当前关卡。无参数。

### play_in_editor
启动 Play In Editor (PIE)。无参数。

### stop_play_in_editor
停止 Play In Editor (PIE)。无参数。

### take_screenshot
截取编辑器视口截图。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| filename | string | | `screenshot` | 文件名（不含扩展名） |

### focus_viewport
聚焦视口到指定 Actor 或坐标。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| actor_name | string | | — | 目标 Actor 名称（与 location 二选一） |
| location | [f64;3] | | — | 目标坐标 [x, y, z] |

### get_current_level
获取当前关卡名称、路径、Actor 数量。无参数。

### get_selected_actors
获取当前选中的 Actor 列表。无参数。

### select_actor
选中/取消选中指定 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| actor_name | string | ✅ | — | Actor 名称 |
| add_to_selection | bool | | false | 是否追加到当前选择 |

### get_ue_logs
获取 UE 编辑器输出日志。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| count | i32 | | 100 | 最大返回条数 (1-1000) |
| verbosity | string | | `Log` | 最低级别: Error/Warning/Log/Verbose/VeryVerbose |
| clear_after | bool | | false | 读取后清空缓冲区 |

### execute_editor_command
执行编辑器控制台命令（如 `undo`, `redo`, `newlevel`）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| command | string | ✅ | — | 命令名，自动尝试 `editor.<command>` 前缀 |

### focus_editor_panel
聚焦/切换到编辑器面板。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| panel | string | ✅ | — | ContentBrowser / WorldOutliner / Details / OutputLog / Layers / Viewport |

### get_editor_commands
列出控制台命令。用于发现可用命令。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| prefix | string | | `editor.` | 搜索前缀 |

---

## 4. 蓝图操作 (11)

### create_blueprint
创建 Blueprint 资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Blueprint 名称 |
| parent_class | string | ✅ | — | 父类名，如 `AActor` |
| path | string | | `/Game` | 资产路径 |

### compile_blueprint
编译 Blueprint。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |

### get_blueprint_info
获取 Blueprint 信息与变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |

### add_blueprint_node
向 Blueprint 图形添加节点。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| node_type | string | ✅ | — | Event / CallFunction / CustomEvent / VariableGet / VariableSet / PrintString |
| function_name | string | | — | 函数名（CallFunction 时必填） |
| variable_name | string | | — | 变量名（VariableGet/Set 时必填） |
| event_name | string | | — | 事件名（Event/CustomEvent 时必填） |
| position_x | i32 | | 0 | 节点 X 坐标 |
| position_y | i32 | | 0 | 节点 Y 坐标 |
| graph_type | string | | EventGraph | 目标图：EventGraph 或函数图名 |
| target_class | string | | — | 目标类（Event 时可选） |

### connect_blueprint_pins
连接两个节点之间的引脚。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| source_node_id | string | ✅ | — | 源节点 GUID |
| source_pin | string | ✅ | — | 源引脚名 |
| target_node_id | string | ✅ | — | 目标节点 GUID |
| target_pin | string | ✅ | — | 目标引脚名 |

### get_blueprint_graph
获取 Blueprint 图形结构。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| graph_type | string | | EventGraph | 目标图：EventGraph 或函数图名 |

### add_blueprint_variable
给 Blueprint 添加变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| variable_name | string | ✅ | — | 变量名 |
| variable_type | string | ✅ | — | int/float/bool/string/name/text/Vector/Rotator/Transform/Color/UObject类名 |
| is_array | bool | | false | 是否数组类型 |

### remove_blueprint_variable
从 Blueprint 删除变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| variable_name | string | ✅ | — | 要删除的变量名 |

### create_blueprint_function_graph
在 Blueprint 中创建新函数图。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| function_name | string | ✅ | — | 函数名 |
| category | string | | — | 函数分类（可选） |

### list_blueprint_graphs
列出 Blueprint 中所有图（EventGraph, FunctionGraphs, Delegates）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |

### delete_blueprint_graph
删除 Blueprint 中的函数图（不能删除 EventGraph）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | Blueprint 资产路径 |
| graph_name | string | ✅ | — | 函数图名称 |

---

## 5. 资产操作 (6)

### get_asset_list
列出 `/Game` 下的所有资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | | `/Game` | 搜索路径 |
| class_name | string | | — | 过滤类名（可选） |

### get_asset_info
获取资产详细信息。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | 资产路径 |

### delete_asset
删除资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| path | string | ✅ | — | 资产路径 |

### rename_asset
重命名/移动资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| source_path | string | ✅ | — | 源路径 |
| destination_path | string | ✅ | — | 目标路径 |

### import_asset
导入外部文件到 Content Browser。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| file_path | string | ✅ | — | 源文件绝对路径 (FBX/PNG/WAV) |
| destination_path | string | | `/Game` | Content Browser 目标路径 |

### export_asset
导出资产到磁盘。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| asset_path | string | ✅ | — | 资产路径 |
| output_dir | string | | `Saved/Exports` | 输出目录 |

---

## 6. 组件操作 (3)

### get_actor_components
获取 Actor 上的所有组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |

### add_component
给 Actor 添加组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| component_class | string | ✅ | — | 组件类名 |
| component_name | string | | — | 可选组件名 |

### remove_component
从 Actor 移除组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| component_name | string | ✅ | — | 组件名 |

---

## 7. 材质操作 (3)

### set_material
应用材质到网格组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| actor_name | string | ✅ | — | Actor 名称 |
| material_path | string | ✅ | — | 材质资产路径 |
| slot_index | i32 | | 0 | 材质槽索引 |

### create_material_instance
创建材质实例 (MIC)。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| parent_path | string | ✅ | — | 父材质路径 |
| instance_path | string | ✅ | — | 新实例路径 |

### set_material_parameter
设置材质实例参数。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| instance_path | string | ✅ | — | 材质实例路径 |
| parameter_name | string | ✅ | — | 参数名 |
| parameter_type | string | ✅ | — | Scalar / Vector |
| value | varies | ✅ | — | 标量值（float）或向量值（[r,g,b] 0-1） |

---

## 8. 网格/光照/特效 (3)

### set_static_mesh
设置 StaticMeshComponent 的网格。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| actor_name | string | ✅ | — | Actor 名称 |
| mesh_path | string | ✅ | — | Static Mesh 资产路径 |

### set_light_parameters
设置光源参数。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| actor_name | string | ✅ | — | Actor 名称 |
| intensity | f64 | | — | 光照强度 |
| color | [f64;3] | | — | 颜色 [r, g, b]（0-1） |
| cast_shadows | bool | | — | 是否投射阴影 |

### spawn_effect
生成 Niagara/Cascade 粒子特效。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| effect_path | string | ✅ | — | 特效资产路径 |
| location | [f64;3] | | [0,0,0] | 生成位置 |

---

## 9. 输入/相机 (2)

### simulate_key
模拟键盘按键。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| key | string | ✅ | — | 按键名，如 `SpaceBar`, `LeftMouseButton` |
| action | string | | `tap` | press / release / tap |

### get_viewport_camera
获取编辑器视口相机位置与旋转。无参数。

---

## 10. 视口/调试 (3)

### set_view_mode
设置视口渲染模式。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| mode | string | ✅ | — | Lit / Unlit / Wireframe / ShaderComplexity / etc. |

### show_debug
切换调试可视化。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| flag | string | ✅ | — | collision / navigation / bounds / etc. |
| enable | bool | | — | 不填则切换 |

### add_actor_tag
给 Actor 添加标签。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | Actor 名称 |
| tag | string | ✅ | — | 标签文本 |

---

## 11. 关卡/代码 (2)

### open_level
打开关卡。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| name | string | ✅ | — | 关卡名 |

### generate_cpp_class
生成 C++ 类模板。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| class_name | string | ✅ | — | 类名 |
| parent_class | string | ✅ | — | 父类，如 `AActor` |
| module | string | | 项目名 | 模块名 |
