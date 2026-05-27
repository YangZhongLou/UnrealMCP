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

    #[tool(description = "Get list of currently selected actors in the editor")]
    async fn get_selected_actors(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_selected_actors", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Selected: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Select an actor by name in the editor")]
    async fn select_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name to select")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "If true, add to existing selection; default false (replace)")]
        add_to_selection: Option<bool>,
    ) -> String {
        let mut params = json!({"actorName": actor_name});
        if let Some(a) = add_to_selection {
            params["addToSelection"] = json!(a);
        }

        let mut client = self.client.lock().await;
        match client.send_command("select_actor", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Selected: {}", actor_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set the static mesh on a StaticMeshComponent")]
    async fn set_static_mesh(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name that has the StaticMeshComponent")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Static mesh asset path, e.g. '/Engine/BasicShapes/Cube.Cube'")]
        mesh_path: String,
        #[tool(param)]
        #[schemars(description = "Optional component name, defaults to first StaticMeshComponent found")]
        component_name: Option<String>,
    ) -> String {
        let mut params = json!({
            "actorName": actor_name,
            "meshPath": mesh_path
        });
        if let Some(c) = component_name {
            params["componentName"] = json!(c);
        }

        let mut client = self.client.lock().await;
        match client.send_command("set_static_mesh", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Static mesh set on {}", actor_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Apply a material to a component on an actor")]
    async fn set_material(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Material asset path, e.g. '/Game/Materials/MyMat.MyMat'")]
        material_path: String,
        #[tool(param)]
        #[schemars(description = "Optional component name, defaults to first mesh component")]
        component_name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional material slot index, default 0")]
        slot_index: Option<i32>,
    ) -> String {
        let mut params = json!({
            "actorName": actor_name,
            "materialPath": material_path
        });
        if let Some(c) = component_name {
            params["componentName"] = json!(c);
        }
        if let Some(s) = slot_index {
            params["slotIndex"] = json!(s);
        }

        let mut client = self.client.lock().await;
        match client.send_command("set_material", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Material applied to {}", actor_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Create a material instance from a parent material")]
    async fn create_material_instance(
        &self,
        #[tool(param)]
        #[schemars(description = "Asset path for the new material instance, e.g. '/Game/Materials/MI_MyMat'")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Parent material path, e.g. '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial'")]
        parent_path: String,
        #[tool(param)]
        #[schemars(description = "Optional: 'constant' (persistent asset, default) or 'dynamic' (runtime only)")]
        instance_type: Option<String>,
    ) -> String {
        let mut params = json!({
            "path": path,
            "parentPath": parent_path
        });
        if let Some(t) = instance_type {
            params["instanceType"] = json!(t);
        }

        let mut client = self.client.lock().await;
        match client.send_command("create_material_instance", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Material instance created: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set a parameter on a material instance")]
    async fn set_material_parameter(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name that has the material instance")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Parameter name")]
        parameter_name: String,
        #[tool(param)]
        #[schemars(description = "Optional scalar (float) value")]
        scalar_value: Option<f64>,
        #[tool(param)]
        #[schemars(description = "Optional vector [r, g, b] value (0.0-1.0)")]
        vector_value: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional component name")]
        component_name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional material slot index, default 0")]
        slot_index: Option<i32>,
    ) -> String {
        let mut params = json!({
            "actorName": actor_name,
            "parameterName": parameter_name
        });
        if let Some(s) = scalar_value {
            params["scalarValue"] = json!(s);
        }
        if let Some(v) = vector_value {
            params["vectorValue"] = json!(v);
        }
        if let Some(c) = component_name {
            params["componentName"] = json!(c);
        }
        if let Some(s) = slot_index {
            params["slotIndex"] = json!(s);
        }

        let mut client = self.client.lock().await;
        match client.send_command("set_material_parameter", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    "Material parameter set".to_string()
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Find actors by class name (supports partial match)")]
    async fn find_actors_by_class(
        &self,
        #[tool(param)]
        #[schemars(description = "Class name to search for, e.g. 'PointLight', 'StaticMeshActor'")]
        class_name: String,
        #[tool(param)]
        #[schemars(description = "If true, match exact class name; default false (substring match)")]
        exact_match: Option<bool>,
    ) -> String {
        let mut params = json!({"className": class_name});
        if let Some(e) = exact_match {
            params["exactMatch"] = json!(e);
        }

        let mut client = self.client.lock().await;
        match client.send_command("find_actors_by_class", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Found: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Spawn an actor from a Blueprint asset")]
    async fn spawn_blueprint_actor(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path, e.g. '/Game/Blueprints/BP_MyActor.BP_MyActor'")]
        blueprint_path: String,
        #[tool(param)]
        #[schemars(description = "Optional actor name")]
        name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional location [x, y, z]")]
        location: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional rotation [pitch, yaw, roll]")]
        rotation: Option<Vec<f64>>,
    ) -> String {
        let mut params = json!({"blueprintPath": blueprint_path});
        if let Some(n) = name {
            params["name"] = json!(n);
        }
        if let Some(loc) = location {
            params["location"] = json!(loc);
        }
        if let Some(rot) = rotation {
            params["rotation"] = json!(rot);
        }

        let mut client = self.client.lock().await;
        match client.send_command("spawn_blueprint_actor", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Spawned: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Simulate a keyboard key press or release")]
    async fn simulate_key(
        &self,
        #[tool(param)]
        #[schemars(description = "Key name, e.g. 'W', 'SpaceBar', 'Enter', 'LeftMouseButton'")]
        key: String,
        #[tool(param)]
        #[schemars(description = "Key action: 'press', 'release', or 'tap' (default)")]
        action: Option<String>,
    ) -> String {
        let mut params = json!({"key": key});
        if let Some(a) = action {
            params["action"] = json!(a);
        }

        let mut client = self.client.lock().await;
        match client.send_command("simulate_key", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Key simulated: {}", key)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get the current viewport camera position and rotation")]
    async fn get_viewport_camera(&self) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("get_viewport_camera", json!({})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Camera: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set parameters on a light actor (intensity, color, shadows)")]
    async fn set_light_parameters(
        &self,
        #[tool(param)]
        #[schemars(description = "Light actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Optional light intensity")]
        intensity: Option<f64>,
        #[tool(param)]
        #[schemars(description = "Optional light color [r, g, b] (0.0-1.0)")]
        color: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional cast shadows flag")]
        cast_shadows: Option<bool>,
    ) -> String {
        let mut params = json!({"actorName": actor_name});
        if let Some(i) = intensity { params["intensity"] = json!(i); }
        if let Some(c) = color { params["color"] = json!(c); }
        if let Some(s) = cast_shadows { params["castShadows"] = json!(s); }

        let mut client = self.client.lock().await;
        match client.send_command("set_light_parameters", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Light parameters set on {}", actor_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Spawn a Niagara/Cascade particle effect at a location")]
    async fn spawn_effect(
        &self,
        #[tool(param)]
        #[schemars(description = "Particle system asset path, e.g. '/Game/FX/PS_Fire.PS_Fire'")]
        asset_path: String,
        #[tool(param)]
        #[schemars(description = "Optional spawn location [x, y, z], default origin")]
        location: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "Optional rotation [pitch, yaw, roll]")]
        rotation: Option<Vec<f64>>,
        #[tool(param)]
        #[schemars(description = "If true, auto-destroy when finished; default true")]
        auto_destroy: Option<bool>,
    ) -> String {
        let mut params = json!({"assetPath": asset_path});
        if let Some(loc) = location { params["location"] = json!(loc); }
        if let Some(rot) = rotation { params["rotation"] = json!(rot); }
        if let Some(a) = auto_destroy { params["autoDestroy"] = json!(a); }

        let mut client = self.client.lock().await;
        match client.send_command("spawn_effect", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Effect spawned: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Add a tag to an actor")]
    async fn add_actor_tag(
        &self,
        #[tool(param)]
        #[schemars(description = "Actor name")]
        actor_name: String,
        #[tool(param)]
        #[schemars(description = "Tag to add")]
        tag: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("add_actor_tag", json!({
            "actorName": actor_name,
            "tag": tag
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Tag '{}' added to {}", tag, actor_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Set the editor viewport render mode")]
    async fn set_view_mode(
        &self,
        #[tool(param)]
        #[schemars(description = "View mode: Lit, Unlit, Wireframe, ShaderComplexity, etc.")]
        mode: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("set_view_mode", json!({"mode": mode})).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("View mode set to: {}", mode)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Toggle debug visualization (collision, navigation, bounds, etc.)")]
    async fn show_debug(
        &self,
        #[tool(param)]
        #[schemars(description = "Debug flag: collision, navigation, bones, bounds, skeletalmeshes")]
        flag: String,
        #[tool(param)]
        #[schemars(description = "Optional: true to show, false to hide; toggles if omitted")]
        enable: Option<bool>,
    ) -> String {
        let mut params = json!({"flag": flag});
        if let Some(e) = enable { params["enable"] = json!(e); }

        let mut client = self.client.lock().await;
        match client.send_command("show_debug", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Debug flag '{}' set", flag)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Add a node to a Blueprint graph (EventGraph or function graph)")]
    async fn add_blueprint_node(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path, e.g. '/Game/Blueprints/BP_MyActor.BP_MyActor'")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Node type: CallFunction, Event, CustomEvent, VariableGet, VariableSet, PrintString")]
        node_type: String,
        #[tool(param)]
        #[schemars(description = "Function name (for CallFunction), event name (for Event/CustomEvent), or variable name (for VariableGet/Set)")]
        name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional class name to search for the function; auto-searched if omitted")]
        class_name: Option<String>,
        #[tool(param)]
        #[schemars(description = "Graph type, default 'EventGraph'. Use function name for function graphs")]
        graph_type: Option<String>,
        #[tool(param)]
        #[schemars(description = "Optional X position in graph")]
        pos_x: Option<i32>,
        #[tool(param)]
        #[schemars(description = "Optional Y position in graph")]
        pos_y: Option<i32>,
    ) -> String {
        let mut params = json!({
            "path": path,
            "node_type": node_type,
        });
        if let Some(n) = name {
            match node_type.as_str() {
                "CallFunction" => { params["function_name"] = json!(n); }
                "Event" | "CustomEvent" => { params["event_name"] = json!(n); }
                "VariableGet" | "VariableSet" => { params["variable_name"] = json!(n); }
                _ => {}
            }
        }
        if let Some(c) = class_name { params["class_name"] = json!(c); }
        if let Some(g) = graph_type { params["graph_type"] = json!(g); }
        if let Some(x) = pos_x { params["pos_x"] = json!(x); }
        if let Some(y) = pos_y { params["pos_y"] = json!(y); }

        let mut client = self.client.lock().await;
        match client.send_command("add_blueprint_node", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Node added: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Connect two pins between nodes in a Blueprint graph")]
    async fn connect_blueprint_pins(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Source node ID (GUID returned by add_blueprint_node or get_blueprint_graph)")]
        source_node_id: String,
        #[tool(param)]
        #[schemars(description = "Source pin name, e.g. 'then', 'ReturnValue'")]
        source_pin: String,
        #[tool(param)]
        #[schemars(description = "Target node ID")]
        target_node_id: String,
        #[tool(param)]
        #[schemars(description = "Target pin name, e.g. 'execute', 'InString'")]
        target_pin: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("connect_blueprint_pins", json!({
            "path": path,
            "source_node_id": source_node_id,
            "source_pin": source_pin,
            "target_node_id": target_node_id,
            "target_pin": target_pin,
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Pins connected: {}[{}] -> {}[{}]", source_node_id, source_pin, target_node_id, target_pin)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Add a variable to a Blueprint")]
    async fn add_blueprint_variable(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Variable name")]
        variable_name: String,
        #[tool(param)]
        #[schemars(description = "Variable type: int, float, bool, string, name, text, Vector, Rotator, Transform, Color, or any UObject class like AActor")]
        variable_type: String,
        #[tool(param)]
        #[schemars(description = "Optional: true if array type")]
        is_array: Option<bool>,
    ) -> String {
        let mut params = json!({
            "path": path,
            "variable_name": variable_name,
            "variable_type": variable_type,
        });
        if let Some(a) = is_array { params["is_array"] = json!(a); }

        let mut client = self.client.lock().await;
        match client.send_command("add_blueprint_variable", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Variable added: {}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Remove a variable from a Blueprint")]
    async fn remove_blueprint_variable(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Variable name to remove")]
        variable_name: String,
    ) -> String {
        let mut client = self.client.lock().await;
        match client.send_command("remove_blueprint_variable", json!({
            "path": path,
            "variable_name": variable_name,
        })).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Variable removed: {}", variable_name)
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }

    #[tool(description = "Get the graph structure (nodes, pins, connections) of a Blueprint")]
    async fn get_blueprint_graph(
        &self,
        #[tool(param)]
        #[schemars(description = "Blueprint asset path")]
        path: String,
        #[tool(param)]
        #[schemars(description = "Graph type, default 'EventGraph'")]
        graph_type: Option<String>,
    ) -> String {
        let mut params = json!({"path": path});
        if let Some(g) = graph_type { params["graph_type"] = json!(g); }

        let mut client = self.client.lock().await;
        match client.send_command("get_blueprint_graph", params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("Graph: {}", response["result"])
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
