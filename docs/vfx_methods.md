# UnrealMCP VFX 扩展接口协议

本文档定义 `TA-Playground` 项目 **UE + AI 自动生成特效资源工作流** 在 UnrealMCP 中新增的 5 个 method 的 JSON 请求/响应 schema，以及配套的异步状态查询方法。

目标：让后续 C++ 命令实现（`Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/VfxCommands.cpp`）与 Rust MCP Server tool 实现（`Plugins/UnrealMCP/MCP_Server/src/tools/`）保持字段名、类型、返回值结构一致。

---

## 1. 通用约定

### 1.1 JSON-RPC 信封

所有请求沿用 UnrealMCP 现有格式：

```json
{
  "method": "<method_name>",
  "params": { ... },
  "id": "<request_id>"
}
```

响应：

```json
// 成功
{
  "id": "<request_id>",
  "success": true,
  "result": { ... }
}

// 失败
{
  "id": "<request_id>",
  "success": false,
  "error": "human-readable error message"
}
```

### 1.2 字段命名

- JSON 请求/响应字段使用 `camelCase`（与现有 UnrealMCP 命令保持一致）。
- UE 资产路径使用 `/Game/...` 风格，例如 `/Game/Generated/Meshes/SM_Rock`。
- 绝对磁盘路径使用 Windows 风格或跨平台风格均可，C++ 侧用 `FPaths` 统一处理。

### 1.3 常用类型

| 类型 | JSON 表示 | 说明 |
| --- | --- | --- |
| `Vector3` | `[f64; 3]` | `[x, y, z]`，用于 location/rotation/scale/color |
| `Vector4` | `[f64; 4]` | `[x, y, z, w]`，用于 color with alpha |
| `AssetPath` | `string` | UE 内容浏览器路径，如 `/Game/VFX/Templates/NS_BurstBase` |
| `FilePath` | `string` | 磁盘绝对路径，如 `D:/.../mesh.glb` |

### 1.4 材质母材参数命名

与 `Content/Materials/Generated/` 母材约定对齐：

- `M_Generated_Opaque`: `BaseColor`, `Normal`, `Roughness`, `Metallic`
- `M_Generated_Translucent`: `BaseColor`, `Opacity`, `Emissive`, `Normal`
- `M_Generated_Unlit_Additive`: `BaseColor`, `Opacity`, `EmissiveColor`
- `M_Generated_Masked`: `BaseColor`, `OpacityMask`, `Normal`

### 1.5 Niagara User Parameter 命名

与 `Content/VFX/Templates/` 模板约定对齐：

- `NS_BurstBase`: `User.Color`, `User.Size`, `User.Rate`, `User.Lifetime`, `User.Velocity`
- `NS_TrailBase`: `User.Color`, `User.Width`, `User.Lifetime`, `User.Speed`
- `NS_AmbientBase`: `User.Color`, `User.Density`, `User.Size`, `User.Speed`
- `NS_ImpactBase`: `User.Color`, `User.Size`, `User.DecalSize`, `User.Lifetime`

---

## 2. 接口详情

### 2.1 `generate_and_import_3d`

**用途**：将外部 3D 文件导入 UE，或在本地调用 Hunyuan3D-2.1 服务生成模型后再导入。生成阶段走子进程异步，不阻塞 Editor。

#### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `meshFile` | `FilePath` | 条件 | — | 已生成的 GLB/OBJ/FBX 文件绝对路径。与 `prompt`/`referenceImage` 二选一。 |
| `prompt` | `string` | 条件 | — | 文本提示，用于驱动 Hunyuan3D 生成。 |
| `referenceImage` | `FilePath` | 条件 | — | 参考图绝对路径，与 `prompt` 同时提供时优先按图生成。 |
| `destinationPath` | `AssetPath` | ✅ | — | 导入目标路径，如 `/Game/Generated/Meshes`。 |
| `actorName` | `string` | | 自动生成 | 生成后摆放的 StaticMeshActor 名称。 |
| `location` | `Vector3` | | `[0,0,0]` | Actor 位置。 |
| `rotation` | `Vector3` | | `[0,0,0]` | Actor 旋转 `[pitch, yaw, roll]`。 |
| `scale` | `Vector3` | | `[1,1,1]` | Actor 缩放。 |
| `generationParams` | `object` | | `{}` | 见下方生成参数。 |
| `waitForCompletion` | `bool` | | `false` | 是否阻塞等待生成完成。仅对生成模式有效；直接导入始终同步完成。 |

`generationParams` 子字段：

