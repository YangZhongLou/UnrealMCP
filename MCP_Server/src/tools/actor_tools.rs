use rmcp::model::*;
use rmcp::schemars;
use rmcp::server::tool;
use serde_json::json;
use crate::unreal_client::UnrealClient;
use std::sync::Arc;
use tokio::sync::Mutex;

pub struct ActorTools {
    client: Arc<Mutex<UnrealClient>>,
}

impl ActorTools {
    pub fn new(client: Arc<Mutex<UnrealClient>>) -> Self {
        Self { client }
    }

    #[tool(description = "Spawn an actor in the Unreal Engine scene")]
    async fn spawn_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor class name, e.g. 'StaticMeshActor', 'PointLight'")]
        class_name: String,
        #[tool(param)]
        #[schemars(description = "Optional actor name")]
        name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional location [x, y, z]")]
        location: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional rotation [pitch, yaw, roll]")]
        rotation: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional scale [x, y, z]")]
        scale: Option<Vec<f64>>,
    ) -> Result<CallToolResult, McpError> {
        let mut params = json!({"className": class_name});
        if let Some(n) = name {
            params["name"] = json!(n);
        }
        if let Some(loc) = location {
            params["location"] = json!(loc);
        }
        if let Some(rot) = rotation {
            params["rotation"] = json!(rot);
        }
        if let Some(s) = scale {
            params["scale"] = json!(s);
        }

        let mut client = self.client.lock().await;
        match client.send_command("spawn_actor", params).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    format!("Spawned actor: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Destroy an actor by name")]
    async fn destroy_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name to destroy")]
        name: String,
    ) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("destroy_actor", json!({"name": name})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    format!("Destroyed actor: {}", name)
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }

    #[tool(description = "Get list of all actors in the current scene")]
    async fn get_actor_list(&self) -> Result<CallToolResult, McpError> {
        let mut client = self.client.lock().await;
        match client.send_command("get_actor_list", json!({})).await {
            Ok(response) => {
                let text = if response["success"].as_bool().unwrap_or(false) {
                    format!("Actors: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                };
                Ok(CallToolResult::success(vec![Content::text(text)]))
            }
            Err(e) => Ok(CallToolResult::success(vec![Content::text(format!("Error: {}", e))])),
        }
    }
}
