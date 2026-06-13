#!/usr/bin/env python3
import asyncio
import logging

from mcp.server.fastmcp import FastMCP

from client.unreal_client import UnrealClient
from tools.actor_tools import register_actor_tools
from tools.editor_tools import register_editor_tools

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
)
logger = logging.getLogger(__name__)

mcp = FastMCP("UnrealMCP")
unreal_client = UnrealClient(host="127.0.0.1", port=13378)


def register_all_tools():
    register_actor_tools(mcp, unreal_client)
    register_editor_tools(mcp, unreal_client)
    logger.info("All tools registered")


if __name__ == "__main__":
    register_all_tools()
    logger.info("Starting Unreal MCP Server...")
    mcp.run(transport="stdio")