| 参数 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `steps` | `int` | `30` | 推理步数。 |
| `seed` | `int` | 随机 | 随机种子。 |
| `guidanceScale` | `f64` | `7.5` | 文生图/图生 3D 引导强度。 |
| `turbo` | `bool` | `false` | 是否使用 turbo 低显存模式。 |
| `outputDir` | `FilePath` | 项目 `hunyuan/output/<uuid>` | Hunyuan3D 输出目录。 |

#### 响应

**直接导入成功 / 生成同步完成**：

```json
{
  "id": "req-1",
  "success": true,
  "result": {
    "assetPath": "/Game/Generated/Meshes/SM_Rock.SM_Rock",
    "actorName": "AIGenerated_Rock_01",
    "meshFile": "D:/Playground/TA-Playground/hunyuan/output/xxx/mesh.glb",
    "location": [0, 0, 0],
    "rotation": [0, 0, 0],
    "scale": [1, 1, 1],
    "status": "completed"
  }
}
```

**异步生成已启动**（`waitForCompletion: false` 且需要生成）：

```json
{
  "id": "req-1",
  "success": true,
  "result": {
    "jobId": "gen-3d-550e8400-e29b-41d4-a716-446655440000",
    "status": "pending",
    "message": "Hunyuan3D generation started, use get_generate_and_import_3d_status to poll."
  }
}
```

#### Rust tool 签名

```rust
#[tool(description = "Generate a 3D model via Hunyuan3D or import an existing mesh file, then spawn it in the scene.")]
async fn generate_and_import_3d(
    &self,
    #[tool(param)] destination_path: String,
    #[tool(param)] mesh_file: Option<String>,
    #[tool(param)] prompt: Option<String>,
    #[tool(param)] reference_image: Option<String>,
    #[tool(param)] actor_name: Option<String>,
    #[tool(param)] location: Option<Vec<f64>>,
    #[tool(param)] rotation: Option<Vec<f64>>,
    #[tool(param)] scale: Option<Vec<f64>>,
    #[tool(param)] generation_params: Option<Value>,
    #[tool(param)] wait_for_completion: Option<bool>,
) -> String
```

#### 实现说明

- C++ 侧优先检查 `meshFile`：存在则直接走 `ImportAssets` 导入并生成 Actor。
- 若需提供生成，C++ 启动 `hunyuan/vfx_generation_service.py` 子进程，传入参数化命令行，立即返回 `jobId`。
- 生成产物约定为 `outputDir/mesh.glb` 与 `outputDir/mesh.png`（如存在）。
- 失败时返回 `success: false`，`error` 包含子进程 stderr 摘要。

---

### 2.2 `create_material_from_textures`

**用途**：根据贴图集合创建 Material Instance Constant（MIC），并自动连接母材的 Texture Parameter。

#### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `AssetPath` | ✅ | — | 新 MIC 路径，如 `/Game/Generated/Materials/MI_Rock`。 |
| `parentPath` | `AssetPath` | ✅ | — | 母材路径，如 `/Game/Materials/Generated/M_Generated_Opaque`。 |
| `maps` | `object` | ✅ | — | 参数名 → 贴图资产路径，见示例。 |
| `scalarParameters` | `object` | | `{}` | 标量参数覆盖，如 `{"Roughness": 0.8}`。 |
| `vectorParameters` | `object` | | `{}` | 向量/颜色参数覆盖，如 `{"EmissiveColor": [1,0.5,0]}`。 |
| `reuse` | `bool` | | `true` | 路径已存在时是否直接复用并重新设置参数。 |

`maps` 示例：

```json
{
  "BaseColor": "/Game/Generated/Textures/T_Rock_Diffuse",
  "Normal": "/Game/Generated/Textures/T_Rock_Normal",
  "Roughness": "/Game/Generated/Textures/T_Rock_Roughness",
  "Metallic": "/Game/Generated/Textures/T_Rock_Metallic"
}
```

#### 响应

```json
{
  "id": "req-2",
  "success": true,
  "result": {
    "path": "/Game/Generated/Materials/MI_Rock.MI_Rock",
    "parentPath": "/Game/Materials/Generated/M_Generated_Opaque",
    "maps": {
      "BaseColor": "/Game/Generated/Textures/T_Rock_Diffuse",
      "Normal": "/Game/Generated/Textures/T_Rock_Normal"
    },
    "reused": false
  }
}
```

#### Rust tool 签名

