#!/usr/bin/env python3
"""DEPRECATED: use UnrealMCP Rust tools (analyze_html_layout / generate_umg_from_html).

This file remains only for emergency local debugging. Prefer MCP_Server.
"""

from __future__ import annotations

import sys

print(
    "html_umg_mcp/server.py is deprecated.\n"
    "Use the main UnrealMCP (unreal-mcp-server) tools:\n"
    "  analyze_html_layout / generate_umg_from_html / list_html_umg_capabilities\n"
    "See Plugins/UnrealMCP/docs/html-umg-mcp.md",
    file=sys.stderr,
)
sys.exit(2)
