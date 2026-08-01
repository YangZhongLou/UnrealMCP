//! HTML HUD mockup → UMG widget_tree JSON (schema_version 1.0).
//! Integrated into UnrealMCP; pairs with UE command `generate_umg_widget`.

use regex::Regex;
use serde_json::{json, Value};
use std::collections::HashMap;
use std::fs;
use std::path::Path;

const DESIGN_W: f64 = 1920.0;
const DESIGN_H: f64 = 1080.0;
const SAFE: f64 = 24.0;

const XUANMO: &str = "#161412";
const XIANGHUANG: &str = "#d4a843";
const YUBAI: &str = "#e8e0d4";
const YUBAI_DIM: &str = "#b8b0a4";
const CTA_GOLD: &str = "#d4a843";
const CTA_INK: &str = "#140f0a";

fn panel_style() -> Value {
    json!({
        "background_color": XUANMO,
        "border_color": "#8a6a3a",
        "opacity": 0.92
    })
}

fn text_style(color: &str, size: i32) -> Value {
    json!({ "color": color, "font_size": size })
}

fn style_from_data_umg_attrs(attr: &dyn Fn(&str) -> Option<String>) -> Value {
    let mut style = serde_json::Map::new();
    if let Some(color) = attr("color") {
        style.insert("color".into(), json!(color));
    }
    let bg = attr("bg").or_else(|| attr("background-color"));
    if let Some(bg) = bg {
        style.insert("background_color".into(), json!(bg));
    }
    if let Some(font_size) = attr("font-size") {
        if let Ok(n) = font_size.parse::<f64>() {
            style.insert("font_size".into(), json!(n as i32));
        }
    }
    if let Some(opacity) = attr("opacity") {
        if let Ok(n) = opacity.parse::<f64>() {
            style.insert("opacity".into(), json!(n));
        }
    }
    if let Some(brush) = attr("brush") {
        style.insert("brush".into(), json!(brush));
    }
    Value::Object(style)
}

fn label(
    name: &str,
    text: &str,
    x: f64,
    y: f64,
    w: f64,
    h: f64,
    anchors: &str,
    color: &str,
    size: i32,
) -> Value {
    json!({
        "type": "TextBlock",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": text,
        "anchors": anchors,
        "style": text_style(color, size),
        "children": []
    })
}

fn image_panel(name: &str, x: f64, y: f64, w: f64, h: f64, anchors: &str) -> Value {
    json!({
        "type": "Image",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": "",
        "anchors": anchors,
        "style": panel_style(),
        "children": []
    })
}

fn button(
    name: &str,
    text: &str,
    x: f64,
    y: f64,
    w: f64,
    h: f64,
    anchors: &str,
    primary: bool,
) -> Value {
    json!({
        "type": "Button",
        "name": name,
        "x": x,
        "y": y,
        "width": w,
        "height": h,
        "text": text,
        "anchors": anchors,
        "style": {
            "background_color": if primary { CTA_GOLD } else { "#1a1612" },
            "color": if primary { CTA_INK } else { YUBAI },
            "font_size": 14
        },
        "children": []
    })
}

fn extract_texts(html: &str) -> HashMap<String, String> {
    let mut out = HashMap::new();
    let pairs = [
        ("turn-val", "turn"),
        ("objective-tip", "objective"),
        ("status-title", "status_title"),
        ("status-name", "status_title"),
        ("status-detail", "status_detail"),
        ("btn-cta", "cta"),
    ];
    for (id, key) in pairs {
        let pat = format!(
            r#"(?is)id\s*=\s*["']{}["'][^>]*>(.*?)</"#,
            regex::escape(id)
        );
        if let Ok(re) = Regex::new(&pat) {
            if let Some(caps) = re.captures(html) {
                let raw = caps.get(1).map(|m| m.as_str()).unwrap_or("");
                let text = Regex::new(r"<[^>]+>")
                    .ok()
                    .map(|r| r.replace_all(raw, "").to_string())
                    .unwrap_or_else(|| raw.to_string());
                let text = Regex::new(r"\s+")
                    .ok()
                    .map(|r| r.replace_all(text.trim(), " ").to_string())
                    .unwrap_or(text);
                if !text.is_empty() {
                    out.insert(key.to_string(), text);
                }
            }
        }
    }
    out
}

