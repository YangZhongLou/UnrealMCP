# Audio Server Model Weight Download Guide

This guide lists reliable ways to obtain the weights for the three models used by `Plugins/UnrealMCP/audio_server/main.py`:

- **ACE-Step** — music / BGM (`/generate/music`)
- **Stable Audio Open** — SFX / ambience (`/generate/sfx`)
- **MMAudio** — Foley / video-to-audio (`/generate/foley`)

> Scope: research only. No large files were downloaded during the writing of this guide.

## TL;DR

- The `HF_ENDPOINT=https://hf-mirror.com` setting currently redirects most requests back to `huggingface.co` and does **not** cache these specific repos.
- **ModelScope (modelscope.cn)** is the fastest verified public mirror for all three models from mainland China / unstable HF networks.
- For Stable Audio Open, the HF repo is **gated**: you must accept the license on HuggingFace before downloading, even from a mirror.
- Recommended workflow: pre-download on a machine with a stable HF/ModelScope connection, verify checksums, then copy the folders into the target environment and point `config.yaml`/`.env` at them.

---

## 1. ACE-Step (music)

### What the project expects

The installer clones the original [`ace-step/ACE-Step`](https://github.com/ace-step/ACE-Step) repository (not ACE-Step 1.5). When `checkpoint_path` is left empty, the pipeline downloads the original v1-3.5B checkpoint to the default cache (`~/.cache/ace-step/checkpoints`).

### Official HuggingFace repo

| Repo ID | License | Gated |
|---|---|---|
| [`ACE-Step/ACE-Step-v1-3.5B`](https://huggingface.co/ACE-Step/ACE-Step-v1-3.5B) | Apache-2.0 | No |

### Required files (repo total ≈ 8.28 GB)

| File | Size | SHA-256 (LFS oid) | Purpose |
|---|---|---|---|
| `ace_step_transformer/diffusion_pytorch_model.safetensors` | 6.61 GB | `e810f16728d8a2e0d1b9c3a907aac8c9a427ce38edbd890cb3dce5ff92da5aad` | Main 3.5B DiT transformer |
| `music_dcae_f8c8/diffusion_pytorch_model.safetensors` | 313 MB | `2b0cb469307ac50659d1880db2a99bae47d0df335cbb36853964662d4b80e8ee` | Deep Compression Audio Encoder (DCAE) |
| `music_vocoder/diffusion_pytorch_model.safetensors` | 206 MB | `c92c9b46e28ab7b37b777780cf4308ad7ddac869636bb77aa61599358c4bc1c0` | Vocoder |
| `umt5-base/model.safetensors` | 1.13 GB | `779cec0d210b2123e21d0a9cd8128f02b4d412627355028965a8be0b241cc3b6` | Text encoder (UMT5-base) |
| `config.json`, `configuration.json` | < 1 KB each | — | Loader config |

### Verified mirrors

| Mirror | Repo ID / URL | Notes |
|---|---|---|
| **ModelScope** | [`ACE-Step/ACE-Step-v1-3.5B`](https://www.modelscope.cn/models/ACE-Step/ACE-Step-v1-3.5B) | ✅ Full mirror, same SHA-256 as HF |
| OpenCSG | [`AIWizards/ACE-Step-v1-3.5B`](https://opencsg.com/models/AIWizards/ACE-Step-v1-3.5B) | Listed, not byte-verified here |
| RunningHub | [model page](https://www.runninghub.cn/model/public/1922757687526789121) | Single `.safetensors` repack, not the original layout |

> ACE-Step 1.5 (`ACE-Step/Ace-Step1.5`, ~35 GB default) is a separate product. This project installs the original repo, so use `ACE-Step-v1-3.5B` unless you intentionally migrate to 1.5.

---

## 2. Stable Audio Open (SFX / ambience)

### What the project expects

`main.py` calls `stable_audio_tools.get_pretrained_model("stabilityai/stable-audio-open-1.0")`. The repo is gated, so you must accept the license on HuggingFace before any download (HF CLI, mirror, or otherwise).

### Official HuggingFace repo

| Repo ID | License | Gated |
|---|---|---|
| [`stabilityai/stable-audio-open-1.0`](https://huggingface.co/stabilityai/stable-audio-open-1.0) | Stable Audio Community License | **Yes** |

### Required files (repo total ≈ 10.8 GB)

| File | Size | SHA-256 (LFS oid) | Purpose |
|---|---|---|---|
| `model.ckpt` | 4.85 GB | `6049ae92ec8362804cb4cb8a2845be93071439da2daff9997c285f8119d7ea40` | Full training checkpoint (used by `stable-audio-tools`) |
| `model.safetensors` | 4.85 GB | `7b20458a071231aaf32613b6fbc7945f28f34dbba4f295bb49bad56f5f66b57e` | Diffusers-compatible full weights |
| `transformer/diffusion_pytorch_model.safetensors` | 3.94 GB | `65ae9715febfeb2dc9f33aa506c2bda69fd2ddf8ebc483fb2df090df4ca506fd` | DiT transformer component |
| `vae_model.ckpt` | 624 MB | `771265f2e9a7fa9c3b7899be2d6a5a93954032e5e696edd62239ba7f1cd67116` | Audio VAE checkpoint |
| `vae/diffusion_pytorch_model.safetensors` | 624 MB | `2131cdb52020b2473707465449d8bdb4f6cca61c93150a947baca02bc58ffd7b` | VAE component (Diffusers layout) |
| `text_encoder/model.safetensors` | 439 MB | `095878c81d21f17d235e1b4aa20e9595289177e2658ef4dcec69fa5aa3b07ca2` | T5 text encoder |
| `projection_model/diffusion_pytorch_model.safetensors` | 1.59 MB | `032102e2b0e4b9b9e19c648dc476345157b97906752a7b4b7a0133900f340926` | Projection model |
| `tokenizer/spiece.model` | 792 KB | `d60acb128cf7b7f2536e8f38a5b18a05535c9e14c7a355904270e15b0945ea86` | SentencePiece tokenizer |
| `model_config.json`, `vae_model_config.json`, `tokenizer/*.json`, `scheduler/*.json` | < 50 KB each | — | Config files |

### Verified mirrors

| Mirror | Repo ID / URL | Notes |
|---|---|---|
| **ModelScope** | [`stabilityai/stable-audio-open-1.0`](https://modelscope.cn/models/stabilityai/stable-audio-open-1.0) | ✅ Full mirror, same files/SHA-256; still requires HF license acceptance for the upstream repo |

> Gitee / GitCode HuggingFace mirrors were checked; the specific Stable Audio Open repo either was not present or had an empty file tree at the time of writing.

---

## 3. MMAudio (Foley / video-to-audio)

### What the project expects

`main.py` loads `model_name: large_44k_v2`. MMAudio needs:

- a flow-prediction network (`weights/mmaudio_large_44k_v2.pth`)
- a 44.1 kHz VAE (`ext_weights/v1-44.pth`)
- a Synchformer visual encoder (`ext_weights/synchformer_state_dict.pth`)
- a BigVGAN v2 44 kHz vocoder (`nvidia/bigvgan_v2_44khz_128band_512x`), downloaded automatically by the package
- CLIP (`apple/DFN5B-CLIP-ViT-H-14-384` or the local variant), downloaded automatically by the package

> **Note:** Even if you manually place the three MMAudio files above, the package will still auto-download **BigVGAN** and **CLIP** from HuggingFace on first load. These are large (~489 MB and ~3.9 GB respectively) and can be slow or hang on unstable networks. Pre-download the bundled ModelScope repo below to avoid this.

### Official HuggingFace / GitHub sources

| Repo / Release | License | Gated |
|---|---|---|
| [`hkchengrex/MMAudio`](https://huggingface.co/hkchengrex/MMAudio) | CC BY-NC 4.0 | No |
| [`nvidia/bigvgan_v2_44khz_128band_512x`](https://huggingface.co/nvidia/bigvgan_v2_44khz_128band_512x) | MIT | No |
| MMAudio ext_weights on [GitHub Releases v0.1](https://github.com/hkchengrex/MMAudio/releases/tag/v0.1) | — | No |

### Required MMAudio files for `large_44k_v2`

| File | Size | MD5 (official) | SHA-256 (LFS oid) | Purpose |
|---|---|---|---|---|
| `weights/mmaudio_large_44k_v2.pth` | 3.85 GB | `01ad4464f049b2d7efdaa4c1a59b8dfe` | `a6bf693424fbd4ce0244fff8c412347714d5ac586e28dbeffadfa0f2b647af74` | Main flow-prediction network |
| `ext_weights/v1-44.pth` | 1.14 GB | `fab020275fa44c6589820ce025191600` | `ab6cc15dc31947675f75c950c41f4dcfd0d6d1817555ac871f809ec388e4651a` | 44.1 kHz VAE |
| `ext_weights/synchformer_state_dict.pth` | 950 MB | `5b2f5594b0730f70e41e549b7c94390c` | `8aff082f2df5c3bc52759db0c865c7ee772ae6400b860d1b7e90413f2defb67c` | Visual sync encoder |
| `ext_weights/best_netG.pt` | 429 MB | `eeaf372a38a9c31c362120aba2dde292` | `970ca75ee4d5ce583e9396a4534acb14971ea2b4f1c22e038f476680c868a789` | 16 kHz vocoder (fallback) |
| `weights/mmaudio_small_16k.pth` | 601 MB | `af93cde404179f58e3919ac085b8033b` | `61987bcbd6fc689af063075d7efaef29425f65df155dac589c07fa8173a03c1c` | Only if using 16 kHz variant |

### Required BigVGAN file for 44 kHz inference

| File | Size | SHA-256 (LFS oid) | Purpose |
|---|---|---|---|
| `bigvgan_generator.pt` | 489 MB | `d9fe7ec6bd0b44ed9d66973d5012d8181c1570b01e5c72df51973e241dccd357` | 44 kHz BigVGAN v2 generator |
| `config.json` | 1.4 KB | — | Model hyper-parameters |

> The `bigvgan_discriminator_optimizer.pt` (1.53 GB) is also in the repo but is **not required for inference**.

### Required CLIP file for conditioning

| File | Size | SHA-256 (LFS oid) | Purpose |
|---|---|---|---|
| `open_clip_pytorch_model.bin` / `pytorch_model.bin` | ~3.9 GB | — | Text/image conditioning for MMAudio |
| `config.json` | < 10 KB | — | Model configuration |

> The exact filename depends on the `open_clip` / `transformers` loader version (`open_clip_pytorch_model.bin` or `pytorch_model.bin`).

### Verified mirrors

| Mirror | Repo ID / URL | Notes |
|---|---|---|
| **ModelScope (recommended)** | [`PineKing2024/MMAudio`](https://modelscope.cn/models/PineKing2024/MMAudio) | ✅ Contains MMAudio weights, VAE, Synchformer, BigVGAN generator, and Apple CLIP; ships a `checkfile.json` with MD5 checksums |
| **ModelScope (safetensors)** | [`Kijai/MMAudio_safetensors`](https://modelscope.cn/models/Kijai/MMAudio_safetensors) | ComfyUI repack in `.safetensors`; useful only if you migrate the loader to safetensors |

> **Avoid slow HF auto-downloads:** If BigVGAN or CLIP downloads from HuggingFace are unreliable, download the single ModelScope repo `PineKing2024/MMAudio` instead. It bundles all MMAudio dependencies (weights, VAE, Synchformer, BigVGAN generator, and CLIP) so the server can run fully offline after copying the folder into place.

---

## 4. Suggested pre-download workflow

### 4.1 On a machine with a stable connection

Use **ModelScope** when HuggingFace is unstable. Install the SDK once:

```bash
pip install modelscope
```

Then download each repo into a local folder:

```bash
# ACE-Step v1-3.5B (~8.3 GB)
modelscope download --local-dir ./audio_weights/ACE-Step-v1-3.5B ACE-Step/ACE-Step-v1-3.5B

# Stable Audio Open 1.0 (~10.8 GB)
# IMPORTANT: accept the license at https://huggingface.co/stabilityai/stable-audio-open-1.0 first.
modelscope download --local-dir ./audio_weights/stable-audio-open-1.0 stabilityai/stable-audio-open-1.0

# MMAudio + BigVGAN + CLIP (~7.5 GB for large_44k_v2 set)
modelscope download --local-dir ./audio_weights/MMAudio PineKing2024/MMAudio
```

If you prefer the official HF path, use `huggingface-cli` with resume and a long timeout:

```bash
export HF_HUB_DOWNLOAD_TIMEOUT=300
export HF_HUB_ENABLE_HF_TRANSFER=1   # faster, optional: pip install hf-transfer
huggingface-cli download ACE-Step/ACE-Step-v1-3.5B --local-dir ./audio_weights/ACE-Step-v1-3.5B --resume-download
huggingface-cli download stabilityai/stable-audio-open-1.0 --local-dir ./audio_weights/stable-audio-open-1.0 --resume-download
huggingface-cli download hkchengrex/MMAudio --local-dir ./audio_weights/MMAudio --resume-download
huggingface-cli download nvidia/bigvgan_v2_44khz_128band_512x --local-dir ./audio_weights/bigvgan_v2_44khz_128band_512x --resume-download
```

### 4.2 Verify what you downloaded

ACE-Step and Stable Audio Open:

```bash
# List LFS pointers; the SHA-256 in the pointer should match the tables above.
git lfs ls-files --long  # run inside each cloned repo
```

MMAudio:

```bash
# On Linux/macOS/WSL
md5sum audio_weights/MMAudio/weights/mmaudio_large_44k_v2.pth
md5sum audio_weights/MMAudio/ext_weights/v1-44.pth
md5sum audio_weights/MMAudio/ext_weights/synchformer_state_dict.pth
md5sum audio_weights/MMAudio/ext_weights/best_netG.pt
md5sum audio_weights/MMAudio/ext_weights/nvidia/bigvgan_v2_44khz_128band_512x/bigvgan_generator.pt
```

### 4.3 Move weights to the target machine

Copy the entire downloaded folders via USB drive / LAN / rsync. Do not rely on partial file copies; verify checksums on the target machine before starting the server.

### 4.4 Tell the audio server to use local weights

Edit `Plugins/UnrealMCP/audio_server/config.yaml` (or `audio_server/.env`):

```yaml
models:
  ace_step:
    checkpoint_path: "D:/audio_weights/ACE-Step-v1-3.5B"
  stable_audio_open:
    pretrained_name: "D:/audio_weights/stable-audio-open-1.0"
  mmaudio:
    model_name: "large_44k_v2"
```

For MMAudio, the package looks for `weights/` and `ext_weights/` relative to the current working directory or the installed package. The safest approach is to copy the MMAudio weights into the `MMAudio` third-party folder next to the code:

```text
Plugins/UnrealMCP/third_party/MMAudio/
  weights/mmaudio_large_44k_v2.pth
  ext_weights/v1-44.pth
  ext_weights/synchformer_state_dict.pth
  ext_weights/best_netG.pt
  ext_weights/nvidia/bigvgan_v2_44khz_128band_512x/bigvgan_generator.pt
  ext_weights/nvidia/bigvgan_v2_44khz_128band_512x/config.json
```

Or set the working directory to your weight folder before launching `main.py`.

---

## 5. Why `HF_ENDPOINT=https://hf-mirror.com` did not help

`hf-mirror.com` is a reverse proxy / redirector. When it does not have a local cache for a repo, it forwards the request to `huggingface.co`. For the repos above, the mirror returned a `308 Permanent Redirect` back to the official HF URL during testing, so the download still traversed the unstable HF path. Using a true mirror such as **ModelScope** avoids that hop entirely.

---

## 6. Quick reference: total storage

| Model | Minimum files | Approx. size |
|---|---|---|
| ACE-Step v1-3.5B | Full repo | ~8.3 GB |
| Stable Audio Open 1.0 | Full repo (gated) | ~10.8 GB |
| MMAudio (`large_44k_v2`) | weights + ext_weights + BigVGAN | ~6.5 GB |
| **Total** | | **~25–26 GB** |

---

## 7. Sources

- ACE-Step GitHub repo: <https://github.com/ace-step/ACE-Step>
- ACE-Step HuggingFace: <https://huggingface.co/ACE-Step/ACE-Step-v1-3.5B>
- ACE-Step ModelScope mirror: <https://www.modelscope.cn/models/ACE-Step/ACE-Step-v1-3.5B>
- Stable Audio Open HuggingFace: <https://huggingface.co/stabilityai/stable-audio-open-1.0>
- Stable Audio Open ModelScope mirror: <https://modelscope.cn/models/stabilityai/stable-audio-open-1.0>
- MMAudio GitHub / MODELS.md: <https://github.com/hkchengrex/MMAudio/blob/main/docs/MODELS.md>
- MMAudio HuggingFace: <https://huggingface.co/hkchengrex/MMAudio>
- MMAudio ModelScope mirror: <https://modelscope.cn/models/PineKing2024/MMAudio>
- MMAudio download checksums: <https://github.com/hkchengrex/MMAudio/blob/main/mmaudio/utils/download_utils.py>
- BigVGAN v2 44kHz HuggingFace: <https://huggingface.co/nvidia/bigvgan_v2_44khz_128band_512x>
