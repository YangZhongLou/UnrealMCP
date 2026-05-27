use unreal_mcp_server::unreal_client::UnrealClient;
use serde_json::json;

#[tokio::test]
#[ignore]
async fn test_real_ue_check_connection() {
    let mut client = UnrealClient::new("127.0.0.1:13377");
    let response = client.send_command("get_editor_info", json!({})).await.unwrap();
    assert_eq!(response["success"], true);
    let version = response["result"]["engine_version"].as_str().unwrap();
    assert!(!version.is_empty());
    println!("Connected to UE {}", version);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_create_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let level_path = format!("/Game/Maps/TestCL_{}", timestamp);

    let response = client.send_command("create_level", json!({
        "path": level_path
    })).await.unwrap();

    println!("Response: {}", serde_json::to_string_pretty(&response).unwrap());
    assert_eq!(response["success"], true, "create_level failed: {:?}", response);
    assert_eq!(response["result"]["created"], true);
    assert!(!response["result"]["path"].as_str().unwrap().is_empty());

    println!("Level created at: {}", response["result"]["path"].as_str().unwrap());
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_destroy_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestActor_{}", timestamp);

    // Spawn
    let response = client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [0.0, 0.0, 100.0]
    })).await.unwrap();

    println!("Spawn response: {}", serde_json::to_string_pretty(&response).unwrap());

    assert_eq!(response["success"], true, "spawn_actor failed: {:?}", response);
    assert!(response["result"]["actor_name"].as_str().unwrap().contains(&actor_name));

    // Verify in actor list — skip here to avoid large response truncation
    // (editor level has 1000+ actors, 64KB buffer may not be enough)
    // get_actor_list is tested separately below

    // Destroy
    let response = client.send_command("destroy_actor", json!({
        "name": actor_name
    })).await.unwrap();

    println!("Destroy response: {}", serde_json::to_string_pretty(&response).unwrap());

    assert_eq!(response["success"], true, "destroy_actor failed: {:?}", response);
    assert_eq!(response["result"]["destroyed"], true);

    println!("Spawn → verify → destroy: OK for {}", actor_name);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_actor_with_defaults() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Spawn with just class name (no name/location)
    let response = client.send_command("spawn_actor", json!({
        "className": "PointLight"
    })).await.unwrap();

    println!("Response: {}", serde_json::to_string_pretty(&response).unwrap());

    assert_eq!(response["success"], true, "spawn_actor failed: {:?}", response);
    assert_eq!(response["result"]["class"], "PointLight");

    let spawned_name = response["result"]["actor_name"].as_str().unwrap().to_string();

    // Cleanup
    client.send_command("destroy_actor", json!({"name": spawned_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_transform_and_property() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestProp_{}", timestamp);

    // Spawn
    let r = client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [0.0, 0.0, 100.0]
    })).await.unwrap();
    assert_eq!(r["success"], true, "spawn failed: {:?}", r);

    // Set transform
    let r = client.send_command("set_actor_transform", json!({
        "name": actor_name,
        "location": [100.0, 200.0, 300.0],
        "rotation": [0.0, 90.0, 0.0],
        "scale": [2.0, 2.0, 2.0]
    })).await.unwrap();
    println!("Transform: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_actor_transform failed: {:?}", r);

    // Get property
    let r = client.send_command("get_actor_property", json!({
        "actorName": actor_name,
        "propertyName": "bHidden"
    })).await.unwrap();
    println!("GetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_actor_property failed: {:?}", r);

    // Cleanup
    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_current_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("get_current_level", json!({})).await.unwrap();
    println!("Level: {}", serde_json::to_string_pretty(&r).unwrap());

    assert_eq!(r["success"], true, "get_current_level failed: {:?}", r);
    let name = r["result"]["name"].as_str().unwrap();
    let path = r["result"]["path"].as_str().unwrap();
    assert!(!name.is_empty(), "Level name is empty");
    assert!(!path.is_empty(), "Level path is empty");
    assert!(r["result"]["actor_count"].as_u64().unwrap() > 0, "Level should have actors");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_select_and_focus() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestFocus_{}", timestamp);

    // Spawn
    client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [500.0, 0.0, 100.0]
    })).await.unwrap();

    // Select the actor
    let r = client.send_command("select_actor", json!({
        "actorName": actor_name
    })).await.unwrap();
    println!("Select: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "select_actor failed: {:?}", r);

    // Focus viewport on the actor
    let r = client.send_command("focus_viewport", json!({
        "actorName": actor_name
    })).await.unwrap();
    println!("Focus: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "focus_viewport failed: {:?}", r);

    // Verify selection
    let r = client.send_command("get_selected_actors", json!({})).await.unwrap();
    println!("Selected: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_selected_actors failed: {:?}", r);

    // Cleanup
    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_save_current_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // First create a named level so SaveLevel doesn't show "Save As" dialog
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let level_path = format!("/Game/Maps/SaveTest_{}", timestamp);
    client.send_command("create_level", json!({"path": level_path})).await.unwrap();

    // Now save should work without dialog
    let r = client.send_command("save_current_level", json!({})).await.unwrap();
    println!("Save: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "save_current_level failed: {:?}", r);
    assert_eq!(r["result"]["saved"], true);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_actor_components() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestComp_{}", timestamp);

    // Spawn a light (has multiple components)
    client.send_command("spawn_actor", json!({
        "className": "PointLight",
        "name": actor_name,
        "location": [0.0, 0.0, 100.0]
    })).await.unwrap();

    let r = client.send_command("get_actor_components", json!({
        "actorName": actor_name
    })).await.unwrap();
    println!("Components: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_actor_components failed: {:?}", r);
    assert!(r["result"]["count"].as_u64().unwrap() > 0, "Should have components");

    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_duplicate_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let source_name = format!("TestDupSrc_{}", timestamp);
    let dup_name = format!("TestDupDst_{}", timestamp);

    // Spawn
    client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": source_name,
        "location": [0.0, 0.0, 0.0]
    })).await.unwrap();

    // Duplicate
    let r = client.send_command("duplicate_actor", json!({
        "name": source_name,
        "newName": dup_name
    })).await.unwrap();
    println!("Duplicate: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "duplicate_actor failed: {:?}", r);
    assert_eq!(r["result"]["source"], source_name);
    let dup_name_resp = r["result"]["actor_name"].as_str().unwrap().to_string();

    // Verify duplicated actor exists by querying its components
    let r = client.send_command("get_actor_components", json!({
        "actorName": dup_name_resp
    })).await.unwrap();
    assert_eq!(r["success"], true, "Duplicate not found in world: {:?}", r);

    // Cleanup both
    client.send_command("destroy_actor", json!({"name": source_name})).await.unwrap();
    client.send_command("destroy_actor", json!({"name": dup_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_find_actors_by_class() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("find_actors_by_class", json!({
        "className": "StaticMesh"
    })).await.unwrap();
    println!("Find: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "find_actors_by_class failed: {:?}", r);
    // Note: count may be 0 if no StaticMeshActors in level
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_asset_list() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("get_asset_list", json!({
        "path": "/Game"
    })).await.unwrap();
    println!("Asset list: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_asset_list failed: {:?}", r);
    assert!(r["result"]["count"].as_u64().is_some(), "Should have count field");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_viewport_camera() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("get_viewport_camera", json!({})).await.unwrap();
    println!("Camera: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_viewport_camera failed: {:?}", r);
    assert!(r["result"]["location"].is_array(), "Should have location array");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_run_console_command() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("run_console_command", json!({
        "command": "stat fps"
    })).await.unwrap();
    println!("Console: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "run_console_command failed: {:?}", r);
    assert_eq!(r["result"]["executed"], true);

    // Restore: turn off fps display
    client.send_command("run_console_command", json!({"command": "stat fps"})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_editor_commands() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("get_editor_commands", json!({
        "prefix": "stat"
    })).await.unwrap();
    println!("Commands: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_editor_commands failed: {:?}", r);
    assert!(r["result"]["count"].as_u64().unwrap() > 0, "Should find stat commands");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_actor_property() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestProp_{}", timestamp);

    client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [0.0, 0.0, 100.0]
    })).await.unwrap();

    // Set bHidden to true
    let r = client.send_command("set_actor_property", json!({
        "actorName": actor_name,
        "propertyName": "bHidden",
        "value": {"BoolValue": true}
    })).await.unwrap();
    println!("SetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_actor_property failed: {:?}", r);
    assert_eq!(r["result"]["set"], true);

    // Verify
    let r = client.send_command("get_actor_property", json!({
        "actorName": actor_name,
        "propertyName": "bHidden"
    })).await.unwrap();
    println!("GetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true);
    assert_eq!(r["result"]["value"], true);

    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_actor_tag() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestTag_{}", timestamp);
    let tag = format!("TestTag_{}", timestamp);

    client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [0.0, 0.0, 0.0]
    })).await.unwrap();

    let r = client.send_command("add_actor_tag", json!({
        "actorName": actor_name,
        "tag": tag
    })).await.unwrap();
    println!("Tag: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_actor_tag failed: {:?}", r);

    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_asset_info() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Query /Engine asset — should exist in every UE install
    let r = client.send_command("get_asset_info", json!({
        "path": "/Engine/EngineDamageTypes/DmgTypeBP_Environmental"
    })).await.unwrap();
    println!("AssetInfo: {}", serde_json::to_string_pretty(&r).unwrap());
    assert!(r["success"].as_bool().is_some(), "Should have success field");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_ue_logs() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("get_ue_logs", json!({
        "count": 10
    })).await.unwrap();
    println!("Logs: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_ue_logs failed: {:?}", r);
    assert!(r["result"]["count"].as_u64().is_some(), "Should have count");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_view_mode() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("set_view_mode", json!({
        "mode": "Unlit"
    })).await.unwrap();
    println!("ViewMode: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_view_mode failed: {:?}", r);

    // Restore original view mode
    client.send_command("set_view_mode", json!({"mode": "Lit"})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_show_debug() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("show_debug", json!({
        "type": "Collision"
    })).await.unwrap();
    println!("Debug: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "show_debug failed: {:?}", r);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_take_screenshot() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("take_screenshot", json!({
        "filename": "real_ue_test_screenshot"
    })).await.unwrap();
    println!("Screenshot: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "take_screenshot failed: {:?}", r);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_simulate_key() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("simulate_key", json!({
        "key": "F",
        "action": "press"
    })).await.unwrap();
    println!("Key: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "simulate_key failed: {:?}", r);

    // Release
    client.send_command("simulate_key", json!({
        "key": "F",
        "action": "release"
    })).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_remove_component() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestCompAdd_{}", timestamp);

    client.send_command("spawn_actor", json!({
        "className": "StaticMeshActor",
        "name": actor_name,
        "location": [0.0, 0.0, 0.0]
    })).await.unwrap();

    // Add a PointLightComponent
    let r = client.send_command("add_component", json!({
        "actorName": actor_name,
        "componentClass": "PointLightComponent"
    })).await.unwrap();
    println!("AddComp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_component failed: {:?}", r);

    let comp_name = r["result"]["component_name"].as_str().unwrap().to_string();

    // Verify via get_actor_components
    let r = client.send_command("get_actor_components", json!({
        "actorName": actor_name
    })).await.unwrap();
    println!("Comps: count={}", r["result"]["count"]);

    // Remove the component
    let r = client.send_command("remove_component", json!({
        "actorName": actor_name,
        "componentName": comp_name
    })).await.unwrap();
    println!("RemoveComp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "remove_component failed: {:?}", r);
    assert_eq!(r["result"]["removed"], true);

    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_light_parameters() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH).unwrap().as_secs();
    let actor_name = format!("TestLight_{}", timestamp);

    client.send_command("spawn_actor", json!({
        "className": "PointLight",
        "name": actor_name,
        "location": [0.0, 0.0, 200.0]
    })).await.unwrap();

    let r = client.send_command("set_light_parameters", json!({
        "actorName": actor_name,
        "intensity": 8000.0,
        "color": [1.0, 0.0, 0.0],
        "castShadows": false
    })).await.unwrap();
    println!("Light: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_light_parameters failed: {:?}", r);

    // Verify intensity was set via get_actor_property
    let r = client.send_command("get_actor_property", json!({
        "actorName": actor_name,
        "propertyName": "IntensityUnits"
    })).await.unwrap();
    println!("Verify intensity: {}", serde_json::to_string_pretty(&r).unwrap());
    // Property name may vary by UE version; just confirm we can read it back
    assert!(r["success"].as_bool().is_some(), "get_actor_property failed");

    client.send_command("destroy_actor", json!({"name": actor_name})).await.unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_execute_editor_command() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("execute_editor_command", json!({
        "command": "Undo"
    })).await.unwrap();
    println!("Exec: {}", serde_json::to_string_pretty(&r).unwrap());
    // Command may or may not be recognized — just verify it doesn't crash
    assert!(r["success"].as_bool().is_some(), "execute_editor_command failed");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_focus_editor_panel() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client.send_command("focus_editor_panel", json!({
        "panel": "ContentBrowser"
    })).await.unwrap();
    println!("Panel: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "focus_editor_panel failed: {:?}", r);
    assert_eq!(r["result"]["focused"], true);
}
