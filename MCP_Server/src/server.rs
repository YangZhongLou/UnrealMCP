use rmcp::{
    ServerHandler,
    model::{ServerCapabilities, ServerInfo},
    tool,
};
use serde_json::{json, Value};
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

    async fn call(&self, method: &str, params: Value) -> String {
        let mut client = self.client.lock().await;
        match client.send_command(method, params).await {
            Ok(response) => {
                if response["success"].as_bool().unwrap_or(false) {
                    format!("{}", response["result"])
                } else {
                    format!("Failed: {}", response["error"])
                }
            }
            Err(e) => format!("Error: {}", e),
        }
    }
}

#[tool(tool_box)]
impl UnrealMcpServer {
    // ── Connection ──

    #[tool(description = "Check connection to Unreal Engine")]
    async fn check_unreal_connection(&self) -> String {
        let r = self.call("get_editor_info", json!({})).await;
        if r.starts_with("Failed:") || r.starts_with("Error:") {
            "Not connected to Unreal Engine. Run the Unreal Editor with the UnrealMCP plugin.".into()
        } else {
            format!("Connected: {}", r)
        }
    }

    // ── Actor ──

    #[tool(description = "Spawn an actor in the Unreal Engine scene")]
    async fn spawn_actor(
        &self,
        #[tool(param)] #[schemars(description = "Actor class name, e.g. 'StaticMeshActor', 'PointLight'")]
        class_name: String,
        #[tool(param)] #[schemars(description = "Optional actor name")] name: Option<String>,
        #[tool(param)] #[schemars(description = "Optional location [x, y, z]")] location: Option<Vec<f64>>,
        #[tool(param)] #[schemars(description = "Optional rotation [pitch, yaw, roll]")] rotation: Option<Vec<f64>>,
        #[tool(param)] #[schemars(description = "Optional scale [x, y, z]")] scale: Option<Vec<f64>>,
    ) -> String {
        let mut p = json!({"className": class_name});
        if let Some(v) = name { p["name"] = json!(v); }
        if let Some(v) = location { p["location"] = json!(v); }
        if let Some(v) = rotation { p["rotation"] = json!(v); }
        if let Some(v) = scale { p["scale"] = json!(v); }
        self.call("spawn_actor", p).await
    }

    #[tool(description = "Destroy an actor by name")]
    async fn destroy_actor(&self, #[tool(param)] name: String) -> String {
        self.call("destroy_actor", json!({"name": name})).await
    }

    #[tool(description = "Set actor transform (location, rotation, scale)")]
    async fn set_actor_transform(
        &self,
        #[tool(param)] name: String,
        #[tool(param)] location: Option<Vec<f64>>,
        #[tool(param)] rotation: Option<Vec<f64>>,
        #[tool(param)] scale: Option<Vec<f64>>,
    ) -> String {
        let mut p = json!({"name": name});
        if let Some(v) = location { p["location"] = json!(v); }
        if let Some(v) = rotation { p["rotation"] = json!(v); }
        if let Some(v) = scale { p["scale"] = json!(v); }
        self.call("set_actor_transform", p).await
    }

    #[tool(description = "Get list of all actors in the current scene")]
    async fn get_actor_list(&self) -> String {
        self.call("get_actor_list", json!({})).await
    }

