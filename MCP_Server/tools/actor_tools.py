from mcp.server.fastmcp import FastMCP
from client.unreal_client import UnrealClient


def register_actor_tools(mcp: FastMCP, client: UnrealClient):

    @mcp.tool()
    async def spawn_actor(
        class_name: str,
        name: str = "",
        location: list = None,
        rotation: list = None,
        scale: list = None
    ) -> str:
        params = {"className": class_name}
        if name:
            params["name"] = name
        if location:
            params["location"] = location
        if rotation:
            params["rotation"] = rotation
        if scale:
            params["scale"] = scale

        result = client.send_command("spawn_actor", params)
        if result.get("success"):
            return f"Spawned actor: {result.get('result', {})}"
        return f"Failed to spawn actor: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def destroy_actor(name: str) -> str:
        result = client.send_command("destroy_actor", {"name": name})
        if result.get("success"):
            return f"Destroyed actor: {name}"
        return f"Failed to destroy actor: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def set_actor_transform(
        name: str,
        location: list = None,
        rotation: list = None,
        scale: list = None
    ) -> str:
        params = {"name": name}
        if location:
            params["location"] = location
        if rotation:
            params["rotation"] = rotation
        if scale:
            params["scale"] = scale

        result = client.send_command("set_actor_transform", params)
        if result.get("success"):
            return f"Transform updated for {name}"
        return f"Failed: {result.get('error', 'Unknown error')}"

    @mcp.tool()
    async def get_actor_list() -> str:
        result = client.send_command("get_actor_list", {})
        if result.get("success"):
            actors = result.get("result", [])
            return f"Actors in scene ({len(actors)}): {actors}"
        return f"Failed to get actor list: {result.get('error', 'Unknown error')}"
