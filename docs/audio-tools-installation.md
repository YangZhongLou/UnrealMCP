# AI 音频工具安装指南

本指南说明如何在本地安装 AI 音频生成工具，供 UnrealMCP 的 `audio-designer` Agent 使用。

覆盖三个模型：

- **ACE-Step**：BGM / 主题曲 / 完整歌曲生成
- **Stable Audio Open**：音效 / 环境声 / 短循环生成
- **MMAudio**：视频/录屏 Foley 配音

---

## 1. 通用前置依赖

### 1.1 必需软件

| 软件 | 用途 | 推荐版本 |
| --- | --- | --- |
| Python | 运行模型推理 | 3.10 |
| Git | 克隆仓库 | 最新 |
| Git LFS | 下载大文件权重 | 最新 |
| FFmpeg | 音频处理与解码 | 4.x / 5.x |
| CUDA Toolkit | NVIDIA GPU 加速 | 11.8 / 12.x |

### 1.2 检查命令

```powershell
python --version          # 应为 3.10.x
git --version
git lfs version
ffmpeg -version
nvidia-smi                # 确认 NVIDIA GPU 和驱动
```

> 如果没有 NVIDIA GPU，ACE-Step 支持 CPU/offload 模式；Stable Audio Open 和 MMAudio 也可用 CPU，但速度较慢。

---

## 2. Python 环境管理

### 2.1 一键安装（推荐）

项目提供 PowerShell 一键安装脚本，会自动完成：

- 检测 Python 3.10、Git、FFmpeg、NVIDIA GPU
- 创建 venv 或 conda 环境
- 安装 CUDA 12.6 版 PyTorch
- 克隆并安装 ACE-Step、Stable Audio Open、MMAudio
- 生成 `audio_server/.env`

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

脚本会自动把 PyTorch 固定为 CUDA 12.6 版本，避免 Stable Audio Open 将其降级为 CPU 版本。

### 2.2 手动环境

如果你想手动管理环境：

```powershell
# 创建基础环境
conda create -n ai-audio python=3.10 -y
conda activate ai-audio

# 安装 PyTorch（带 CUDA 12.6，与当前驱动兼容）
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu126
```

> 根据你的 NVIDIA 驱动支持的最高 CUDA 版本调整 index-url（560.94 驱动支持 CUDA 12.6）。

---

## 3. ACE-Step 安装

ACE-Step 用于生成 BGM、主题曲和带歌词的完整歌曲。

### 3.1 克隆与安装

```powershell
git clone https://github.com/ace-step/ACE-Step.git
cd ACE-Step

# 激活环境后安装
pip install -e .
```

### 3.2 验证安装

```powershell
# 启动 Gradio 界面
acestep --port 7865
```

浏览器打开 `http://127.0.0.1:7865`，第一次运行会自动下载模型权重到 `~/.cache/ace-step/checkpoints`。

### 3.3 硬件要求

| 配置 | 显存 | 体验 |
| --- | --- | --- |
| 最小 | 8GB | 开启 `--cpu_offload true` 可用 |
| 推荐 | 12GB+ | 正常速度生成 |
| 最佳 | RTX 4090 / A100 | 4 分钟音乐约 20 秒 |

### 3.4 推荐启动参数（显存有限）

```powershell
acestep --torch_compile true --cpu_offload true --overlapped_decode true --port 7865
```

---

## 4. Stable Audio Open 安装

Stable Audio Open 用于生成音效、环境声和短循环音频。

### 4.1 克隆与安装

```powershell
git clone https://github.com/Stability-AI/stable-audio-tools.git
cd stable-audio-tools

pip install -e ".[train,ui]"
```

### 4.2 安装 Flash Attention（可选，推荐）

```powershell
pip install flash-attn --no-build-isolation
```

> 如果编译失败，可以跳过；模型仍可用，但生成速度和显存占用会稍差。

### 4.3 登录 HuggingFace

Stable Audio Open 权重托管在 HuggingFace，需要登录并接受模型使用条款。

```powershell
pip install huggingface-hub
huggingface-cli login
```