```rust
#[tool(description = "Create a Material Instance Constant from a parent material and texture maps.")]
async fn create_material_from_textures(
    &self,
    #[tool(param)] path: String,
    #[tool(param)] parent_path: String,
    #[tool(param)] maps: Value,
    #[tool(param)] scalar_parameters: Option<Value>,
    #[tool(param)] vector_parameters: Option<Value>,
    #[tool(param)] reuse: Option<bool>,
) -> String
```

#### 实现说明

- C++ 侧可委托给 `Content/Python/import_generated_asset.py` 中的 Python 函数执行复杂材质装配，C++ 仅负责触发与错误返回（参见计划第 5 节“决策 2”）。
- 贴图参数不存在于母材时，返回 `error` 但不中断其他有效参数的写入。

---

### 2.3 `set_texture_parameter`

**用途**：在运行时/编辑器中修改 Actor 材质实例的 Texture 类型参数。

#### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `actorName` | `string` | ✅ | — | 场景中的 Actor 名称。 |
| `parameterName` | `string` | ✅ | — | Texture 参数名，如 `BaseColor`。 |
| `texturePath` | `AssetPath` | ✅ | — | 贴图资产路径。 |
| `componentName` | `string` | | 首个 Mesh 组件 | 目标组件名。 |
| `slotIndex` | `int` | | `0` | 材质槽索引。 |

#### 响应

```json
{
  "id": "req-3",
  "success": true,
  "result": {
    "actorName": "AIGenerated_Rock_01",
    "componentName": "StaticMeshComponent0",
    "slotIndex": 0,
    "parameterName": "BaseColor",
    "texturePath": "/Game/Generated/Textures/T_Rock_Diffuse"
  }
}
```

#### Rust tool 签名

```rust
#[tool(description = "Set a texture parameter on an actor's material instance.")]
async fn set_texture_parameter(
    &self,
    #[tool(param)] actor_name: String,
    #[tool(param)] parameter_name: String,
    #[tool(param)] texture_path: String,
    #[tool(param)] component_name: Option<String>,
    #[tool(param)] slot_index: Option<i32>,
) -> String
```

---

### 2.4 `duplicate_niagara_system`

**用途**：基于 Niagara 模板复制出一个新的 Niagara System 资产，可选择是否在场景中生成 Actor。

#### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `templatePath` | `AssetPath` | ✅ | — | 源 Niagara System 路径，如 `/Game/VFX/Templates/NS_BurstBase`。 |
| `newPath` | `AssetPath` | ✅ | — | 新系统路径，如 `/Game/VFX/Instances/NS_FireBurst`。 |
| `initialParameters` | `object` | | `{}` | 复制后立即写入的 User 参数，如 `{"User.Color": [1,0.2,0]}`。 |
| `spawnActor` | `bool` | | `false` | 是否在场景中生成一个 Niagara System 实例 Actor。 |
| `actorName` | `string` | | 自动生成 | 生成 Actor 的名称（`spawnActor` 为 true 时有效）。 |
| `location` | `Vector3` | | `[0,0,0]` | Actor 位置。 |
| `rotation` | `Vector3` | | `[0,0,0]` | Actor 旋转。 |

#### 响应

```json
{
  "id": "req-4",
  "success": true,
  "result": {
    "templatePath": "/Game/VFX/Templates/NS_BurstBase",
    "newPath": "/Game/VFX/Instances/NS_FireBurst.NS_FireBurst",
    "actorName": "NS_FireBurst_Actor_01",
    "location": [0, 0, 0],
    "parametersSet": ["User.Color"]
  }
}
```

#### Rust tool 签名

```rust
#[tool(description = "Duplicate a Niagara System template and optionally spawn it in the scene.")]
async fn duplicate_niagara_system(
    &self,
    #[tool(param)] template_path: String,
    #[tool(param)] new_path: String,
    #[tool(param)] initial_parameters: Option<Value>,
    #[tool(param)] spawn_actor: Option<bool>,
    #[tool(param)] actor_name: Option<String>,
    #[tool(param)] location: Option<Vec<f64>>,
    #[tool(param)] rotation: Option<Vec<f64>>,
) -> String
```

---

### 2.5 `set_niagara_parameter`

**用途**：修改场景中 Niagara Actor 或 Niagara System 资产上的 User Parameter。

#### 请求参数

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `actorName` | `string` | 条件 | — | 场景中 Niagara Actor 名称。与 `systemPath` 二选一。 |
| `systemPath` | `AssetPath` | 条件 | — | Niagara System 资产路径，直接修改资产默认参数。 |
| `parameterName` | `string` | ✅ | — | User 参数全名，如 `User.Color`。 |
| `value` | `scalar` / `Vector3` / `Vector4` / `bool` | ✅ | — | 参数值。 |
| `componentName` | `string` | | 首个 Niagara 组件 | `actorName` 模式下有效。 |