fn looks_like_hud(html: &str) -> bool {
    html.contains("class=\"hud\"")
        || html.contains("class='hud'")
        || html.contains("res-panel")
        || html.contains("right-stack")
        || html.contains("data-umg-type")
}

fn parse_data_umg(html: &str) -> Result<Option<Value>, String> {
    let re = Regex::new(
        r#"(?is)<[a-zA-Z0-9]+([^>]*data-umg-type\s*=\s*["']([^"']+)["'][^>]*)>(.*?)</[a-zA-Z0-9]+>"#,
    )
    .map_err(|e| e.to_string())?;
    let mut children = Vec::new();
    let mut errors = Vec::new();
    for caps in re.captures_iter(html) {
        let attrs = caps.get(1).map(|m| m.as_str()).unwrap_or("");
        let umg_type = caps.get(2).map(|m| m.as_str()).unwrap_or("").trim();
        let inner = caps.get(3).map(|m| m.as_str()).unwrap_or("");
        let attr = |name: &str| -> Option<String> {
            let pat = format!(
                r#"(?i)data-umg-{}\s*=\s*["']([^"']*)["']"#,
                regex::escape(name)
            );
            Regex::new(&pat)
                .ok()
                .and_then(|r| r.captures(attrs).map(|c| c[1].to_string()))
                .map(|s| s.trim().to_string())
                .filter(|s| !s.is_empty())
        };
        let mut missing = Vec::new();
        if umg_type.is_empty() {
            missing.push("type");
        }
        for key in ["name", "x", "y", "w", "h", "anchor"] {
            if attr(key).is_none() {
                missing.push(key);
            }
        }
        if !missing.is_empty() {
            errors.push(format!(
                "data-umg 节点缺少必填属性: {} (type={umg_type:?})",
                missing.join(", ")
            ));
            continue;
        }
        let name = attr("name").unwrap();
        let anchors = attr("anchor").unwrap();
        let parse_f64 = |key: &str| -> Result<f64, String> {
            attr(key)
                .unwrap()
                .parse()
                .map_err(|_| format!("data-umg 节点坐标/尺寸不是数字 (name={name})"))
        };
        let x = match parse_f64("x") {
            Ok(v) => v,
            Err(e) => {
                errors.push(e);
                continue;
            }
        };
        let y = match parse_f64("y") {
            Ok(v) => v,
            Err(e) => {
                errors.push(e);
                continue;
            }
        };
        let w = match parse_f64("w") {
            Ok(v) => v,
            Err(e) => {
                errors.push(e);
                continue;
            }
        };
        let h = match parse_f64("h") {
            Ok(v) => v,
            Err(e) => {
                errors.push(e);
                continue;
            }
        };
        let text = Regex::new(r"<[^>]+>")
            .ok()
            .map(|r| r.replace_all(inner, "").to_string())
            .unwrap_or_else(|| inner.to_string());
        let text = text.trim();
        let text = if text.is_empty() {
            attr("text").unwrap_or_default()
        } else {
            text.to_string()
        };
        let mut child = json!({
            "type": umg_type,
            "name": name,
            "x": x,
            "y": y,
            "width": w,
            "height": h,
            "text": text,
            "anchors": anchors,
            "style": style_from_data_umg_attrs(&attr),
            "children": []
        });
        if let Some(visibility) = attr("visibility") {
            child["visibility"] = json!(visibility);
        }
        if let Some(z_order) = attr("z") {
            match z_order.parse::<i32>() {
                Ok(z) => child["z_order"] = json!(z),
                Err(_) => {
                    errors.push(format!("data-umg-z 不是整数 (name={name})"));
                    continue;
                }
            }
        }
        children.push(child);
    }
    if !errors.is_empty() {
        return Err(errors.join("; "));
    }
    if children.is_empty() {
        return Ok(None);
    }
    Ok(Some(json!({
        "schema_version": "1.0",
        "canvas_width": DESIGN_W,
        "canvas_height": DESIGN_H,
        "blueprint_name": "WBP_FromHtml",
        "output_path": "/Game/UI/HUD",
        "source": "data-umg",
        "status": "success",
        "widgets": [{
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
            "children": children
        }]
    })))
}

