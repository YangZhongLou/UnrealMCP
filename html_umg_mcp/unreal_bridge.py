"""TCP bridge to UnrealMCP CommandServer (default 127.0.0.1:13377)."""

from __future__ import annotations

import json
import os
import socket
from typing import Any


def _addr() -> tuple[str, int]:
    raw = os.environ.get("UNREAL_MCP_ADDR", "127.0.0.1:13377")
    if ":" in raw:
        host, port_s = raw.rsplit(":", 1)
        return host, int(port_s)
    return raw, 13377


def send_command(method: str, params: dict[str, Any], timeout: float = 60.0) -> dict[str, Any]:
    host, port = _addr()
    req = {
        "id": f"html_umg_{os.getpid()}_{method}",
        "method": method,
        "params": params,
    }
    payload = (json.dumps(req, ensure_ascii=False) + "\n").encode("utf-8")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((host, port))
        sock.sendall(payload)
        buf = b""
        while b"\n\n" not in buf:
            chunk = sock.recv(8192)
            if not chunk:
                break
            buf += chunk
        text = buf.decode("utf-8", errors="replace")
        json_str = text.split("\n\n", 1)[0].strip()
        return json.loads(json_str)
    except OSError as exc:
        return {"success": False, "error": f"无法连接 UnrealMCP ({host}:{port}): {exc}"}
    finally:
        try:
            sock.close()
        except OSError:
            pass
