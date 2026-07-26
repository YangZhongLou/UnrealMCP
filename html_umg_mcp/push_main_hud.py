#!/usr/bin/env python3
"""Push hud-html-mockup → UnrealMCP generate_umg_widget (局内 HUD 打通).

Requires: Unreal Editor open, UnrealMCP listening on UNREAL_MCP_ADDR (default 13377).
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import sys
from pathlib import Path

# Reuse local parser (same profile as Rust html_umg)
_ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(_ROOT))
from parser import analyze_html_file  # noqa: E402

REPO = Path(__file__).resolve().parents[4]
HTML = REPO / "Art" / "workspace" / "hud-html-mockup" / "index.html"
BP_NAME = "WBP_MainHud_FromHtml"
OUT_PATH = "/Game/UI/HUD"
MAIN_HUD_REQUIRED_WIDGETS = {
    "RootCanvas",
    "TxtResVal_0",
    "BtnResourceToggle",
    "TxtTurnValue",
    "TxtExploreValue",
    "ContextDockRoot",
    "TxtStatusTitle",
    "BtnStatusExec",
    "BtnEndTurn",
    "BtnForceEndTurn",
    "ForceEndConfirmRoot",
    "BtnForceEndConfirmCancel",
    "BtnForceEndConfirmOk",
    "BtnQuickSettings",
    "BtnQuickPause",
    "BtnRail_0",
    "BtnRail_1",
    "BtnRail_2",
    "BtnRail_3",
    "BtnRail_4",
    "BtnRail_5",
    "BtnRail_6",
    "BtnChipFound",
    "BtnExpand",
    "BtnCultivateFanTian",
    "BtnFanTianHerb",
    "BtnFanTianGrain",
    "BtnUpgradeLingTian",
    "BtnBuild",
    "BtnClearBeastLair",
    "BtnGarrisonBeast",
    "BtnUngarrisonBeast",
    "BtnGather",
    "BtnAttack",
    "BtnFortify",
    "BtnHuiChun",
    "BtnChuanLin",
    "BtnGuWu",
    "BtnCuiShu",
    "BtnChunHuo",
    "BtnReinforceVein",
    "BtnRecruitCaiYao",
    "BtnRecruitZhanTang",
    "BtnRecruitZhenCha",
    "BtnRecruitKuangGong",
    "BtnRecruitHuFa",
    "BtnRecruitYiShi",
    "BtnRecruitGuShi",
    "BtnRecruitLianDanShi",
    "BtnRecruitLingZhiFu",
}


def _addr() -> tuple[str, int]:
    raw = os.environ.get("UNREAL_MCP_ADDR", "127.0.0.1:13377")
    host, _, port = raw.partition(":")
    return host or "127.0.0.1", int(port or "13377")


def send(method: str, params: dict, timeout: float = 120.0) -> dict:
    host, port = _addr()
    req = json.dumps(
        {"id": f"push_main_hud_{os.getpid()}", "method": method, "params": params},
        ensure_ascii=False,
    ) + "\n"
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((host, port))
        sock.sendall(req.encode("utf-8"))
        buf = b""
        while b"\n\n" not in buf:
            chunk = sock.recv(8192)
            if not chunk:
                break
            buf += chunk
        text = buf.decode("utf-8", errors="replace").split("\n\n", 1)[0].strip()
        return json.loads(text)
    finally:
        sock.close()


def _widget_names(tree: dict) -> set[str]:
    names: set[str] = set()

    def visit(widget: dict) -> None:
        name = widget.get("name")
        if isinstance(name, str) and name:
            names.add(name)
        for child in widget.get("children", []):
            if isinstance(child, dict):
                visit(child)

    for widget in tree.get("widgets", []):
        if isinstance(widget, dict):
            visit(widget)
    return names


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyze an annotated HTML mockup and generate a UMG Widget Blueprint."
    )
    parser.add_argument("--html", type=Path, default=HTML, help="HTML mockup path")
    parser.add_argument("--blueprint-name", default=BP_NAME, help="Generated WBP asset name")
    parser.add_argument("--output-path", default=OUT_PATH, help="Unreal package directory under /Game")
    parser.add_argument("--dump", type=Path, help="Optional analyzed widget-tree JSON path")
    parser.add_argument(
        "--require-widget",
        action="append",
        default=[],
        help="Required widget name; repeat for multiple contract checks",
    )
    parser.add_argument(
        "--analyze-only",
        action="store_true",
        help="Validate and optionally dump the tree without connecting to Unreal",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    html_path = args.html.resolve()
    if not html_path.is_file():
        print(json.dumps({"status": "error", "message": f"missing {html_path}"}, ensure_ascii=False))
        return 2
    if not args.output_path.startswith("/Game/") or "/" in args.blueprint_name:
        print(
            json.dumps(
                {
                    "status": "error",
                    "message": (
                        "output-path must start with /Game/ and blueprint-name must not contain '/'"
                    ),
                },
                ensure_ascii=False,
            )
        )
        return 2

    tree = analyze_html_file(
        str(html_path),
        blueprint_name=args.blueprint_name,
        output_path=args.output_path,
    )
    if tree.get("status") == "error" or not tree.get("widgets"):
        print(json.dumps(tree, ensure_ascii=False, indent=2))
        return 1

    required = set(args.require_widget)
    if html_path == HTML.resolve() and args.blueprint_name == BP_NAME:
        required.update(MAIN_HUD_REQUIRED_WIDGETS)
    missing = sorted(required - _widget_names(tree))
    if missing:
        print(
            json.dumps(
                {
                    "status": "error",
                    "message": "widget contract missing required names",
                    "missing": missing,
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        return 5

    dump = args.dump
    if dump is None and html_path == HTML.resolve() and args.blueprint_name == BP_NAME:
        dump = _ROOT / "_last_main_hud_tree.json"
    if dump:
        dump = dump.resolve()
        dump.parent.mkdir(parents=True, exist_ok=True)
        dump.write_text(json.dumps(tree, ensure_ascii=False, indent=2), encoding="utf-8")
    print(
        "parsed: "
        f"children={len(tree['widgets'][0].get('children', []))} "
        f"required={len(required)} "
        f"dump={dump or 'disabled'}"
    )
    if args.analyze_only:
        print(json.dumps({"status": "ok", "analysis_only": True}, ensure_ascii=False))
        return 0

    try:
        resp = send("generate_umg_widget", {"widget_tree": tree})
    except OSError as exc:
        print(
            json.dumps(
                {
                    "status": "error",
                    "message": f"无法连接 UnrealMCP: {exc}",
                    "hint": "请先打开 ClanSimulator Editor，确认端口 13377",
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        return 3

    print(
        json.dumps(
            {"status": "ok" if resp.get("success") else "error", "unreal": resp},
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0 if resp.get("success") else 4


if __name__ == "__main__":
    raise SystemExit(main())