fn build_hud_mockup(texts: &HashMap<String, String>) -> Value {
    let get = |k: &str, d: &'static str| texts.get(k).map(|s| s.as_str()).unwrap_or(d);

    let mut children: Vec<Value> = Vec::new();

    // Top-left resource panel
    let (res_w, res_h) = (340.0, 280.0);
    let (res_x, res_y) = (SAFE, SAFE);
    children.push(image_panel(
        "ResPanelBg",
        res_x,
        res_y,
        res_w,
        res_h,
        "top-left",
    ));
    children.push(label(
        "TxtResTitle",
        get("res_title", "宗门资源"),
        res_x + 14.0,
        res_y + 10.0,
        200.0,
        28.0,
        "top-left",
        XIANGHUANG,
        18,
    ));
    children.push(button(
        "BtnResourceToggle",
        "收起 ▾",
        res_x + res_w - 90.0,
        res_y + 8.0,
        76.0,
        30.0,
        "top-left",
        false,
    ));
    let rows = [
        ("元石", get("yuan_shi", "120"), XIANGHUANG),
        ("精血", get("jing_xue", "8 / 20"), "#e07068"),
        ("灵气", get("ling_qi", "45"), "#7eb0bc"),
        ("粮食", get("liang_shi", "30"), YUBAI_DIM),
    ];
    let mut row_y = res_y + 48.0;
    for (i, (name, val, color)) in rows.iter().enumerate() {
        children.push(label(
            &format!("TxtResName_{i}"),
            name,
            res_x + 14.0,
            row_y,
            120.0,
            22.0,
            "top-left",
            color,
            16,
        ));
        children.push(label(
            &format!("TxtResVal_{i}"),
            val,
            res_x + 160.0,
            row_y,
            160.0,
            22.0,
            "top-left",
            color,
            16,
        ));
        row_y += 26.0;
    }
    children.push(label(
        "TxtObjective",
        get(
            "objective",
            "目标：开拓者建立主城 → 占领邻格 → 建弟子居 → 招采药人",
        ),
        res_x + 14.0,
        row_y + 12.0,
        res_w - 28.0,
        60.0,
        "top-left",
        "#c8e0a8",
        12,
    ));

    // Top-right pills
    let (pill_w, pill_h, pill_gap) = (120.0, 56.0, 10.0);
    let explore_x = DESIGN_W - SAFE - pill_w;
    let turn_x = explore_x - pill_gap - pill_w;
    let pill_y = SAFE;
    children.push(image_panel(
        "TurnPillBg",
        turn_x,
        pill_y,
        pill_w,
        pill_h,
        "top-right",
    ));
    children.push(label(
        "TxtTurnLabel",
        "回合",
        turn_x,
        pill_y + 6.0,
        pill_w,
        16.0,
        "top-right",
        YUBAI_DIM,
        11,
    ));
    children.push(label(
        "TxtTurnValue",
        get("turn", "1"),
        turn_x,
        pill_y + 24.0,
        pill_w,
        24.0,
        "top-right",
        XIANGHUANG,
        18,
    ));
    children.push(image_panel(
        "ExplorePillBg",
        explore_x,
        pill_y,
        pill_w,
        pill_h,
        "top-right",
    ));
    children.push(label(
        "TxtExploreLabel",
        "探索度",
        explore_x,
        pill_y + 6.0,
        pill_w,
        16.0,
        "top-right",
        YUBAI_DIM,
        11,
    ));
    children.push(label(
        "TxtExploreValue",
        get("explore", "12%"),
        explore_x,
        pill_y + 24.0,
        pill_w,
        24.0,
        "top-right",
        XIANGHUANG,
        18,
    ));

    // System rail
    let (rail_w, rail_h_btn, rail_gap) = (132.0, 36.0, 8.0);
    let rail_labels = ["系统 ▾", "地图", "百科", "仙学", "内政", "外交", "探索"];
    let rail_top = SAFE + pill_h + pill_gap;
    let rail_panel_h =
        rail_labels.len() as f64 * rail_h_btn + (rail_labels.len() - 1) as f64 * rail_gap + 16.0;
    let rail_x = DESIGN_W - SAFE - rail_w;
    children.push(image_panel(
        "SystemRailBg",
        rail_x - 8.0,
        rail_top - 8.0,
        rail_w + 16.0,
        rail_panel_h,
        "top-right",
    ));
    for (i, lab) in rail_labels.iter().enumerate() {
        let y = rail_top + i as f64 * (rail_h_btn + rail_gap);
        children.push(button(
            &format!("BtnRail_{i}"),
            lab,
            rail_x,
            y,
            rail_w,
            rail_h_btn,
            "top-right",
            i == 0,
        ));
    }

    // Bottom-left context dock. All gameplay chips exist in the WBP and start
    // Collapsed; runtime code only toggles/reflows these named controls.
    let (dock_w, dock_h) = (520.0, 72.0);
    let dock_x = SAFE;
    let dock_y = DESIGN_H - SAFE - dock_h;
    let action_defs = [
        ("BtnChipFound", "建立主城", true),
        ("BtnExpand", "占领领地", true),
        ("BtnCultivateFanTian", "开垦凡田", false),
        ("BtnFanTianHerb", "改药垄", false),
        ("BtnFanTianGrain", "改粮垄", false),
        ("BtnUpgradeLingTian", "引灵成田", false),
        ("BtnBuild", "建造", false),
        ("BtnClearBeastLair", "清巢", false),
        ("BtnGarrisonBeast", "驻守", false),
        ("BtnUngarrisonBeast", "撤离", false),
        ("BtnGather", "采集", false),
        ("BtnAttack", "攻击", false),
        ("BtnFortify", "驻防", false),
        ("BtnHuiChun", "回春术", false),
        ("BtnChuanLin", "穿林掠影", false),
        ("BtnGuWu", "蛊雾迷踪", false),
        ("BtnCuiShu", "灵气催熟", false),
        ("BtnChunHuo", "淬火炼丹", false),
        ("BtnReinforceVein", "加固矿脉", false),
        ("BtnRecruitCaiYao", "招采药人", false),
        ("BtnRecruitZhanTang", "招战堂", false),
        ("BtnRecruitZhenCha", "招斥候", false),
        ("BtnRecruitKuangGong", "招矿工", false),
        ("BtnRecruitHuFa", "招护法", false),
        ("BtnRecruitYiShi", "招医师", false),
        ("BtnRecruitGuShi", "招蛊师", false),
        ("BtnRecruitLianDanShi", "招炼丹师", false),
        ("BtnRecruitLingZhiFu", "招灵植夫", false),
    ];
    let mut context_children = vec![
        image_panel("ContextDockBg", 0.0, 0.0, dock_w, dock_h, "fill"),
        label(
            "TxtStatusTitle",
            get("status_title", "开拓者 · 格(0,0)"),
            12.0,
            8.0,
            380.0,
            22.0,
            "top-left",
            "#ffe9a8",
            14,
        ),
        label(
            "TxtStatusDetail",
            get("status_detail", "生命 10/10 · AP 2/2"),
            12.0,
            32.0,
            380.0,
            32.0,
            "top-left",
            YUBAI_DIM,
            12,
        ),
        button(
            "BtnStatusExec",
            "先点目标格",
            dock_w - 112.0,
            16.0,
            100.0,
            32.0,
            "top-left",
            false,
        ),
    ];
    for (name, text, primary) in action_defs {
        let mut action = button(name, text, 12.0, 80.0, 148.0, 40.0, "top-left", primary);
        action["visibility"] = json!("collapsed");
        context_children.push(action);
    }
    children.push(json!({
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
        "children": context_children
    }));

    // Bottom-right CTA
    let (cta_w, cta_h) = (200.0, 48.0);
    let (quick_w, quick_h, gap) = (132.0, 40.0, 10.0);
    let force_w = 120.0;
    let cta_x = DESIGN_W - SAFE - cta_w;
    let cta_y = DESIGN_H - SAFE - cta_h;
    let side_y = cta_y + (cta_h - quick_h) * 0.5;
    let force_x = cta_x - gap - force_w;
    // ForceEnd starts Collapsed at runtime, so Settings/Pause hug the CTA.
    // LayoutHtmlHudBottomRightCta inserts ForceEnd into this row when visible.
    let settings_x = cta_x - gap - quick_w;
    let pause_x = settings_x - gap - quick_w;
    children.push(button(
        "BtnEndTurn",
        get("cta", "建立主城"),
        cta_x,
        cta_y,
        cta_w,
        cta_h,
        "bottom-right",
        true,
    ));
    let mut force_button = button(
        "BtnForceEndTurn",
        "强行结束",
        force_x,
        side_y,
        force_w,
        quick_h,
        "bottom-right",
        false,
    );
    force_button["visibility"] = json!("collapsed");
    children.push(force_button);
    children.push(button(
        "BtnQuickSettings",
        "系统",
        settings_x,
        side_y,
        quick_w,
        quick_h,
        "bottom-right",
        false,
    ));
    children.push(button(
        "BtnQuickPause",
        "暂停",
        pause_x,
        side_y,
        quick_w,
        quick_h,
        "bottom-right",
        false,
    ));

    // Force-end confirmation is generated into the WBP. Runtime code only wires it.
    let mut confirm_fog = image_panel("ForceEndConfirmFog", 0.0, 0.0, DESIGN_W, DESIGN_H, "fill");
    confirm_fog["style"] = json!({
        "background_color": "#060504",
        "opacity": 0.74
    });
    children.push(json!({
        "type": "CanvasPanel",
        "name": "ForceEndConfirmRoot",
        "x": 0.0,
        "y": 0.0,
        "width": DESIGN_W,
        "height": DESIGN_H,
        "text": "",
        "anchors": "fill",
        "z_order": 100,
        "visibility": "collapsed",
        "style": {},
        "children": [
            confirm_fog,
            image_panel("ForceEndConfirmPanel", 680.0, 410.0, 560.0, 260.0, "center"),
            label(
                "TxtForceEndConfirmTitle",
                "结束回合？",
                740.0,
                448.0,
                440.0,
                40.0,
                "center",
                XIANGHUANG,
                24,
            ),
            label(
                "TxtForceEndConfirmBody",
                "仍有可操作事项，确认结束回合？",
                740.0,
                510.0,
                440.0,
                50.0,
                "center",
                YUBAI,
                15,
            ),
            button(
                "BtnForceEndConfirmCancel",
                "取消",
                760.0,
                598.0,
                150.0,
                44.0,
                "center",
                false,
            ),
            button(
                "BtnForceEndConfirmOk",
                "确认结束",
                1010.0,
                598.0,
                150.0,
                44.0,
                "center",
                true,
            )
        ]
    }));

    let count = children.len();
    json!({
        "schema_version": "1.0",
        "canvas_width": DESIGN_W,
        "canvas_height": DESIGN_H,
        "blueprint_name": "WBP_MainHud_FromHtml",
        "output_path": "/Game/UI/HUD",
        "source": "hud-html-mockup",
        "status": "success",
        "message": format!("已解析 HUD mockup，子控件数 {count}"),
        "widgets": [{
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
            "children": children
        }]
    })
}