然后在浏览器中访问 [stabilityai/stable-audio-open-1.0](https://huggingface.co/stabilityai/stable-audio-open-1.0) 并接受条款。

### 4.4 验证安装

```powershell
python run_gradio.py --pretrained-name stabilityai/stable-audio-open-1.0
```

浏览器打开 `http://127.0.0.1:7860` 测试生成。

### 4.5 硬件要求

| 配置 | 显存 | 说明 |
| --- | --- | --- |
| Small 模型 | 8GB | 可在消费级显卡运行 |
| Open 1.0 Medium | 16GB | fp16 推理 |

---

## 5. MMAudio 安装

MMAudio 用于为视频或游戏录屏生成同步音效（Foley）。

### 5.1 克隆与安装

```powershell
git clone https://github.com/hkchengrex/MMAudio.git
cd MMAudio

pip install -r requirements.txt
```

### 5.2 下载模型权重

首次运行 demo 时会自动下载权重。也可以手动下载到项目目录：

```powershell
python demo.py --prompt "coffee shop ambiance" --duration 8
```

### 5.3 硬件要求

| 配置 | 显存 | 说明 |
| --- | --- | --- |
| 最小 | 8GB | 可用低显存模式 |
| 推荐 | 12GB+ | 正常生成速度 |

---

## 6. 与 UnrealMCP 集成

三个模型安装完成后，可以通过以下方式与 UE 编辑器交互：

### 6.1 编辑器预生成工作流

```text
1. 在 Gradio/命令行中生成音频
2. 导出 WAV 文件
3. 使用 UnrealMCP import_asset 导入 /Game/Audio/Generated
4. 创建 Sound Cue / MetaSound Source
5. 在 Blueprint 中触发播放
```

### 6.2 运行时生成工作流（可选）

项目已提供现成封装：

- **FastAPI 服务**：`Plugins/UnrealMCP/audio_server/main.py`
- **一键安装脚本**：`Plugins/UnrealMCP/scripts/install-audio-tools.ps1`

运行流程：

1. 执行 `install-audio-tools.ps1` 安装三个模型并生成 `audio_server/.env`；
2. 启动 `audio_server/main.py`；
3. UE 通过 HTTP 调用 `/generate/music`、`/generate/sfx` 或 `/generate/foley`；
4. 返回 `audio_base64`，用 **Runtime Audio Importer** 插件转成 `USoundWave` 播放。

> Runtime Audio Importer 开源版已归档，建议从 Fab 商城购买最新版。

---

## 7. 快速验证清单

安装完成后，依次验证：

- [ ] `python --version` 为 3.10.x
- [ ] `nvidia-smi` 正常显示 GPU
- [ ] ACE-Step Gradio 能生成 30 秒音乐
- [ ] Stable Audio Open Gradio 能生成 5 秒音效
- [ ] MMAudio demo 能根据文本生成 8 秒音频
- [ ] UnrealMCP `import_asset` 能把 WAV 导入 UE Content Browser

---

## 8. 常见问题

| 问题 | 可能原因 | 解决办法 |
| --- | --- | --- |
| `CUDA out of memory` | 显存不足 | 开启 `--cpu_offload`、降低时长、使用量化模型 |
| `flash-attn` 编译失败 | CUDA 版本不匹配 | 跳过 Flash Attention，或安装对应 CUDA 版本的 PyTorch |
| HuggingFace 下载失败 | 未登录或未接受条款 | 运行 `huggingface-cli login`，在模型页面点击 Accept |
| HuggingFace 下载慢/超时 | 网络到 `huggingface.co` 不稳定 | 改用 ModelScope 预下载，或参考下方「慢网络 / 离线环境下载 weights」 |
| 离线环境无法下载 | 目标机器无稳定外网 | 在另一台机器预下载，运行 `Plugins\UnrealMCP\scripts\import-audio-weights.ps1` 导入 |
| PyTorch 变成 CPU 版本 | 模型包把 torch 降级 | 重新安装 CUDA 版：`pip install --force-reinstall torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu126` |
| ACE-Step 首次启动慢 | 自动下载权重 | 等待完成，或手动下载放到 `~/.cache/ace-step/checkpoints` |
| MMAudio 提示缺少权重 | 模型未自动下载 | 检查网络，手动下载后放入项目 checkpoints 目录 |
| UE 导入 WAV 失败 | 格式不支持 | 确保是 16/24bit WAV，采样率 44.1kHz 或 48kHz |

---

## 9. 慢网络 / 离线环境下载 weights

本文档聚焦安装步骤；完整的 repo ID、SHA-256/MD5 校验值、镜像链接与推荐工作流详见：

- [`Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md`](./../audio_server/MODEL_DOWNLOAD_GUIDE.md)

下面给出在 HuggingFace 连接不稳或完全离线时的实用下载方案。

### 9.1 设置 HF_ENDPOINT（效果有限）

可以临时把 HuggingFace Hub 的 endpoint 指向镜像：

```powershell
$env:HF_ENDPOINT = "https://hf-mirror.com"
```

但在 ACE-Step / Stable Audio Open / MMAudio 这几个仓库上，`hf-mirror.com` 往往会返回 `308` 跳转回 `huggingface.co`，下载仍走原站，因此**仅作为首次尝试**。如果仍然失败，请改用 ModelScope 或预下载方案。

> 同时可安装 `hf_xet` 并增大超时：  
> `pip install hf_xet`  
> `$env:HF_HUB_DOWNLOAD_TIMEOUT = "300"`

### 9.2 使用 ModelScope 镜像下载

ModelScope（modelscope.cn）是当前验证可用的公共镜像，对三个模型都有完整副本。

安装 SDK：

```powershell
pip install modelscope
```

下载命令示例（在稳定的机器上执行）：

```powershell
# ACE-Step v1-3.5B（约 8.3 GB）
modelscope download --local-dir ./audio_weights/ACE-Step-v1-3.5B ACE-Step/ACE-Step-v1-3.5B

# Stable Audio Open 1.0（约 10.8 GB，需先在 HF 页面接受使用条款）
# https://huggingface.co/stabilityai/stable-audio-open-1.0
modelscope download --local-dir ./audio_weights/stable-audio-open-1.0 stabilityai/stable-audio-open-1.0

# MMAudio large_44k_v2 完整包（含 BigVGAN / CLIP，约 7.5 GB）
modelscope download --local-dir ./audio_weights/MMAudio PineKing2024/MMAudio
```

### 9.3 使用 huggingface-cli 断点续传

如果 HF 连接尚可但容易中断，可用官方 CLI 的 resume 功能配合较长超时：

```powershell
$env:HF_HUB_DOWNLOAD_TIMEOUT = "300"
$env:HF_HUB_ENABLE_HF_TRANSFER = "1"   # 可选加速，需先 pip install hf-transfer

huggingface-cli download ACE-Step/ACE-Step-v1-3.5B --local-dir ./audio_weights/ACE-Step-v1-3.5B --resume-download
huggingface-cli download stabilityai/stable-audio-open-1.0 --local-dir ./audio_weights/stable-audio-open-1.0 --resume-download
huggingface-cli download hkchengrex/MMAudio --local-dir ./audio_weights/MMAudio --resume-download
huggingface-cli download nvidia/bigvgan_v2_44khz_128band_512x --local-dir ./audio_weights/bigvgan_v2_44khz_128band_512x --resume-download
```

### 9.4 推荐离线工作流：预下载 + 校验 + 导入

最可靠的方式是在另一台网络稳定的机器上提前下载，再搬到目标环境：

1. **预下载**：使用 ModelScope 或 `huggingface-cli --resume-download` 把三个仓库下载到同一目录，例如 `D:\AudioWeights\`。
2. **校验**：对照 `MODEL_DOWNLOAD_GUIDE.md` 中的 SHA-256 / MD5 表检查关键文件。MMAudio 的 ModelScope 包自带 `checkfile.json` 可用。
3. **导入**：在目标机器上运行项目提供的导入脚本：

   ```powershell
   powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\import-audio-weights.ps1 -SourceDir D:\AudioWeights -UpdateEnv
   ```

   该脚本会把 weights 复制/链接到 `Plugins/UnrealMCP/third_party/` 与 `audio_server/` 期望的位置，并可选更新 `audio_server/.env` 中的 `ACE_STEP_CHECKPOINT_PATH`。

### 9.5 指向本地 checkpoint 的 config.yaml 示例

编辑 `Plugins/UnrealMCP/audio_server/config.yaml`，用绝对路径替换 HF repo ID：

```yaml
models:
  ace_step:
    checkpoint_path: "D:/AudioWeights/ACE-Step-v1-3.5B"
  stable_audio_open:
    pretrained_name: "D:/AudioWeights/stable-audio-open-1.0"
  mmaudio:
    model_name: "large_44k_v2"
```

> MMAudio 的 `weights/` 与 `ext_weights/` 会优先从当前工作目录或 `third_party/MMAudio/` 下查找。如果 `main.py` 启动后仍提示缺少权重，把 MMAudio 相关文件放到 `Plugins/UnrealMCP/third_party/MMAudio/weights/` 与 `ext_weights/` 下，具体布局见 `MODEL_DOWNLOAD_GUIDE.md`。

---

## 10. 相关文件

- Agent 定义：`SkillHub/.claude/agents/audio-designer.md`
- 调用命令：`SkillHub/.claude/commands/audio-designer.md`
- 工作流说明：`README.md`
