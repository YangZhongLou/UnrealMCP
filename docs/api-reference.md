# UnrealMCP API Reference

73 个 MCP 工具，15 个类别。

---

## 1. 连接 (2)

### check_unreal_connection

检查与 Unreal Editor 的连接状态。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| (无参数) | — | — | — | — |

**返回**: 连接状态文本。

---

### get_editor_info

获取引擎版本、项目名称等编辑器信息。无参数。

---

## 2. Actor 操作 (9)

### spawn_actor

在场景中创建 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| class_name | string | ✅ | — | Actor 类名，如 `StaticMeshActor`, `PointLight` |
| name | string | | — | 可选的 Actor 名称 |
| location | [f64;3] | | [0,0,0] | 位置 [x, y, z] |
| rotation | [f64;3] | | [0,0,0] | 旋转 [pitch, yaw, roll] |
| scale | [f64;3] | | [1,1,1] | 缩放 [x, y, z] |

### destroy_actor

删除指定 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| name | string | ✅ | — | Actor 名称 |

### duplicate_actor

复制 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| name | string | ✅ | — | 源 Actor 名称 |
| new_name | string | | — | 新 Actor 名称（可选） |

### get_actor_list

获取场景中所有 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| (无参数) | — | — | — | — |

### set_actor_transform

设置 Actor 位置/旋转/缩放。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| name | string | ✅ | — | Actor 名称 |
| location | [f64;3] | | — | 位置 [x, y, z] |
| rotation | [f64;3] | | — | 旋转 [pitch, yaw, roll] |
| scale | [f64;3] | | — | 缩放 [x, y, z] |

### set_actor_property

设置 Actor 属性值。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| property_name | string | ✅ | — | 属性名 |
| value | any | ✅ | — | 属性值（JSON） |

### get_actor_property

获取 Actor 属性值。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| property_name | string | ✅ | — | 属性名 |

### find_actors_by_class

按类名搜索 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| class_name | string | ✅ | — | Actor 类名 |
| exact_match | bool | | false | 是否精确匹配 |

### spawn_blueprint_actor

从 Blueprint 资产生成 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| blueprint_path | string | ✅ | — | Blueprint 资产路径 |
| name | string | | — | 可选的 Actor 名称 |
| location | [f64;3] | | [0,0,0] | 位置 |
| rotation | [f64;3] | | [0,0,0] | 旋转 |

---

## 3. 编辑器操作 (15)

### get_editor_info

获取引擎版本、项目名称等编辑器信息。无参数。

### run_console_command

执行 Unreal 控制台命令。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| command | string | ✅ | — | 控制台命令，如 `stat fps` |

### save_current_level

保存当前关卡。无参数。

### create_level

创建新关卡并保存。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 新关卡资产路径，如 `/Game/Maps/NewMap` |

### play_in_editor

启动 Play In Editor (PIE)。无参数。

### stop_play_in_editor

停止 Play In Editor (PIE)。无参数。

### take_screenshot

截取编辑器视口截图。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| filename | string | | `screenshot` | 文件名（不含扩展名） |

### focus_viewport

聚焦视口到指定 Actor 或坐标。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | | — | 目标 Actor 名称（与 location 二选一） |
| location | [f64;3] | | — | 目标坐标 [x, y, z] |

### get_current_level

获取当前关卡名称、路径、Actor 数量。无参数。

### get_selected_actors

获取当前选中的 Actor 列表。无参数。

### select_actor

选中/取消选中指定 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| add_to_selection | bool | | false | 是否追加到当前选择 |

### get_ue_logs

获取 UE 编辑器输出日志。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| count | i32 | | 100 | 最大返回条数 (1-1000) |
| verbosity | string | | `Log` | 最低级别: Error/Warning/Log/Verbose/VeryVerbose |
| clear_after | bool | | false | 读取后清空缓冲区 |

### execute_editor_command

