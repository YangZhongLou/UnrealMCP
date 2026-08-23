"""
HTML → UMG JSON parser for gu-dao-ren HUD mockups.

Supports:
  1) Explicit data-umg-* attributes on elements
  2) Built-in profile for Art/workspace/hud-html-mockup (class-based)

Output schema aligns with UnrealMCP generate_umg_widget (schema_version 1.0).
"""

from __future__ import annotations

import re
from html.parser import HTMLParser
from typing import Any


DESIGN_W = 1920.0
DESIGN_H = 1080.0
SAFE = 24.0

# Colors aligned with hud-html-mockup / ClanUITheme
XUANMO = "#161412"
XIANGHUANG = "#d4a843"
YUBAI = "#e8e0d4"
YUBAI_DIM = "#b8b0a4"
CTA_GOLD = "#d4a843"
CTA_INK = "#140f0a"

# 顶栏版式新增色（Art/UI/Screens/mockups/main-hud/美术设计.md 为 SSOT，对齐 --muted/--gold/--jing/--ling/--guide）
MUTED = "#a09682"
GOLD = "#d2af6e"
JING = "#dc786e"
LING = "#78b4be"
GUIDE = "#c8e0a8"

# 皮件 brush（Content 路径）。军帐家族见 Art/workspace/hud-chrome-llm/_family.md；
# 须已再导入 Content（export 更新后不得沿用旧像素）。
# 面板三族按尺度单轨；钮件九宫由生成器 Box(margin 0.14/0.16) 承载。
BRUSH_PANEL_BAR = "/Game/UI/Texture/T_UI_Background_M19_HudBar"
BRUSH_PANEL_PANEL = "/Game/UI/Texture/T_UI_Background_M19_HudPanel"
BRUSH_PANEL_RAIL = "/Game/UI/Texture/T_UI_Background_M19_HudRail"
BRUSH_BTN_GOLD = "/Game/UI/Texture/T_UI_Frame_HudBtnGold"
BRUSH_BTN_ZHU = "/Game/UI/Texture/T_UI_Frame_HudBtnZhu"
BRUSH_BTN_INK = "/Game/UI/Texture/T_UI_Frame_HudBtnInk"
BRUSH_ICONS = [
    "/Game/UI/Texture/T_UI_Icon_YuanShi_01",
    "/Game/UI/Texture/T_UI_Icon_JingXue_01",
    "/Game/UI/Texture/T_UI_Icon_LingQi_01",
]


def _panel_style() -> dict[str, Any]:
    return {
        "background_color": XUANMO,
        "border_color": "#8a6a3a",
        "opacity": 0.92,
    }


def _text_style(color: str = YUBAI, size: int = 14) -> dict[str, Any]:
    return {"color": color, "font_size": size}


def _style_from_data_umg_attrs(attr_get) -> dict[str, Any]:
    """Build UMG style dict from optional data-umg-color/bg/font-size/opacity/brush."""
    style: dict[str, Any] = {}
    color = attr_get("color", "").strip()
    if color:
        style["color"] = color
    bg = attr_get("bg", "").strip()
    if not bg:
        bg = attr_get("background-color", "").strip()
    if bg:
        style["background_color"] = bg
    font_size = attr_get("font-size", "").strip()
    if font_size:
        try:
            style["font_size"] = int(float(font_size))
        except ValueError:
            pass
    opacity = attr_get("opacity", "").strip()
    if opacity:
        try:
            style["opacity"] = float(opacity)
        except ValueError:
            pass
    brush = attr_get("brush", "").strip()
    if brush:
        style["brush"] = brush
    return style


def _btn(
    name: str,
    text: str,
    x: float,
    y: float,
    w: float,
    h: float,
    *,
    anchors: str,
    primary: bool = False,
    skin: str | None = None,
    text_color: str | None = None,
    visibility: str | None = None,
) -> dict[str, Any]:
    style = {
        "background_color": CTA_GOLD if primary else "#1a1612",
        "color": CTA_INK if primary else YUBAI,
        "font_size": 14,
    }
    # 钮件皮（鎏金/朱砂/玄铁玉白文）；有 brush 时生成器忽略 background_color
    if skin == "gold":
        style = {"brush": BRUSH_BTN_GOLD, "color": YUBAI, "font_size": 14}
    elif skin == "zhu":
        style = {"brush": BRUSH_BTN_ZHU, "color": YUBAI, "font_size": 14}
    elif skin == "ink":
        style = {"brush": BRUSH_BTN_INK, "color": YUBAI, "font_size": 14}
    if text_color:
        style["color"] = text_color
    widget = {
        "type": "Button",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": text,
        "anchors": anchors,
        "style": style,
        "children": [],
    }
    if visibility:
        widget["visibility"] = visibility
    return widget


