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

建议使用 `conda` 或 `venv` 为每个模型创建独立环境，避免依赖冲突。

```powershell
# 创建基础环境
conda create -n ai-audio python=3.10 -y
conda activate ai-audio

# 安装 PyTorch（带 CUDA）
conda install pytorch torchvision torchaudio pytorch-cuda=12.1 -c pytorch -c nvidia -y
```

> 根据你的 CUDA 版本调整 `pytorch-cuda=12.1` 为实际版本号。

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
| ACE-Step 首次启动慢 | 自动下载权重 | 等待完成，或手动下载放到 `~/.cache/ace-step/checkpoints` |
| MMAudio 提示缺少权重 | 模型未自动下载 | 检查网络，手动下载后放入项目 checkpoints 目录 |
| UE 导入 WAV 失败 | 格式不支持 | 确保是 16/24bit WAV，采样率 44.1kHz 或 48kHz |

---

## 9. 相关文件

- Agent 定义：`SkillHub/.claude/agents/audio-designer.md`
- 调用命令：`SkillHub/.claude/commands/audio-designer.md`
- 工作流说明：`README.md`