    #[tool(description = "Set an actor property value")]
    async fn set_actor_property(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] property_name: String,
        #[tool(param)] value: Value,
    ) -> String {
        self.call("set_actor_property", json!({"actorName": actor_name, "propertyName": property_name, "value": value})).await
    }

    #[tool(description = "Get an actor property value")]
    async fn get_actor_property(&self, #[tool(param)] actor_name: String, #[tool(param)] property_name: String) -> String {
        self.call("get_actor_property", json!({"actorName": actor_name, "propertyName": property_name})).await
    }

    #[tool(description = "Duplicate an actor by name")]
    async fn duplicate_actor(&self, #[tool(param)] name: String, #[tool(param)] new_name: Option<String>) -> String {
        let mut p = json!({"name": name});
        if let Some(v) = new_name { p["newName"] = json!(v); }
        self.call("duplicate_actor", p).await
    }

    #[tool(description = "Find actors by class name")]
    async fn find_actors_by_class(
        &self,
        #[tool(param)] class_name: String,
        #[tool(param)] exact_match: Option<bool>,
    ) -> String {
        let mut p = json!({"className": class_name});
        if let Some(v) = exact_match { p["exactMatch"] = json!(v); }
        self.call("find_actors_by_class", p).await
    }

    #[tool(description = "Spawn an actor from a Blueprint asset")]
    async fn spawn_blueprint_actor(
        &self,
        #[tool(param)] blueprint_path: String,
        #[tool(param)] name: Option<String>,
        #[tool(param)] location: Option<Vec<f64>>,
        #[tool(param)] rotation: Option<Vec<f64>>,
    ) -> String {
        let mut p = json!({"blueprintPath": blueprint_path});
        if let Some(v) = name { p["name"] = json!(v); }
        if let Some(v) = location { p["location"] = json!(v); }
        if let Some(v) = rotation { p["rotation"] = json!(v); }
        self.call("spawn_blueprint_actor", p).await
    }

    // ── Editor ──

    #[tool(description = "Get Unreal Editor information")]
    async fn get_editor_info(&self) -> String {
        self.call("get_editor_info", json!({})).await
    }

    #[tool(description = "Run a console command in Unreal Engine")]
    async fn run_console_command(&self, #[tool(param)] command: String) -> String {
        self.call("run_console_command", json!({"command": command})).await
    }

    #[tool(description = "Save the current level")]
    async fn save_current_level(&self) -> String {
        self.call("save_current_level", json!({})).await
    }

    #[tool(description = "Create a new level")]
    async fn create_level(
        &self,
        #[tool(param)] path: String,
    ) -> String {
        self.call("create_level", json!({"path": path})).await
    }

    #[tool(description = "Start Play In Editor (PIE)")]
    async fn play_in_editor(&self) -> String {
        self.call("play_in_editor", json!({})).await
    }

    #[tool(description = "Stop Play In Editor (PIE)")]
    async fn stop_play_in_editor(&self) -> String {
        self.call("stop_play_in_editor", json!({})).await
    }

    #[tool(description = "Take a screenshot of the current viewport")]
    async fn take_screenshot(&self, #[tool(param)] filename: Option<String>) -> String {
        let mut p = json!({});
        if let Some(v) = filename { p["filename"] = json!(v); }
        self.call("take_screenshot", p).await
    }

    #[tool(description = "Focus the viewport camera on an actor or location")]
    async fn focus_viewport(
        &self,
        #[tool(param)] actor_name: Option<String>,
        #[tool(param)] location: Option<Vec<f64>>,
    ) -> String {
        let mut p = json!({});
        if let Some(v) = actor_name { p["actorName"] = json!(v); }
        if let Some(v) = location { p["location"] = json!(v); }
        self.call("focus_viewport", p).await
    }

    #[tool(description = "Get information about the currently open level")]
    async fn get_current_level(&self) -> String {
        self.call("get_current_level", json!({})).await
    }

    #[tool(description = "Get list of currently selected actors in the editor")]
    async fn get_selected_actors(&self) -> String {
        self.call("get_selected_actors", json!({})).await
    }

    #[tool(description = "Select an actor by name in the editor")]
    async fn select_actor(&self, #[tool(param)] actor_name: String, #[tool(param)] add_to_selection: Option<bool>) -> String {
        let mut p = json!({"actorName": actor_name});
        if let Some(v) = add_to_selection { p["addToSelection"] = json!(v); }
        self.call("select_actor", p).await
    }

    #[tool(description = "Get recent Unreal Editor output log messages")]
    async fn get_ue_logs(
        &self,
        #[tool(param)] count: Option<i32>,
        #[tool(param)] verbosity: Option<String>,
        #[tool(param)] clear_after: Option<bool>,
    ) -> String {
        let mut p = json!({});
        if let Some(v) = count { p["count"] = json!(v); }
        if let Some(v) = verbosity { p["verbosity"] = json!(v); }
        if let Some(v) = clear_after { p["clearAfter"] = json!(v); }
        self.call("get_ue_logs", p).await
    }

    #[tool(description = "Execute an editor console command (e.g. 'newlevel', 'undo', 'redo')")]
    async fn execute_editor_command(&self, #[tool(param)] command: String) -> String {
        self.call("execute_editor_command", json!({"command": command})).await
    }

    #[tool(description = "Focus/switch to an editor panel by name")]
    async fn focus_editor_panel(&self, #[tool(param)] panel: String) -> String {
        self.call("focus_editor_panel", json!({"panel": panel})).await
    }

    #[tool(description = "List available editor console commands matching a prefix")]
    async fn get_editor_commands(&self, #[tool(param)] prefix: Option<String>) -> String {
        let mut p = json!({});
        if let Some(v) = prefix { p["prefix"] = json!(v); }
        self.call("get_editor_commands", p).await
    }

    // ── Blueprint ──

    #[tool(description = "Create a new Blueprint")]
    async fn create_blueprint(
        &self,
        #[tool(param)] name: String,
        #[tool(param)] parent_class: Option<String>,
        #[tool(param)] path: Option<String>,
    ) -> String {
        let mut p = json!({"name": name});
        if let Some(v) = parent_class { p["parentClass"] = json!(v); }
        if let Some(v) = path { p["path"] = json!(v); }
        self.call("create_blueprint", p).await
    }

    #[tool(description = "Compile a Blueprint")]
    async fn compile_blueprint(&self, #[tool(param)] path: String) -> String {
        self.call("compile_blueprint", json!({"path": path})).await
    }

    #[tool(description = "Get Blueprint information")]
    async fn get_blueprint_info(&self, #[tool(param)] path: String) -> String {
        self.call("get_blueprint_info", json!({"path": path})).await
    }

    #[tool(description = "Add a node to a Blueprint graph")]
    async fn add_blueprint_node(
        &self,
        #[tool(param)] path: String,
        #[tool(param)] node_type: String,
        #[tool(param)] name: Option<String>,
        #[tool(param)] class_name: Option<String>,
        #[tool(param)] graph_type: Option<String>,
        #[tool(param)] pos_x: Option<i32>,
        #[tool(param)] pos_y: Option<i32>,
    ) -> String {
        let mut p = json!({"path": path, "node_type": node_type});
        if let Some(n) = name {
            match node_type.as_str() {
                "CallFunction" => { p["function_name"] = json!(n); }
                "Event" | "CustomEvent" => { p["event_name"] = json!(n); }
                "VariableGet" | "VariableSet" => { p["variable_name"] = json!(n); }
                _ => {}
            }
        }
        if let Some(v) = class_name { p["class_name"] = json!(v); }
        if let Some(v) = graph_type { p["graph_type"] = json!(v); }
        if let Some(v) = pos_x { p["pos_x"] = json!(v); }
        if let Some(v) = pos_y { p["pos_y"] = json!(v); }
        self.call("add_blueprint_node", p).await
    }

    #[tool(description = "Connect two pins between nodes in a Blueprint graph")]
    async fn connect_blueprint_pins(
        &self,
        #[tool(param)] path: String,
        #[tool(param)] source_node_id: String,
        #[tool(param)] source_pin: String,
        #[tool(param)] target_node_id: String,
        #[tool(param)] target_pin: String,
    ) -> String {
        self.call("connect_blueprint_pins", json!({
            "path": path, "source_node_id": source_node_id, "source_pin": source_pin,
            "target_node_id": target_node_id, "target_pin": target_pin,
        })).await
    }

    #[tool(description = "Get the graph structure of a Blueprint")]
    async fn get_blueprint_graph(&self, #[tool(param)] path: String, #[tool(param)] graph_type: Option<String>) -> String {
        let mut p = json!({"path": path});
        if let Some(v) = graph_type { p["graph_type"] = json!(v); }
        self.call("get_blueprint_graph", p).await
    }

    #[tool(description = "Add a variable to a Blueprint")]
    async fn add_blueprint_variable(
        &self,
        #[tool(param)] path: String,
        #[tool(param)] variable_name: String,
        #[tool(param)] variable_type: String,
        #[tool(param)] is_array: Option<bool>,
    ) -> String {
        let mut p = json!({"path": path, "variable_name": variable_name, "variable_type": variable_type});
        if let Some(v) = is_array { p["is_array"] = json!(v); }
        self.call("add_blueprint_variable", p).await
    }

    #[tool(description = "Remove a variable from a Blueprint")]
    async fn remove_blueprint_variable(&self, #[tool(param)] path: String, #[tool(param)] variable_name: String) -> String {
        self.call("remove_blueprint_variable", json!({"path": path, "variable_name": variable_name})).await
    }

    #[tool(description = "Create a new function graph in a Blueprint")]
    async fn create_blueprint_function_graph(
        &self,
        #[tool(param)] path: String,
        #[tool(param)] function_name: String,
        #[tool(param)] category: Option<String>,
    ) -> String {
        let mut p = json!({"path": path, "function_name": function_name});
        if let Some(v) = category { p["category"] = json!(v); }
        self.call("create_blueprint_function_graph", p).await
    }

    #[tool(description = "List all graphs in a Blueprint")]
    async fn list_blueprint_graphs(&self, #[tool(param)] path: String) -> String {
        self.call("list_blueprint_graphs", json!({"path": path})).await
    }

    #[tool(description = "Delete a function graph from a Blueprint")]
    async fn delete_blueprint_graph(&self, #[tool(param)] path: String, #[tool(param)] graph_name: String) -> String {
        self.call("delete_blueprint_graph", json!({"path": path, "graph_name": graph_name})).await
    }

    // ── Asset ──

    #[tool(description = "List assets in a path")]
    async fn get_asset_list(&self, #[tool(param)] path: Option<String>) -> String {
        let p = if let Some(v) = path { json!({"path": v}) } else { json!({}) };
        self.call("get_asset_list", p).await
    }

    #[tool(description = "Get asset information")]
    async fn get_asset_info(&self, #[tool(param)] path: String) -> String {
        self.call("get_asset_info", json!({"path": path})).await
    }

    #[tool(description = "Delete an asset")]
    async fn delete_asset(&self, #[tool(param)] path: String) -> String {
        self.call("delete_asset", json!({"path": path})).await
    }

    #[tool(description = "Rename an asset")]
    async fn rename_asset(&self, #[tool(param)] path: String, #[tool(param)] new_name: String) -> String {
        self.call("rename_asset", json!({"path": path, "newName": new_name})).await
    }

    #[tool(description = "Import an external file into Unreal content browser")]
    async fn import_asset(
        &self,
        #[tool(param)] file_path: String,
        #[tool(param)] destination_path: Option<String>,
    ) -> String {
        let mut p = json!({"file_path": file_path});
        if let Some(v) = destination_path { p["destination_path"] = json!(v); }
        self.call("import_asset", p).await
    }

    #[tool(description = "Export an asset to a file on disk")]
    async fn export_asset(&self, #[tool(param)] asset_path: String, #[tool(param)] output_dir: Option<String>) -> String {
        let mut p = json!({"asset_path": asset_path});
        if let Some(v) = output_dir { p["output_dir"] = json!(v); }
        self.call("export_asset", p).await
    }

    // ── Component ──

    #[tool(description = "Get all components attached to an actor")]
    async fn get_actor_components(&self, #[tool(param)] actor_name: String) -> String {
        self.call("get_actor_components", json!({"actorName": actor_name})).await
    }

    #[tool(description = "Add a component to an actor")]
    async fn add_component(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] component_class: String,
        #[tool(param)] component_name: Option<String>,
    ) -> String {
        let mut p = json!({"actorName": actor_name, "componentClass": component_class});
        if let Some(v) = component_name { p["componentName"] = json!(v); }
        self.call("add_component", p).await
    }

    #[tool(description = "Remove a component from an actor")]
    async fn remove_component(&self, #[tool(param)] actor_name: String, #[tool(param)] component_name: String) -> String {
        self.call("remove_component", json!({"actorName": actor_name, "componentName": component_name})).await
    }

    // ── Material ──

    #[tool(description = "Apply a material to a component on an actor")]
    async fn set_material(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] material_path: String,
        #[tool(param)] component_name: Option<String>,
        #[tool(param)] slot_index: Option<i32>,
    ) -> String {
        let mut p = json!({"actorName": actor_name, "materialPath": material_path});
        if let Some(v) = component_name { p["componentName"] = json!(v); }
        if let Some(v) = slot_index { p["slotIndex"] = json!(v); }
        self.call("set_material", p).await
    }

    #[tool(description = "Create a material instance from a parent material")]
    async fn create_material_instance(
        &self,
        #[tool(param)] path: String,
        #[tool(param)] parent_path: String,
        #[tool(param)] instance_type: Option<String>,
    ) -> String {
        let mut p = json!({"path": path, "parentPath": parent_path});
        if let Some(v) = instance_type { p["instanceType"] = json!(v); }
        self.call("create_material_instance", p).await
    }

    #[tool(description = "Set a parameter on a material instance")]
    async fn set_material_parameter(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] parameter_name: String,
        #[tool(param)] scalar_value: Option<f64>,
        #[tool(param)] vector_value: Option<Vec<f64>>,
        #[tool(param)] component_name: Option<String>,
        #[tool(param)] slot_index: Option<i32>,
    ) -> String {
        let mut p = json!({"actorName": actor_name, "parameterName": parameter_name});
        if let Some(v) = scalar_value { p["scalarValue"] = json!(v); }
        if let Some(v) = vector_value { p["vectorValue"] = json!(v); }
        if let Some(v) = component_name { p["componentName"] = json!(v); }
        if let Some(v) = slot_index { p["slotIndex"] = json!(v); }
        self.call("set_material_parameter", p).await
    }

    // ── Mesh / Light / Effect ──

    #[tool(description = "Set the static mesh on a StaticMeshComponent")]
    async fn set_static_mesh(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] mesh_path: String,
        #[tool(param)] component_name: Option<String>,
    ) -> String {
        let mut p = json!({"actorName": actor_name, "meshPath": mesh_path});
        if let Some(v) = component_name { p["componentName"] = json!(v); }
        self.call("set_static_mesh", p).await
    }

    #[tool(description = "Set parameters on a light actor")]
    async fn set_light_parameters(
        &self,
        #[tool(param)] actor_name: String,
        #[tool(param)] intensity: Option<f64>,
        #[tool(param)] color: Option<Vec<f64>>,
        #[tool(param)] cast_shadows: Option<bool>,
    ) -> String {
        let mut p = json!({"actorName": actor_name});
        if let Some(v) = intensity { p["intensity"] = json!(v); }
        if let Some(v) = color { p["color"] = json!(v); }
        if let Some(v) = cast_shadows { p["castShadows"] = json!(v); }
        self.call("set_light_parameters", p).await
    }

    #[tool(description = "Spawn a Niagara/Cascade particle effect at a location")]
    async fn spawn_effect(
        &self,
        #[tool(param)] asset_path: String,
        #[tool(param)] location: Option<Vec<f64>>,
        #[tool(param)] rotation: Option<Vec<f64>>,
        #[tool(param)] auto_destroy: Option<bool>,
    ) -> String {
        let mut p = json!({"assetPath": asset_path});
        if let Some(v) = location { p["location"] = json!(v); }
        if let Some(v) = rotation { p["rotation"] = json!(v); }
        if let Some(v) = auto_destroy { p["autoDestroy"] = json!(v); }
        self.call("spawn_effect", p).await
    }

    // ── Input / Camera ──

    #[tool(description = "Simulate a keyboard key press or release")]
    async fn simulate_key(&self, #[tool(param)] key: String, #[tool(param)] action: Option<String>) -> String {
        let mut p = json!({"key": key});
        if let Some(v) = action { p["action"] = json!(v); }
        self.call("simulate_key", p).await
    }

    #[tool(description = "Get the current viewport camera position and rotation")]
    async fn get_viewport_camera(&self) -> String {
        self.call("get_viewport_camera", json!({})).await
    }

    // ── Viewport / Debug ──

    #[tool(description = "Set the editor viewport render mode")]
    async fn set_view_mode(&self, #[tool(param)] mode: String) -> String {
        self.call("set_view_mode", json!({"mode": mode})).await
    }

    #[tool(description = "Toggle debug visualization")]
    async fn show_debug(&self, #[tool(param)] flag: String, #[tool(param)] enable: Option<bool>) -> String {
        let mut p = json!({"flag": flag});
        if let Some(v) = enable { p["enable"] = json!(v); }
        self.call("show_debug", p).await
    }

    #[tool(description = "Add a tag to an actor")]
    async fn add_actor_tag(&self, #[tool(param)] actor_name: String, #[tool(param)] tag: String) -> String {
        self.call("add_actor_tag", json!({"actorName": actor_name, "tag": tag})).await
    }

    // ── Level / Code ──

    #[tool(description = "Open a level by path")]
    async fn open_level(&self, #[tool(param)] path: String) -> String {
        self.call("open_level", json!({"path": path})).await
    }

    #[tool(description = "Generate a C++ class template")]
    async fn generate_cpp_class(
        &self,
        #[tool(param)] class_name: String,
        #[tool(param)] parent_class: String,
        #[tool(param)] module: Option<String>,
    ) -> String {
        let mut p = json!({"className": class_name, "parentClass": parent_class});
        if let Some(v) = module { p["module"] = json!(v); }
        self.call("generate_cpp_class", p).await
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