def _label(
    name: str,
    text: str,
    x: float,
    y: float,
    w: float,
    h: float,
    *,
    anchors: str,
    color: str = YUBAI,
    size: int = 14,
) -> dict[str, Any]:
    return {
        "type": "TextBlock",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": text,
        "anchors": anchors,
        "style": _text_style(color, size),
        "children": [],
    }


def _image_panel(
    name: str,
    x: float,
    y: float,
    w: float,
    h: float,
    *,
    anchors: str,
    brush: str | None = None,
) -> dict[str, Any]:
    return {
        "type": "Image",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": "",
        "anchors": anchors,
        "style": {"brush": brush} if brush else _panel_style(),
        "children": [],
    }


def build_hud_mockup_tree(texts: dict[str, str]) -> dict[str, Any]:
    """Design-space layout for hud-html-mockup at 1920×1080 with corner anchors."""
    turn = texts.get("turn", "1")
    explore = texts.get("explore", "12%")
    objective = texts.get("objective", "目标：开拓者建立主城 → 占领邻格 → 建弟子居 → 招采药人")
    status_title = texts.get("status_title", "开拓者 · 格(0,0)")
    status_detail = texts.get("status_detail", "生命 10/10 · AP 2/2")
    cta = texts.get("cta", "建立主城")

    # --- 顶栏横条（回合 + 资源一行 + 短目标 + 明细）：规格禁止左柱高板，高 64 ---
    bar_w, bar_h = 900.0, 64.0
    bar_x, bar_y = SAFE, SAFE
    bar_widgets: list[dict[str, Any]] = [
        _image_panel("TopBarBg", bar_x, bar_y, bar_w, bar_h, anchors="top-left", brush=BRUSH_PANEL_BAR),
        _label("TxtTurnLabel", "回合", bar_x + 14, bar_y + 14, 82, 13, anchors="top-left", color=MUTED, size=11),
        _label("TxtTurnValue", turn, bar_x + 14, bar_y + 29, 82, 24, anchors="top-left", color=YUBAI, size=20),
    ]
    res_chips = [
        ("元石", texts.get("yuan_shi", "120"), GOLD),
        ("精血", texts.get("jing_xue", "8 / 20"), JING),
        ("灵气", texts.get("ling_qi", "45"), LING),
        ("粮食", texts.get("liang_shi", "30"), MUTED),
    ]
    # 14 左 pad + 82 回合片 + 4 片 margin + 6 flex gap = 106
    chip_x = bar_x + 106
    for i, (name, val, color) in enumerate(res_chips):
        # 图标 + 文案并排（美术设计 §3 精修项）；i=3 粮食无图标件，纯文字（已知缺口）
        has_icon = i < len(BRUSH_ICONS)
        if has_icon:
            bar_widgets.append(
                _image_panel(
                    f"ImgResIcon_{i}",
                    chip_x + 4,
                    bar_y + 22,
                    20,
                    20,
                    anchors="top-left",
                    brush=BRUSH_ICONS[i],
                )
            )
        text_x = chip_x + (28 if has_icon else 10)
        text_w = 74 if has_icon else 78
        bar_widgets.append(
            _label(f"TxtResName_{i}", name, text_x, bar_y + 16, text_w, 13, anchors="top-left", color=MUTED, size=11)
        )
        bar_widgets.append(
            _label(f"TxtResVal_{i}", val, text_x, bar_y + 29, text_w, 22, anchors="top-left", color=color, size=17)
        )
        chip_x += 104  # 98 片宽（78+20 padding）+ 6 gap
    toggle_w, toggle_h = 76.0, 36.0
    toggle_x = bar_x + bar_w - 10 - toggle_w
    bar_widgets.append(
        _label(
            "TxtObjective",
            objective,
            chip_x + 20,
            bar_y + 23,
            toggle_x - 6 - (chip_x + 20),
            18,
            anchors="top-left",
            color=GUIDE,
            size=12,
        )
    )
    bar_widgets.append(
        _btn(
            "BtnResourceToggle",
            "明细 ▾",
            toggle_x,
            bar_y + 14,
            toggle_w,
            toggle_h,
            anchors="top-left",
            skin="ink",
        )
    )

    # --- 顶栏明细下拉（默认收起；BtnResourceToggle 展开，高 ≤220）---
    detail_h = 84.0
    detail_root = {
        "type": "CanvasPanel",
        "name": "ResDetailRoot",
        "x": bar_x,
        "y": bar_y + bar_h + 6,
        "width": bar_w,
        "height": detail_h,
        "text": "",
        "anchors": "top-left",
        "z_order": 30,
        "visibility": "collapsed",
        "style": {},
        "children": [
            _image_panel("ResDetailBg", 0, 0, bar_w, detail_h, anchors="fill", brush=BRUSH_PANEL_BAR),
            _label(
                "TxtDetailTip",
                texts.get("detail_tip", "开局路径：开拓者建立主城 → 占领邻格 → 建弟子居 → 招采药人"),
                16,
                12,
                bar_w - 32,
                18,
                anchors="top-left",
                color=GUIDE,
                size=13,
            ),
            _label(
                "TxtDetailLivelihood",
                texts.get("livelihood", "民生：粮 30 / 需 12 · 安稳"),
                16,
                36,
                bar_w - 32,
                16,
                anchors="top-left",
                color=MUTED,
                size=12,
            ),
            _label(
                "TxtDetailSpiritBeast",
                texts.get("spirit_beast", "灵兽：0/3"),
                16,
                54,
                bar_w - 32,
                16,
                anchors="top-left",
                color=MUTED,
                size=12,
            ),
        ],
    }

    # --- 右上：探索 pill（回合已入顶栏，不再单独成片）---
    pill_w, pill_h = 120.0, 64.0
    explore_x = DESIGN_W - SAFE - pill_w
    pill_y = SAFE
    pill_widgets = [
        _image_panel("ExplorePillBg", explore_x, pill_y, pill_w, pill_h, anchors="top-right", brush=BRUSH_PANEL_PANEL),
        _label("TxtExploreLabel", "探索度", explore_x, pill_y + 10, pill_w, 13, anchors="top-right", color=MUTED, size=11),
        _label("TxtExploreValue", explore, explore_x, pill_y + 26, pill_w, 24, anchors="top-right", color=GOLD, size=18),
    ]

    # --- 系统轨：头钮（收起/展开）+ 体钮 ×6，整体一块面板 ---
    rail_w, rail_h_btn, rail_gap = 132.0, 36.0, 8.0
    rail_labels = ["系统 ▾", "地图", "百科", "仙学", "内政", "外交", "探索"]
    rail_top = pill_y + pill_h + 10.0
    rail_head_h = 36.0
    rail_panel_h = rail_head_h + 8.0 + (len(rail_labels) - 1) * rail_h_btn + (len(rail_labels) - 2) * rail_gap + 8.0
    rail_x = DESIGN_W - SAFE - rail_w
    rail_widgets: list[dict[str, Any]] = [
        _image_panel("SystemRailBg", rail_x, rail_top, rail_w, rail_panel_h, anchors="top-right", brush=BRUSH_PANEL_RAIL),
        _btn(
            "BtnRail_0",
            rail_labels[0],
            rail_x,
            rail_top,
            rail_w,
            rail_head_h,
            anchors="top-right",
            skin="ink",
            text_color=GOLD,
        ),
    ]
    for j, lab in enumerate(rail_labels[1:]):
        y = rail_top + rail_head_h + 8.0 + j * (rail_h_btn + rail_gap)
        rail_widgets.append(
            _btn(
                f"BtnRail_{j + 1}",
                lab,
                rail_x + 8,
                y,
                rail_w - 16,
                rail_h_btn,
                anchors="top-right",
                skin="ink",
            )
        )

    # --- Bottom-left context dock ---
    # All gameplay chips are authored in the WBP and start Collapsed. C++ only
    # toggles/reflows these named controls; it never constructs visual fallback.
    dock_w, dock_h = 520.0, 132.0
    dock_x = SAFE
    dock_y = DESIGN_H - SAFE - dock_h
    action_defs = [
        ("BtnChipFound", "建立主城", True),
        ("BtnExpand", "占领领地", True),
        ("BtnCultivateFanTian", "开垦凡田", False),
        ("BtnFanTianHerb", "改药垄", False),
        ("BtnFanTianGrain", "改粮垄", False),
        ("BtnUpgradeLingTian", "引灵成田", False),
        ("BtnBuild", "建造", False),
        ("BtnClearBeastLair", "清巢", False),
        ("BtnGarrisonBeast", "驻守", False),
        ("BtnUngarrisonBeast", "撤离", False),
        ("BtnGather", "采集", False),
        ("BtnAttack", "攻击", False),
        ("BtnFortify", "驻防", False),
        ("BtnHuiChun", "回春术", False),
        ("BtnChuanLin", "穿林掠影", False),
        ("BtnGuWu", "蛊雾迷踪", False),
        ("BtnCuiShu", "灵气催熟", False),
        ("BtnChunHuo", "淬火炼丹", False),
        ("BtnReinforceVein", "加固矿脉", False),
        ("BtnRecruitCaiYao", "招采药人", False),
        ("BtnRecruitZhanTang", "招战堂", False),
        ("BtnRecruitZhenCha", "招斥候", False),
        ("BtnRecruitKuangGong", "招矿工", False),
        ("BtnRecruitHuFa", "招护法", False),
        ("BtnRecruitYiShi", "招医师", False),
        ("BtnRecruitGuShi", "招蛊师", False),
        ("BtnRecruitLianDanShi", "招炼丹师", False),
        ("BtnRecruitLingZhiFu", "招灵植夫", False),
    ]
    context_children: list[dict[str, Any]] = [
        _image_panel("ContextDockBg", 0, 0, dock_w, dock_h, anchors="fill", brush=BRUSH_PANEL_PANEL),
        _label(
            "TxtStatusTitle",
            status_title,
            12,
            8,
            380,
            22,
            anchors="top-left",
            color="#ffe9a8",
            size=14,
        ),
        _label(
            "TxtStatusDetail",
            status_detail,
            12,
            32,
            380,
            32,
            anchors="top-left",
            color=YUBAI_DIM,
            size=12,
        ),
        _btn(
            "BtnStatusExec",
            "先点目标格",
            dock_w - 112,
            16,
            100,
            32,
            anchors="top-left",
            skin="ink",
        ),
    ]
    for name, label_text, primary in action_defs:
        context_children.append(
            _btn(
                name,
                label_text,
                12,
                80,
                148,
                40,
                anchors="top-left",
                skin="gold" if primary else "ink",
                visibility=None if name == "BtnChipFound" else "collapsed",
            )
        )
    dock_widgets = [
        {
            "type": "CanvasPanel",
            "name": "ContextDockRoot",
            "x": dock_x,
            "y": dock_y,
            "width": dock_w,
            "height": dock_h,
            "text": "",
            "anchors": "bottom-left",
            "z_order": 20,
            "style": {},
            "children": context_children,
        }
    ]

    # --- Bottom-right CTA column (right edges aligned) ---
    # Bottom→top: EndTurn, [ForceEnd if visible], Settings, Pause.
    # Gap matches HTML .cta-row (10). Force starts collapsed / hidden:
    # baked positions assume Force takes no space (Settings sits just above CTA).
    # Runtime LayoutHtmlHudBottomRightCta shifts Settings/Pause up when Force is Visible.
    cta_w, cta_h = 200.0, 48.0
    quick_w, quick_h, gap = 132.0, 40.0, 10.0
    force_w = 132.0
    cta_x = DESIGN_W - SAFE - cta_w
    cta_y = DESIGN_H - SAFE - cta_h
    side_x = DESIGN_W - SAFE - quick_w
    force_x = DESIGN_W - SAFE - force_w
    settings_y = cta_y - gap - quick_h
    pause_y = settings_y - gap - quick_h
    force_y = settings_y
    cta_widgets = [
        _btn("BtnEndTurn", cta, cta_x, cta_y, cta_w, cta_h, anchors="bottom-right", skin="gold"),
        _btn(
            "BtnForceEndTurn",
            "强行结束",
            force_x,
            force_y,
            force_w,
            quick_h,
            anchors="bottom-right",
            skin="gold",
            visibility="collapsed",
        ),
        _btn(
            "BtnQuickSettings",
            "系统",
            side_x,
            settings_y,
            quick_w,
            quick_h,
            anchors="bottom-right",
            skin="gold",
        ),
        _btn(
            "BtnQuickPause",
            "暂停",
            side_x,
            pause_y,
            quick_w,
            quick_h,
            anchors="bottom-right",
            skin="gold",
        ),
    ]

    # --- Force-end confirmation overlay (generated in WBP; no runtime C++ chrome) ---
    confirm_fog = _image_panel(
        "ForceEndConfirmFog",
        0,
        0,
        DESIGN_W,
        DESIGN_H,
        anchors="fill",
    )
    confirm_fog["style"] = {
        "background_color": "#060504",
        "opacity": 0.74,
    }
    confirm_panel = _image_panel(
        "ForceEndConfirmPanel",
        680,
        410,
        560,
        260,
        anchors="center",
        brush=BRUSH_PANEL_PANEL,
    )
    confirm_root = {
        "type": "CanvasPanel",
        "name": "ForceEndConfirmRoot",
        "x": 0,
        "y": 0,
        "width": DESIGN_W,
        "height": DESIGN_H,
        "text": "",
        "anchors": "fill",
        "z_order": 100,
        "visibility": "collapsed",
        "style": {},
        "children": [
            confirm_fog,
            confirm_panel,
            _label(
                "TxtForceEndConfirmTitle",
                "结束回合？",
                740,
                448,
                440,
                40,
                anchors="center",
                color=XIANGHUANG,
                size=24,
            ),
            _label(
                "TxtForceEndConfirmBody",
                "仍有可操作事项，确认结束回合？",
                740,
                510,
                440,
                50,
                anchors="center",
                color=YUBAI,
                size=15,
            ),
            _btn(
                "BtnForceEndConfirmCancel",
                "取消",
                760,
                598,
                150,
                44,
                anchors="center",
                skin="ink",
            ),
            _btn(
                "BtnForceEndConfirmOk",
                "确认结束",
                1010,
                598,
                150,
                44,
                anchors="center",
                skin="zhu",
            ),
        ],
    }

    children = (
        bar_widgets
        + [detail_root]
        + pill_widgets
        + rail_widgets
        + dock_widgets
        + cta_widgets
        + [confirm_root]
    )
    return {
        "schema_version": "1.0",
        "canvas_width": DESIGN_W,
        "canvas_height": DESIGN_H,
        "blueprint_name": "WBP_MainHud_FromHtml",
        "output_path": "/Game/UI/HUD",
        "source": "hud-html-mockup",
        "widgets": [
            {
                "type": "CanvasPanel",
                "name": "RootCanvas",
                "x": 0,
                "y": 0,
                "width": DESIGN_W,
                "height": DESIGN_H,
                "text": "",
                "anchors": "fill",
                "visibility": "self-hit-test-invisible",
                "style": {},
                "children": children,
            }
        ],
    }