`value` 类型说明：

| UE 类型 | JSON 示例 |
| --- | --- |
| Float | `1.5` |
| Int | `5`（Rust 侧作为 `Value` 透传） |
| Bool | `true` |
| Color/Vector3 | `[1.0, 0.2, 0.0]` |
| Vector4/LinearColor with Alpha | `[1.0, 0.2, 0.0, 1.0]` |

#### 响应

```json
{
  "id": "req-5",
  "success": true,
  "result": {
    "actorName": "NS_FireBurst_Actor_01",
    "parameterName": "User.Color",
    "valueType": "Vector3",
    "value": [1.0, 0.2, 0.0]
  }
}
```

#### Rust tool 签名

```rust
#[tool(description = "Set a Niagara User Parameter on a spawned Niagara actor or a Niagara System asset.")]
async fn set_niagara_parameter(
    &self,
    #[tool(param)] parameter_name: String,
    #[tool(param)] value: Value,
    #[tool(param)] actor_name: Option<String>,
    #[tool(param)] system_path: Option<String>,
    #[tool(param)] component_name: Option<String>,
) -> String
```

#### 实现说明

- C++ 侧至少支持 `Float`、`Int`、`Bool`、`FLinearColor`（Vector4）四种类型；根据 `value` 的 JSON 类型自动推断。
- 同时提供 `actorName` 与 `systemPath` 时，优先以 `actorName` 为准。
- 修改资产默认参数后应自动保存包。

---

## 3. 配套方法：`get_generate_and_import_3d_status`

由于 `generate_and_import_3d` 在生成模式下为异步，提供状态查询方法。

### 3.1 请求参数

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `jobId` | `string` | ✅ | `generate_and_import_3d` 返回的 job id。 |

### 3.2 响应

```json
{
  "id": "req-6",
  "success": true,
  "result": {
    "jobId": "gen-3d-550e8400-e29b-41d4-a716-446655440000",
    "status": "running",
    "progress": 0.45,
    "message": "Generating mesh...",
    "assetPath": null,
    "actorName": null
  }
}
```

`status` 取值：`pending` | `running` | `completed` | `failed`。

完成时：

```json
{
  "id": "req-6",
  "success": true,
  "result": {
    "jobId": "gen-3d-...",
    "status": "completed",
    "progress": 1.0,
    "assetPath": "/Game/Generated/Meshes/SM_Rock.SM_Rock",
    "actorName": "AIGenerated_Rock_01"
  }
}
```

---

## 4. 错误码与返回规范

C++ 与 Rust 侧统一以下失败模式：

| 场景 | `success` | `error` 示例 |
| --- | --- | --- |
| 必填参数缺失 | `false` | `Missing required parameter: destinationPath` |
| 资产不存在 | `false` | `Asset not found: /Game/...` |
| 生成子进程失败 | `false` | `Failed to start generation subprocess: ...` |
| 生成任务超时/失败 | `false` | `Generation job failed: ...` |
| 材质参数不存在 | `false` | `Parameter BaseColor not found on parent material` |
| 未知 method | `false` | `Unknown method: ...` |

---

## 5. C++ / Rust 实现对齐检查表

| 检查项 | C++ (`VfxCommands.cpp`) | Rust (`tools/vfx_tools.rs`) |
| --- | --- | --- |
| 方法名 snake_case 与本文档一致 | ✅ 待实现 | ✅ 待实现 |
| JSON 参数字段 camelCase | ✅ 待实现 | ✅ 待实现 |
| `generate_and_import_3d` 异步子进程 | ✅ 待实现 | 透传即可 |
| `create_material_from_textures` 优先调用 Python | ✅ 待实现 | 透传即可 |
| `set_texture_parameter` 自动创建 MID | ✅ 待实现 | 透传即可 |
| `duplicate_niagara_system` 复制 + 可选 spawn | ✅ 待实现 | 透传即可 |
| `set_niagara_parameter` 支持 Float/Int/Bool/Vector | ✅ 待实现 | 透传即可 |
| 统一错误返回格式 | ✅ 待实现 | ✅ 待实现 |

---

## 6. 变更日志

- 2026-07-03：初版协议定义，覆盖 Phase 2.1 所需的 5 个 VFX method 与异步状态查询。