/// Parse HTML string into widget_tree JSON.
pub fn analyze_html(html: &str, blueprint_name: Option<&str>, output_path: Option<&str>) -> Value {
    let has_data_umg = html.contains("data-umg-type");
    match parse_data_umg(html) {
        Ok(Some(mut tree)) => {
            if let Some(n) = blueprint_name {
                tree["blueprint_name"] = json!(n);
            }
            if let Some(p) = output_path {
                tree["output_path"] = json!(p);
            }
            return tree;
        }
        Ok(None) if has_data_umg => {
            return json!({
                "schema_version": "1.0",
                "status": "error",
                "message": "检测到 data-umg-type，但未能解析出任何节点（不支持自闭合标签；需要成对开闭标签与必填属性 type/name/x/y/w/h/anchor）",
                "widgets": []
            });
        }
        Ok(None) => {}
        Err(message) => {
            return json!({
                "schema_version": "1.0",
                "status": "error",
                "message": message,
                "widgets": []
            });
        }
    }

    if !looks_like_hud(html) {
        return json!({
            "schema_version": "1.0",
            "status": "error",
            "message": "未识别为 HUD mockup（需要 .hud / .res-panel 或 data-umg-type）",
            "widgets": []
        });
    }

    let texts = extract_texts(html);
    let mut tree = build_hud_mockup(&texts);
    if let Some(n) = blueprint_name {
        tree["blueprint_name"] = json!(n);
    }
    if let Some(p) = output_path {
        tree["output_path"] = json!(p);
    }
    tree
}