class _HudTextExtractor(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.texts: dict[str, str] = {}
        self._capture_id: str | None = None
        self._buf: list[str] = []
        self._in_hud = False
        self.has_hud_classes = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        ad = {k: (v or "") for k, v in attrs}
        classes = set((ad.get("class") or "").split())
        if "hud" in classes or "res-panel" in classes or "right-stack" in classes:
            self.has_hud_classes = True
            self._in_hud = True
        eid = ad.get("id", "")
        if eid in {
            "turn-val",
            "objective-tip",
            "status-title",
            "status-detail",
            "status-name",
            "btn-cta",
        }:
            self._capture_id = eid
            self._buf = []
        if "data-umg-type" in ad:
            self.has_hud_classes = True

    def handle_endtag(self, tag: str) -> None:
        if self._capture_id is not None:
            text = re.sub(r"\s+", " ", "".join(self._buf)).strip()
            key_map = {
                "turn-val": "turn",
                "objective-tip": "objective",
                "status-title": "status_title",
                "status-name": "status_title",
                "status-detail": "status_detail",
                "btn-cta": "cta",
            }
            if self._capture_id in key_map and text:
                self.texts[key_map[self._capture_id]] = text
            self._capture_id = None
            self._buf = []

    def handle_data(self, data: str) -> None:
        if self._capture_id is not None:
            self._buf.append(data)


def _parse_data_umg_tree(html: str) -> dict[str, Any] | None:
    """If HTML uses data-umg-* attributes, build a generic tree (flat under RootCanvas).

    Returns None when no data-umg-type markers match. Callers that detect
    data-umg-type in the source must treat None as a hard parse failure
    (do not fall back to the HUD profile).
    """
    pattern = re.compile(
        r"<([a-zA-Z0-9]+)([^>]*data-umg-type\s*=\s*[\"']([^\"']+)[\"'][^>]*)>(.*?)</\1>",
        re.DOTALL | re.IGNORECASE,
    )
    children: list[dict[str, Any]] = []
    errors: list[str] = []
    for m in pattern.finditer(html):
        attrs_s, umg_type, inner = m.group(2), m.group(3), m.group(4)

        def attr(name: str, default: str = "") -> str:
            am = re.search(rf"data-umg-{name}\s*=\s*[\"']([^\"']*)[\"']", attrs_s, re.I)
            return am.group(1) if am else default

        missing = [
            key
            for key in ("name", "x", "y", "w", "h", "anchor")
            if not attr(key, "").strip()
        ]
        if not umg_type.strip():
            missing.append("type")
        if missing:
            errors.append(
                f"data-umg 节点缺少必填属性: {', '.join(missing)} (type={umg_type!r})"
            )
            continue

        def number_attr(name: str) -> float:
            return float(attr(name))

        try:
            x = number_attr("x")
            y = number_attr("y")
            w = number_attr("w")
            h = number_attr("h")
        except ValueError:
            errors.append(f"data-umg 节点坐标/尺寸不是数字 (name={attr('name')!r})")
            continue

        name = attr("name")
        anchors = attr("anchor")
        text = re.sub(r"<[^>]+>", "", inner).strip() or attr("text", "")
        style = _style_from_data_umg_attrs(attr)
        widget = {
            "type": umg_type,
            "name": name,
            "x": x,
            "y": y,
            "width": w,
            "height": h,
            "text": text,
            "anchors": anchors,
            "style": style,
            "children": [],
        }
        visibility = attr("visibility", "")
        if visibility:
            widget["visibility"] = visibility
        z_order = attr("z", "")
        if z_order:
            try:
                widget["z_order"] = int(z_order)
            except ValueError:
                errors.append(f"data-umg-z 不是整数 (name={name!r})")
                continue
        children.append(widget)

    if errors:
        return {
            "schema_version": "1.0",
            "status": "error",
            "message": "; ".join(errors),
            "widgets": [],
        }
    if not children:
        return None
    return {
        "schema_version": "1.0",
        "canvas_width": DESIGN_W,
        "canvas_height": DESIGN_H,
        "blueprint_name": "WBP_FromHtml",
        "output_path": "/Game/UI/HUD",
        "source": "data-umg",
        "status": "success",
        "message": f"已解析 data-umg 契约，子控件数 {len(children)}",
        "widgets": [
            {
                "type": "CanvasPanel",
                "name": "RootCanvas",
                "x": 0,
                "y": 0,
                "width": DESIGN_W,
                "height": DESIGN_H,
                "text": "",
                "anchors": "fill",
                "visibility": "self-hit-test-invisible",
                "style": {},
                "children": children,
            }
        ],
    }


def analyze_html(html: str, *, blueprint_name: str | None = None, output_path: str | None = None) -> dict[str, Any]:
    """Parse HTML string into UMG widget JSON."""
    has_data_umg = "data-umg-type" in html
    data_tree = _parse_data_umg_tree(html)
    if data_tree is not None:
        if data_tree.get("status") == "error":
            return data_tree
        if blueprint_name:
            data_tree["blueprint_name"] = blueprint_name
        if output_path:
            data_tree["output_path"] = output_path
        return data_tree
    if has_data_umg:
        return {
            "schema_version": "1.0",
            "status": "error",
            "message": (
                "检测到 data-umg-type，但未能解析出任何节点"
                "（不支持自闭合标签；需要成对开闭标签与必填属性 "
                "type/name/x/y/w/h/anchor）"
            ),
            "widgets": [],
        }

    extractor = _HudTextExtractor()
    extractor.feed(html)
    if not extractor.has_hud_classes and ".hud" not in html and "res-panel" not in html:
        return {
            "schema_version": "1.0",
            "status": "error",
            "message": "未识别为 HUD mockup（需要 .hud / .res-panel 或 data-umg-type）",
            "widgets": [],
        }

    tree = build_hud_mockup_tree(extractor.texts)
    if blueprint_name:
        tree["blueprint_name"] = blueprint_name
    if output_path:
        tree["output_path"] = output_path
    tree["status"] = "success"
    tree["message"] = f"已解析 HUD mockup，控件数={_count_widgets(tree)}"
    return tree


def analyze_html_file(path: str, **kwargs: Any) -> dict[str, Any]:
    with open(path, "r", encoding="utf-8") as f:
        html = f.read()
    tree = analyze_html(html, **kwargs)
    tree["source_path"] = path
    return tree


def _count_widgets(node: dict[str, Any]) -> int:
    n = 0
    for w in node.get("widgets", []):
        n += 1 + _count_widgets_in(w)
    return n


def _count_widgets_in(w: dict[str, Any]) -> int:
    n = 0
    for c in w.get("children", []):
        n += 1 + _count_widgets_in(c)
    return n
