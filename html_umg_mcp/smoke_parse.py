#!/usr/bin/env python3
"""Smoke: parse a HUD HTML file and print a short summary."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from parser import analyze_html_file


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: python smoke_parse.py <html_path>")
        return 2
    tree = analyze_html_file(sys.argv[1])
    print(json.dumps(
        {
            "status": tree.get("status"),
            "message": tree.get("message"),
            "blueprint_name": tree.get("blueprint_name"),
            "child_count": len(tree.get("widgets", [{}])[0].get("children", [])),
        },
        ensure_ascii=False,
        indent=2,
    ))
    out = Path(sys.argv[1]).with_suffix(".umg.json")
    if "--dump" in sys.argv:
        out.write_text(json.dumps(tree, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"dumped: {out}")
    return 0 if tree.get("status") != "error" else 1


if __name__ == "__main__":
    raise SystemExit(main())