pub fn analyze_html_file(
    path: &Path,
    blueprint_name: Option<&str>,
    output_path: Option<&str>,
) -> Value {
    match fs::read_to_string(path) {
        Ok(html) => {
            let mut tree = analyze_html(&html, blueprint_name, output_path);
            tree["source_path"] = json!(path.to_string_lossy());
            tree
        }
        Err(e) => json!({
            "status": "error",
            "message": format!("读取 HTML 失败: {e}")
        }),
    }
}

pub fn capabilities() -> Value {
    json!({
        "tools": [
            "analyze_html_layout",
            "generate_umg_from_html",
            "generate_umg_from_json",
            "generate_umg_widget",
            "list_html_umg_capabilities"
        ],
        "integrated_into": "unreal-mcp-server",
        "unreal_method": "generate_umg_widget",
        "design_canvas": { "width": DESIGN_W, "height": DESIGN_H },
        "anchors": [
            "top-left", "top-right", "bottom-left", "bottom-right", "fill", "center"
        ],
        "data_umg_required": ["type", "name", "x", "y", "w", "h", "anchor"],
        "data_umg_optional": ["text", "visibility", "z"],
        "visibility": [
            "visible", "collapsed", "hidden", "hit-test-invisible", "self-hit-test-invisible"
        ],
        "replace_existing_asset": true,
        "profiles": ["hud-html-mockup", "data-umg-*"]
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn find_widget<'a>(tree: &'a Value, name: &str) -> &'a Value {
        fn visit<'a>(widget: &'a Value, name: &str) -> Option<&'a Value> {
            if widget["name"].as_str() == Some(name) {
                return Some(widget);
            }
            widget["children"]
                .as_array()
                .and_then(|children| children.iter().find_map(|child| visit(child, name)))
        }

        tree["widgets"]
            .as_array()
            .expect("widgets array")
            .iter()
            .find_map(|widget| visit(widget, name))
            .unwrap_or_else(|| panic!("missing widget {name}"))
    }

    #[test]
    fn analyze_hud_html_mockup_index() {
        let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join("../../../../Art/workspace/hud-html-mockup/index.html");
        let tree = analyze_html_file(&path, None, None);
        let pretty = serde_json::to_string_pretty(&tree).unwrap_or_else(|_| tree.to_string());
        println!("{pretty}");
        assert_eq!(
            tree["status"].as_str(),
            Some("success"),
            "parse failed: {}",
            tree["message"]
        );
        let widgets = tree["widgets"].as_array().expect("widgets array");
        assert!(!widgets.is_empty(), "expected root widget");
        assert_eq!(tree["canvas_width"].as_f64(), Some(1920.0));
        assert_eq!(tree["canvas_height"].as_f64(), Some(1080.0));
        let children = widgets[0]["children"].as_array().expect("root children");
        assert!(
            children.len() > 10,
            "expected many HUD widgets, got {}",
            children.len()
        );
        assert_eq!(find_widget(&tree, "BtnEndTurn")["text"], "建立主城");
        assert_eq!(
            find_widget(&tree, "BtnForceEndTurn")["x"].as_f64(),
            Some(1566.0)
        );
        assert_eq!(
            find_widget(&tree, "BtnQuickSettings")["x"].as_f64(),
            Some(1554.0)
        );
        assert_eq!(
            find_widget(&tree, "BtnQuickPause")["x"].as_f64(),
            Some(1412.0)
        );
        assert_eq!(
            find_widget(&tree, "ForceEndConfirmRoot")["visibility"],
            "collapsed"
        );
        assert_eq!(find_widget(&tree, "BtnResourceToggle")["text"], "收起 ▾");
        assert_eq!(
            find_widget(&tree, "ContextDockRoot")["anchors"],
            "bottom-left"
        );
        assert_eq!(find_widget(&tree, "BtnExpand")["visibility"], "collapsed");
        assert_eq!(
            find_widget(&tree, "BtnRecruitLingZhiFu")["visibility"],
            "collapsed"
        );
        assert_eq!(
            find_widget(&tree, "RootCanvas")["visibility"],
            "self-hit-test-invisible"
        );
    }

    #[test]
    fn parses_explicit_data_umg_contract() {
        let html = r##"
            <button
                data-umg-type="Button"
                data-umg-name="BtnConfirm"
                data-umg-anchor="bottom-right"
                data-umg-x="1700"
                data-umg-y="1000"
                data-umg-w="180"
                data-umg-h="48"
                data-umg-visibility="collapsed"
                data-umg-z="7"
                data-umg-color="#f5ebd7"
                data-umg-bg="#a05a46"
                data-umg-font-size="24"
            >确认</button>
        "##;
        let tree = analyze_html(html, Some("WBP_Test"), Some("/Game/UI/Test"));
        let button = find_widget(&tree, "BtnConfirm");
        assert_eq!(tree["status"], "success");
        assert_eq!(tree["blueprint_name"], "WBP_Test");
        assert_eq!(tree["output_path"], "/Game/UI/Test");
        assert_eq!(button["anchors"], "bottom-right");
        assert_eq!(button["text"], "确认");
        assert_eq!(button["visibility"], "collapsed");
        assert_eq!(button["z_order"], 7);
        assert_eq!(button["style"]["color"], "#f5ebd7");
        assert_eq!(button["style"]["background_color"], "#a05a46");
        assert_eq!(button["style"]["font_size"], 24);
        assert_eq!(
            find_widget(&tree, "RootCanvas")["visibility"],
            "self-hit-test-invisible"
        );
    }

    #[test]
    fn malformed_self_closing_data_umg_errors() {
        let html = r#"<div class="hud"></div><img data-umg-type="Image" data-umg-name="Bg" />"#;
        let tree = analyze_html(html, None, None);
        assert_eq!(tree["status"], "error");
        assert!(
            tree["message"]
                .as_str()
                .unwrap_or("")
                .contains("data-umg-type"),
            "unexpected message: {}",
            tree["message"]
        );
    }

    #[test]
    fn data_umg_missing_required_attrs_errors() {
        let html = r#"<div data-umg-type="Button">确认</div>"#;
        let tree = analyze_html(html, None, None);
        assert_eq!(tree["status"], "error");
        assert!(
            tree["message"]
                .as_str()
                .unwrap_or("")
                .contains("必填属性"),
            "unexpected message: {}",
            tree["message"]
        );
    }
}