执行编辑器控制台命令（如 `undo`, `redo`, `newlevel`）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| command | string | ✅ | — | 命令名，自动尝试 `editor.<command>` 前缀 |

### focus_editor_panel

聚焦/切换到编辑器面板。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| panel | string | ✅ | — | ContentBrowser / WorldOutliner / Details / OutputLog / Layers / Viewport |

### get_editor_commands

列出控制台命令。用于发现可用命令。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| prefix | string | | `editor.` | 搜索前缀 |

---

## 4. 蓝图操作 (14)

### create_blueprint

创建 Blueprint 资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| name | string | ✅ | — | Blueprint 名称 |
| parent_class | string | | `AActor` | 父类名 |
| path | string | | `/Game` | 资产路径 |

### compile_blueprint

编译 Blueprint。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |

### get_blueprint_info

获取 Blueprint 信息与变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |

### add_blueprint_node

向 Blueprint 图形添加节点。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| node_type | string | ✅ | — | Event / CallFunction / CustomEvent / VariableGet / VariableSet / PrintString |
| name | string | | — | 函数/事件/变量名（依 node_type 而定） |
| class_name | string | | — | 目标类名 |
| graph_type | string | | `EventGraph` | 目标图：EventGraph 或函数图名 |
| pos_x | i32 | | 0 | 节点 X 坐标 |
| pos_y | i32 | | 0 | 节点 Y 坐标 |

### connect_blueprint_pins

连接两个节点之间的引脚。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| source_node_id | string | ✅ | — | 源节点 GUID |
| source_pin | string | ✅ | — | 源引脚名 |
| target_node_id | string | ✅ | — | 目标节点 GUID |
| target_pin | string | ✅ | — | 目标引脚名 |

### get_blueprint_graph

获取 Blueprint 图形结构。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| graph_type | string | | `EventGraph` | 目标图：EventGraph 或函数图名 |

### add_blueprint_variable

给 Blueprint 添加变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| variable_name | string | ✅ | — | 变量名 |
| variable_type | string | ✅ | — | int/float/bool/string/name/text/Vector/Rotator/Transform/Color/UObject类名 |
| is_array | bool | | false | 是否数组类型 |

### remove_blueprint_variable

从 Blueprint 删除变量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| variable_name | string | ✅ | — | 要删除的变量名 |

### create_blueprint_function_graph

在 Blueprint 中创建新函数图。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| function_name | string | ✅ | — | 函数名 |
| category | string | | — | 函数分类（可选） |

### list_blueprint_graphs

列出 Blueprint 中所有图（EventGraph, FunctionGraphs, Delegates）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |

### delete_blueprint_graph

删除 Blueprint 中的函数图（不能删除 EventGraph）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 资产路径 |
| graph_name | string | ✅ | — | 函数图名称 |

### get_level_blueprint

获取当前关卡蓝图的完整图形结构（节点、引脚、连接）。无需参数。

**返回**: 包含 `level_name`, `blueprint_path`, `graphs`（含 nodes 和 pins 的数组）。

### remove_blueprint_nodes

从 Blueprint 或关卡蓝图中删除指定节点。使用 `path="__level__"` 操作关卡蓝图。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | Blueprint 路径或 `__level__` |
| node_ids | [string] | ✅ | — | 要删除的节点 GUID 列表 |

**返回**: `removed_count`（删除数量）, `saved`（是否已保存）。

### save_level_blueprint

保存当前关卡蓝图（标记为已修改并保存关卡）。无需参数。

**返回**: `saved: true`。

---

## 5. 资产操作 (6)

### get_asset_list

列出 `/Game` 下的所有资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | | `/Game` | 搜索路径 |

### get_asset_info

获取资产详细信息。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 资产路径 |

### delete_asset

删除资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 资产路径 |

### rename_asset

重命名/移动资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 源路径 |
| new_name | string | ✅ | — | 新名称 |

### import_asset

