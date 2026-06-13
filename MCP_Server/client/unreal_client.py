import json
import socket
import logging
from typing import Any, Optional

logger = logging.getLogger(__name__)


class UnrealClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 13378):
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None

    def connect(self) -> bool:
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(30.0)
            self.socket.connect((self.host, self.port))
            logger.info(f"Connected to Unreal at {self.host}:{self.port}")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.socket = None
            return False

    def disconnect(self):
        if self.socket:
            try:
                self.socket.close()
            except Exception:
                pass
            self.socket = None
            logger.info("Disconnected from Unreal")

    def send_command(self, method: str, params: dict) -> dict:
        """Send a command to port 13378 (MCPCommandServer) using simple JSON + newline protocol."""
        if not self.socket and not self.connect():
            return {"success": False, "error": "Not connected to Unreal Engine"}

        request = {
            "id": f"cmd_{method}",
            "method": method,
            "params": params
        }

        try:
            message = json.dumps(request) + "\n"
            self.socket.sendall(message.encode("utf-8"))

            response_data = b""
            while True:
                chunk = self.socket.recv(65536)
                if not chunk:
                    break
                response_data += chunk
                if b"\n" in response_data:
                    break

            if not response_data:
                return {"success": False, "error": "Empty response from Unreal"}

            return json.loads(response_data.decode("utf-8").strip())

        except socket.timeout:
            logger.warning("Command timeout, reconnecting...")
            self.disconnect()
            return {"success": False, "error": "Command timeout"}
        except Exception as e:
            logger.error(f"Command failed: {e}")
            self.disconnect()
            return {"success": False, "error": str(e)}

    def is_connected(self) -> bool:
        return self.socket is not None
