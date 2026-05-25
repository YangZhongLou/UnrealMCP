from mcp.server.fastmcp import FastMCP
from client.unreal_client import UnrealClient


def register_editor_tools(mcp: FastMCP, client: UnrealClient):

    @mcp.tool()
    async def run_console_command(command: str) -> str:
        result = client.send_command("run_console_command", {"command": command})
        if result.get("success"):
            return f"Command executed: {result.get('result', 'OK')}"
        return f"Command failed: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def save_current_level() -> str:
        result = client.send_command("save_current_level", {})
        if result.get("success"):
            return "Level saved successfully"
        return f"Failed to save level: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def play_in_editor() -> str:
        result = client.send_command("play_in_editor", {})
        if result.get("success"):
            return "Started Play In Editor"
        return f"Failed: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def stop_play_in_editor() -> str:
        result = client.send_command("stop_play_in_editor", {})
        if result.get("success"):
            return "Stopped Play In Editor"
        return f"Failed: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def get_editor_info() -> str:
        result = client.send_command("get_editor_info", {})
        if result.get("success"):
            info = result.get("result", {})
            return f"Unreal Editor Info: {info}"
        return f"Failed: {result.get('error', 'Unknown error')}"