导入外部文件到 Content Browser。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| file_path | string | ✅ | — | 源文件绝对路径 (FBX/PNG/WAV) |
| destination_path | string | | `/Game` | Content Browser 目标路径 |

### export_asset

导出资产到磁盘。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| asset_path | string | ✅ | — | 资产路径 |
| output_dir | string | | `Saved/Exports` | 输出目录 |

---

## 6. 组件操作 (3)

### get_actor_components

获取 Actor 上的所有组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |

### add_component

给 Actor 添加组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| component_class | string | ✅ | — | 组件类名 |
| component_name | string | | — | 可选组件名 |

### remove_component

从 Actor 移除组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| component_name | string | ✅ | — | 组件名 |

---

## 7. 材质操作 (4)

### set_material

应用材质到网格组件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| material_path | string | ✅ | — | 材质资产路径 |
| component_name | string | | — | 目标组件名（可选） |
| slot_index | i32 | | 0 | 材质槽索引 |

### create_material

创建新材质资产。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 材质资产路径 |
| shading_model | string | | `default_lit` | default_lit / unlit / subsurface / subsurface_profile / clear_coat / thin_translucent |
| blend_mode | string | | `Opaque` | Opaque / Masked / Translucent / Additive |
| base_color | [f64;3] | | — | 基础颜色 [r, g, b]（0-1） |
| metallic | f64 | | — | 金属度（0-1） |
| roughness | f64 | | — | 粗糙度（0-1） |
| specular | f64 | | — | 高光度（0-1） |
| reuse | bool | | false | 已存在时静默成功 |

### create_material_instance

创建材质实例 (MIC/MID)。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 新实例路径 |
| parent_path | string | ✅ | — | 父材质路径 |
| instance_type | string | | `MIC` | MIC / MID |

### set_material_parameter

设置材质实例参数。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| parameter_name | string | ✅ | — | 参数名 |
| scalar_value | f64 | | — | 标量值 |
| vector_value | [f64;3] | | — | 向量值 [r, g, b]（0-1） |
| component_name | string | | — | 目标组件名（可选） |
| slot_index | i32 | | 0 | 材质槽索引 |

---

## 8. 网格/光照/特效 (3)

### set_static_mesh

设置 StaticMeshComponent 的网格。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| mesh_path | string | ✅ | — | Static Mesh 资产路径 |
| component_name | string | | — | 目标组件名（可选） |

### set_light_parameters

设置光源参数。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| intensity | f64 | | — | 光照强度 |
| color | [f64;3] | | — | 颜色 [r, g, b]（0-1） |
| cast_shadows | bool | | — | 是否投射阴影 |

### spawn_effect

生成 Niagara/Cascade 粒子特效。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| asset_path | string | ✅ | — | 特效资产路径 |
| location | [f64;3] | | [0,0,0] | 生成位置 |
| rotation | [f64;3] | | [0,0,0] | 旋转 |
| auto_destroy | bool | | true | 是否自动销毁 |

---

## 9. 输入/相机 (3)

### simulate_key

模拟键盘按键。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| key | string | ✅ | — | 按键名，如 `SpaceBar`, `LeftMouseButton` |
| action | string | | `tap` | press / release / tap |

### get_viewport_camera

获取编辑器视口相机位置与旋转。无参数。

### set_viewport_camera

设置编辑器视口相机位置与旋转。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| location | [f64;3] | | — | 位置 [x, y, z] |
| rotation | [f64;3] | | — | 旋转 [pitch, yaw, roll] |

---

## 10. Runtime 相机 (9)

### get_runtime_camera_state

获取运行时游戏相机状态（位置、缩放、FOV、景深、后处理）。作用于 PlayerController 的 ViewTarget，或场景中第一个 `ACameraActor`。无参数。

### set_runtime_camera_fov

设置运行时相机 FOV。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| fov | f64 | ✅ | — | 视野角度（10-170 度） |

