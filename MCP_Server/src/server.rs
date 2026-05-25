use rmcp::{
    ServerHandler,
    model::{ServerCapabilities, ServerInfo},
    tool,
};
use serde_json::json;
use std::sync::Arc;
use tokio::sync::Mutex;

use crate::unreal_client::UnrealClient;

#[derive(Debug, Clone)]
pub struct UnrealMcpServer {
    client: Arc<Mutex<UnrealClient>>,
}

impl UnrealMcpServer {
    pub async fn new(addr: &str) -> anyhow::Result<Self> {
        let client = Arc::new(Mutex::new(UnrealClient::new(addr)));
        Ok(Self { client })
    }
}

#[tool(tool_box)]
impl UnrealMcpServer {
    #[tool(description = "Check connection to Unreal Engine")]
    async fn check_unreal_connection(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_editor_info", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Connected to Unreal: {}", response["result"])
                } else {
                    "Unreal not responding".to_string()
                }
            }
            Err(_) => "Not connected to Unreal Engine. Please ensure the Unreal Editor is running with the UnrealMCP plugin loaded.".to_string(),
        }
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
    ) -> String {
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
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Spawned actor: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Destroy an actor by name")]
    async fn destroy_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name to destroy")]
        name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("destroy_actor", json!({"name": name})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Destroyed actor: {}", name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set actor transform (location, rotation, scale)")]
    async fn set_actor_transform(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        name: String,
        #[tool(param)]
        #[schemars(description = "Optional location [x, y, z]")]
        location: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional rotation [pitch, yaw, roll]")]
        rotation: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional scale [x, y, z]")]
        scale: Option<Vec<f64>>,
    ) -> String {
        let mut params = json!({"name": name});
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
        match client.send_command("set_actor_transform", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Transform updated".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get list of all actors in the current scene")]
    async fn get_actor_list(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_actor_list", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Actors: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Run a console command in Unreal Engine")]
    async fn run_console_command(
        &self,
        #[tool(param)]
        #[schemars(description = "Console command string")]
        command: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("run_console_command", json!({"command": command})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Command executed successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Save the current level")]
    async fn save_current_level(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("save_current_level", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Level saved successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Start Play In Editor (PIE)")]
    async fn play_in_editor(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("play_in_editor", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Started Play In Editor".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Stop Play In Editor (PIE)")]
    async fn stop_play_in_editor(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("stop_play_in_editor", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Stopped Play In Editor".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get Unreal Editor information")]
    async fn get_editor_info(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_editor_info", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Editor Info: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Create a new Blueprint")]
    async fn create_blueprint(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint name")]
        name: String,
        #[tool(param)]
        #[schemars(description = "Parent class name, e.g. 'Actor', 'Pawn'")]
        parent_class: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional path, default '/Game/Blueprints'")]
        path: Option<String>,
    ) -> String {
        let mut params = json!({"name": name});
        if let Some(pc) = parent_class {
            params["parentClass"] = json!(pc);
        }
        if let Some(p) = path {
            params["path"] = json!(p);
        }

        let mut client = self.client.lock().await;
        match client.send_command("create_blueprint", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Blueprint created: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Compile a Blueprint")]
    async fn compile_blueprint(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("compile_blueprint", json!({"path": path})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Blueprint compiled successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get Blueprint information")]
    async fn get_blueprint_info(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_blueprint_info", json!({"path": path})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Blueprint Info: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "List assets in a path")]
    async fn get_asset_list(
        &self,
        #[tool(param)]
        #[schemars(description = "Asset path, default '/Game'")]
        path: Option<String>,
    ) -> String {
        let params = if let Some(p) = path {
            json!({"path": p})
        } else {
            json!({})
        };

        let mut client = self.client.lock().await;
        match client.send_command("get_asset_list", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Assets: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get asset information")]
    async fn get_asset_info(
        &self,
        #[tool(param)]
        #[schemars(description = "Asset path")]
        path: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_asset_info", json!({"path": path})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Asset Info: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Delete an asset")]
    async fn delete_asset(
        &self,
        #[tool(param)]
        #[schemars(description = "Asset path")]
        path: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("delete_asset", json!({"path": path})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Asset deleted".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Rename an asset")]
    async fn rename_asset(
        &self,
        #[tool(param)]
        #[schemars(description = "Asset path")]
        path: String,
        #[tool(param)]
        #[schemars(description = "New name")]
        new_name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("rename_asset", json!({"path": path, "newName": new_name})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Asset renamed".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set an actor property value")]
    async fn set_actor_property(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Property name, e.g. 'Intensity', 'LightColor'")]
        property_name: String,
        #[tool(param)]
        #[schemars(description = "Property value (number, string, or array)")]
        value: serde_json::Value,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("set_actor_property", json!({
            "actorName": actor_name,
            "propertyName": property_name,
            "value": value
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Property set successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get an actor property value")]
    async fn get_actor_property(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Property name")]
        property_name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_actor_property", json!({
            "actorName": actor_name,
            "propertyName": property_name
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Property value: {}", response["result"]["value"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Duplicate an actor by name")]
    async fn duplicate_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name to duplicate")]
        name: String,
        #[tool(param)]
        #[schemars(description = "Optional new name for the duplicate")]
        new_name: Option<String>,
    ) -> String {
        let mut params = json!({"name": name});
        if let Some(n) = new_name {
            params["newName"] = json!(n);
        }

        let mut client = self.client.lock().await;
        match client.send_command("duplicate_actor", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Duplicated actor: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Open a level by path")]
    async fn open_level(
        &self,
        #[tool(param)]
        #[schemars(description = "Level path, e.g. '/Game/Maps/MyMap'")]
        path: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("open_level", json!({"path": path})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Level opened successfully".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Take a screenshot of the current viewport")]
    async fn take_screenshot(
        &self,
        #[tool(param)]
        #[schemars(description = "Optional filename, default 'screenshot'")]
        filename: Option<String>,
    ) -> String {
        let mut params = json!({});
        if let Some(f) = filename {
            params["filename"] = json!(f);
        }

        let mut client = self.client.lock().await;
        match client.send_command("take_screenshot", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Screenshot saved: {}", response["result"]["path"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Generate a C++ class template")]
    async fn generate_cpp_class(
        &self,
        #[tool(param)]
        #[schemars(description = "Class name")]
        class_name: String,
        #[tool(param)]
        #[schemars(description = "Parent class, e.g. 'Actor', 'Character'")]
        parent_class: String,
        #[tool(param)]
        #[schemars(description = "Optional module name")]
        module: Option<String>,
    ) -> String {
        let mut params = json!({
            "className": class_name,
            "parentClass": parent_class
        });
        if let Some(m) = module {
            params["module"] = json!(m);
        }

        let mut client = self.client.lock().await;
        match client.send_command("generate_cpp_class", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Generated C++ class: {}", response["result"]["path"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get information about the currently open level")]
    async fn get_current_level(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_current_level", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Level: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get all components attached to an actor")]
    async fn get_actor_components(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_actor_components", json!({"actorName": actor_name})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Components: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Add a component to an actor")]
    async fn add_component(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name to add the component to")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Component class name, e.g. 'StaticMeshComponent', 'PointLightComponent'")]
        component_class: String,
        #[tool(param)]
        #[schemars(description = "Optional name for the new component")]
        component_name: Option<String>,
    ) -> String {
        let mut params = json!({
            "actorName": actor_name,
            "componentClass": component_class
        });
        if let Some(n) = component_name {
            params["componentName"] = json!(n);
        }

        let mut client = self.client.lock().await;
        match client.send_command("add_component", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Component added: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Remove a component from an actor by component name")]
    async fn remove_component(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Component name to remove")]
        component_name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("remove_component", json!({
            "actorName": actor_name,
            "componentName": component_name
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Component removed".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Focus the viewport camera on an actor or location")]
    async fn focus_viewport(
        &self,
        #[tool(param)]
        #[schemars(description = "Optional actor name to focus on")]
        actor_name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional location [x, y, z] to focus on")]
        location: Option<Vec<f64>>,
    ) -> String {
        let mut params = json!({});
        if let Some(n) = actor_name {
            params["actorName"] = json!(n);
        }
        if let Some(loc) = location {
            params["location"] = json!(loc);
        }

        let mut client = self.client.lock().await;
        match client.send_command("focus_viewport", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Viewport focused".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }
}

#[tool(tool_box)]
impl ServerHandler for UnrealMcpServer {
    fn get_info(&self) -> ServerInfo {
        ServerInfo {
            instructions: Some("Unreal Engine MCP Server - Control Unreal Editor via AI".into()),
            capabilities: ServerCapabilities::builder().enable_tools().build(),
            ..Default::default()
        }
    }
}
