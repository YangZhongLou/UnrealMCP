use rmcp::model::*;
use rmcp::schemars;
use rmcp::server::tool;
use serde_json::json;
use crate::unreal_client::UnrealClient;
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct EditorTools {
    client: Arc<Mutex<UnrealClient>>,
}

impl EditorTools {
    pub fn new(client: Arc<Mutex<UnrealClient>>) -> Self {
        Self { client }
    }

    #[tool(description = "Run a console command in Unreal Engine")]
    async fn run_console_command(
        &self,
        #[tool(param)]
        #[schemars(description = "Console command string")]
        command: String,
    ) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("run_console_command", json!({"command": command})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    "Command executed successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Save the current level")]
    async fn save_current_level(&self) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("save_current_level", json!({})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    "Level saved successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Start Play In Editor (PIE)")]
    async fn play_in_editor(&self) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("play_in_editor", json!({})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    "Started Play In Editor".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Stop Play In Editor (PIE)")]
    async fn stop_play_in_editor(&self) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("stop_play_in_editor", json!({})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    "Stopped Play In Editor".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Get Unreal Editor information")]
    async fn get_editor_info(&self) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("get_editor_info", json!({})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    format!("Editor Info: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }
}