### set_runtime_camera_dof

设置运行时相机景深。**需要目标 Actor 带有 `UCineCameraComponent`**。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| focal_distance | f64 | ✅ | — | 焦距（cm） |

### set_runtime_camera_post_process

设置运行时相机后处理。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| exposure | f64 | | — | 曝光补偿（-10 到 +10） |
| bloom | f64 | | — | 泛光强度（0-10） |

### set_runtime_camera_transform

设置运行时相机位置与缩放。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| location | [f64;3] | | — | 位置 [x, y, z] |
| zoom | f64 | | — | 缩放（臂长） |

### focus_runtime_camera_on_actor

将运行时相机聚焦到指定 Actor。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | 目标 Actor 名称 |

### set_runtime_camera_focal_length

设置运行时相机焦距。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| focal_length | f64 | ✅ | — | 焦距（mm，1-1000） |

### set_runtime_camera_aperture

设置运行时相机光圈。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| aperture | f64 | ✅ | — | 光圈 f-stop（0.1-64） |

### set_runtime_camera_focus_distance

设置运行时相机对焦距离。**需要目标 Actor 带有 `UCineCameraComponent`**。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| focus_distance | f64 | ✅ | — | 对焦距离（cm） |

---

## 11. 相机轨道 (Camera Rig) (3)

### start_camera_rig

启动 `ACameraRig_Rail` 轨道播放。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| rig_name | string | ✅ | — | 场景中 `ACameraRig_Rail` 的名称 |

### stop_camera_rig

停止 `ACameraRig_Rail` 轨道播放。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| rig_name | string | ✅ | — | 场景中 `ACameraRig_Rail` 的名称 |

### set_camera_rig_speed

设置 `ACameraRig_Rail` 轨道播放速度。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| rig_name | string | ✅ | — | 场景中 `ACameraRig_Rail` 的名称 |
| speed | f64 | ✅ | — | 播放速度（cm/s） |

---

## 12. 相机切换 (Camera Switcher) (4)

### switch_camera

按名称切换到场景中的 `ACameraActor`（带混合过渡）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| camera_name | string | ✅ | — | `ACameraActor` 名称 |
| blend_time | f64 | | — | 混合过渡时间（秒） |

### next_camera

切换到下一个 `ACameraActor`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| blend_time | f64 | | — | 混合过渡时间（秒） |

### prev_camera

切换到上一个 `ACameraActor`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| blend_time | f64 | | — | 混合过渡时间（秒） |

### get_camera_list

获取场景中所有 `ACameraActor` 列表。无参数。

---

## 13. 高级后处理 (3)

### set_runtime_camera_motion_blur

设置动态模糊强度。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| amount | f64 | ✅ | — | 动态模糊强度（0-1） |

### set_runtime_camera_vignette

设置暗角强度。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| intensity | f64 | ✅ | — | 暗角强度（0-10） |

### set_runtime_camera_chromatic_aberration

设置色差强度。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| intensity | f64 | ✅ | — | 色差强度（0-10） |

---

## 14. 视口/调试 (3)

### set_view_mode

设置视口渲染模式。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| mode | string | ✅ | — | Lit / Unlit / Wireframe / ShaderComplexity / etc. |

### show_debug

切换调试可视化。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| flag | string | ✅ | — | collision / navigation / bounds / etc. |
| enable | bool | | — | 不填则切换 |

### add_actor_tag

给 Actor 添加标签。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| actor_name | string | ✅ | — | Actor 名称 |
| tag | string | ✅ | — | 标签文本 |

---

## 15. 关卡/代码 (2)

### open_level

打开关卡。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| path | string | ✅ | — | 关卡路径 |

### generate_cpp_class

生成 C++ 类模板。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| class_name | string | ✅ | — | 类名 |
| parent_class | string | ✅ | — | 父类，如 `AActor` |
| module | string | | 项目名 | 模块名 |
