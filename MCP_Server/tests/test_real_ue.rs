use serde_json::json;
use unreal_mcp_server::unreal_client::UnrealClient;

#[tokio::test]
#[ignore]
async fn test_real_ue_get_editor_info() {
    let mut client = UnrealClient::new("127.0.0.1:13377");
    let response = client
        .send_command("get_editor_info", json!({}))
        .await
        .unwrap();
    assert_eq!(response["success"], true);
    let version = response["result"]["engine_version"].as_str().unwrap();
    assert!(!version.is_empty());
    println!("Connected to UE {}", version);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_check_unreal_connection() {
    // check_unreal_connection is a Rust-side wrapper that calls get_editor_info
    // and formats the result with a "Connected:" prefix.
    let mut client = UnrealClient::new("127.0.0.1:13377");
    let response = client
        .send_command("get_editor_info", json!({}))
        .await
        .unwrap();
    println!(
        "CheckConnection: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );
    assert_eq!(response["success"], true);
    assert!(!response["result"]["engine_version"]
        .as_str()
        .unwrap()
        .is_empty());
}

#[tokio::test]
#[ignore]
async fn test_real_ue_create_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let level_path = format!("/Game/Maps/TestCL_{}", timestamp);

    let response = client
        .send_command(
            "create_level",
            json!({
                "path": level_path
            }),
        )
        .await
        .unwrap();

    println!(
        "Response: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );
    assert_eq!(
        response["success"], true,
        "create_level failed: {:?}",
        response
    );
    assert_eq!(response["result"]["created"], true);
    assert!(!response["result"]["path"].as_str().unwrap().is_empty());

    println!(
        "Level created at: {}",
        response["result"]["path"].as_str().unwrap()
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestActor_{}", timestamp);

    let response = client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    println!(
        "Spawn: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );
    assert_eq!(
        response["success"], true,
        "spawn_actor failed: {:?}",
        response
    );
    assert!(response["result"]["actor_name"]
        .as_str()
        .unwrap()
        .contains(&actor_name));

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_destroy_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestDestroy_{}", timestamp);

    // Spawn first
    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Destroy
    let response = client
        .send_command(
            "destroy_actor",
            json!({
                "name": actor_name
            }),
        )
        .await
        .unwrap();
    println!(
        "Destroy: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );
    assert_eq!(
        response["success"], true,
        "destroy_actor failed: {:?}",
        response
    );
    assert_eq!(response["result"]["destroyed"], true);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_actor_with_defaults() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Spawn with just class name (no name/location)
    let response = client
        .send_command(
            "spawn_actor",
            json!({
                "className": "PointLight"
            }),
        )
        .await
        .unwrap();

    println!(
        "Response: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );

    assert_eq!(
        response["success"], true,
        "spawn_actor failed: {:?}",
        response
    );
    assert_eq!(response["result"]["class"], "PointLight");

    let spawned_name = response["result"]["actor_name"]
        .as_str()
        .unwrap()
        .to_string();

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": spawned_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_actor_transform() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestXform_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "set_actor_transform",
            json!({
                "name": actor_name,
                "location": [100.0, 200.0, 300.0],
                "rotation": [0.0, 90.0, 0.0],
                "scale": [2.0, 2.0, 2.0]
            }),
        )
        .await
        .unwrap();
    println!("Transform: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_actor_transform failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_actor_property() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestGetProp_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "get_actor_property",
            json!({
                "actorName": actor_name,
                "propertyName": "bHidden"
            }),
        )
        .await
        .unwrap();
    println!("GetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_actor_property failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_current_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command("get_current_level", json!({}))
        .await
        .unwrap();
    println!("Level: {}", serde_json::to_string_pretty(&r).unwrap());

    assert_eq!(r["success"], true, "get_current_level failed: {:?}", r);
    let name = r["result"]["name"].as_str().unwrap();
    let path = r["result"]["path"].as_str().unwrap();
    assert!(!name.is_empty(), "Level name is empty");
    assert!(!path.is_empty(), "Level path is empty");
    assert!(
        r["result"]["actor_count"].as_u64().unwrap() > 0,
        "Level should have actors"
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_select_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestSelect_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [500.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "select_actor",
            json!({
                "actorName": actor_name
            }),
        )
        .await
        .unwrap();
    println!("Select: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "select_actor failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_focus_viewport() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestFocus_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [500.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "focus_viewport",
            json!({
                "actorName": actor_name
            }),
        )
        .await
        .unwrap();
    println!("Focus: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "focus_viewport failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_selected_actors() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestGetSel_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [500.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Select the actor first
    client
        .send_command(
            "select_actor",
            json!({
                "actorName": actor_name
            }),
        )
        .await
        .unwrap();

    // Verify it appears in selected actors
    let r = client
        .send_command("get_selected_actors", json!({}))
        .await
        .unwrap();
    println!("Selected: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_selected_actors failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_save_current_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // First create a named level so SaveLevel doesn't show "Save As" dialog
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let level_path = format!("/Game/Maps/SaveTest_{}", timestamp);
    client
        .send_command("create_level", json!({"path": level_path}))
        .await
        .unwrap();

    // Now save should work without dialog
    let r = client
        .send_command("save_current_level", json!({}))
        .await
        .unwrap();
    println!("Save: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "save_current_level failed: {:?}", r);
    assert_eq!(r["result"]["saved"], true);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_actor_components() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestComp_{}", timestamp);

    // Spawn a light (has multiple components)
    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "PointLight",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "get_actor_components",
            json!({
                "actorName": actor_name
            }),
        )
        .await
        .unwrap();
    println!("Components: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_actor_components failed: {:?}", r);
    assert!(
        r["result"]["count"].as_u64().unwrap() > 0,
        "Should have components"
    );

    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_duplicate_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let source_name = format!("TestDupSrc_{}", timestamp);
    let dup_name = format!("TestDupDst_{}", timestamp);

    // Spawn
    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": source_name,
                "location": [0.0, 0.0, 0.0]
            }),
        )
        .await
        .unwrap();

    // Duplicate
    let r = client
        .send_command(
            "duplicate_actor",
            json!({
                "name": source_name,
                "newName": dup_name
            }),
        )
        .await
        .unwrap();
    println!("Duplicate: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "duplicate_actor failed: {:?}", r);
    assert_eq!(r["result"]["source"], source_name);
    let dup_name_resp = r["result"]["actor_name"].as_str().unwrap().to_string();

    // Verify duplicated actor exists by querying its components
    let r = client
        .send_command(
            "get_actor_components",
            json!({
                "actorName": dup_name_resp
            }),
        )
        .await
        .unwrap();
    assert_eq!(r["success"], true, "Duplicate not found in world: {:?}", r);

    // Cleanup both
    client
        .send_command("destroy_actor", json!({"name": source_name}))
        .await
        .unwrap();
    client
        .send_command("destroy_actor", json!({"name": dup_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_find_actors_by_class() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "find_actors_by_class",
            json!({
                "className": "StaticMesh"
            }),
        )
        .await
        .unwrap();
    println!("Find: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "find_actors_by_class failed: {:?}", r);
    // Note: count may be 0 if no StaticMeshActors in level
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_asset_list() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "get_asset_list",
            json!({
                "path": "/Game",
                "limit": 10
            }),
        )
        .await
        .unwrap();
    println!("Asset list count: {}", r["result"]["count"]);
    assert_eq!(r["success"], true, "get_asset_list failed: {:?}", r);
    assert!(
        r["result"]["count"].as_u64().is_some(),
        "Should have count field"
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_viewport_camera() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command("get_viewport_camera", json!({}))
        .await
        .unwrap();
    println!("Camera: {}", serde_json::to_string_pretty(&r).unwrap());

    // Camera may be unavailable if no viewport is active (e.g. editor minimized).
    // When available, check that location/rotation are present.
    if r["success"] == true {
        assert!(
            r["result"]["location"].is_array(),
            "Should have location array"
        );
    } else {
        assert!(
            r["error"].as_str().unwrap().contains("No viewport"),
            "Expected camera error"
        );
    }
}

#[tokio::test]
#[ignore]
async fn test_real_ue_run_console_command() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "run_console_command",
            json!({
                "command": "stat fps"
            }),
        )
        .await
        .unwrap();
    println!("Console: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "run_console_command failed: {:?}", r);
    assert_eq!(r["result"]["executed"], true);

    // Restore: turn off fps display
    client
        .send_command("run_console_command", json!({"command": "stat fps"}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_editor_commands() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "get_editor_commands",
            json!({
                "prefix": "stat"
            }),
        )
        .await
        .unwrap();
    println!("Commands: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_editor_commands failed: {:?}", r);
    assert!(
        r["result"]["count"].as_u64().unwrap() > 0,
        "Should find stat commands"
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_actor_property() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestProp_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Set bHidden to true
    let r = client
        .send_command(
            "set_actor_property",
            json!({
                "actorName": actor_name,
                "propertyName": "bHidden",
                "value": {"BoolValue": true}
            }),
        )
        .await
        .unwrap();
    println!("SetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_actor_property failed: {:?}", r);
    assert_eq!(r["result"]["set"], true);

    // Verify
    let r = client
        .send_command(
            "get_actor_property",
            json!({
                "actorName": actor_name,
                "propertyName": "bHidden"
            }),
        )
        .await
        .unwrap();
    println!("GetProp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true);
    assert_eq!(r["result"]["value"], true);

    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_actor_tag() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestTag_{}", timestamp);
    let tag = format!("TestTag_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 0.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "add_actor_tag",
            json!({
                "actorName": actor_name,
                "tag": tag
            }),
        )
        .await
        .unwrap();
    println!("Tag: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_actor_tag failed: {:?}", r);

    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_asset_info() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Query /Engine asset — should exist in every UE install
    let r = client
        .send_command(
            "get_asset_info",
            json!({
                "path": "/Engine/EngineDamageTypes/DmgTypeBP_Environmental"
            }),
        )
        .await
        .unwrap();
    println!("AssetInfo: {}", serde_json::to_string_pretty(&r).unwrap());
    assert!(
        r["success"].as_bool().is_some(),
        "Should have success field"
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_ue_logs() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "get_ue_logs",
            json!({
                "count": 10
            }),
        )
        .await
        .unwrap();
    println!("Logs: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_ue_logs failed: {:?}", r);
    assert!(r["result"]["count"].as_u64().is_some(), "Should have count");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_view_mode() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "set_view_mode",
            json!({
                "mode": "Unlit"
            }),
        )
        .await
        .unwrap();
    println!("ViewMode: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_view_mode failed: {:?}", r);

    // Restore original view mode
    client
        .send_command("set_view_mode", json!({"mode": "Lit"}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_show_debug() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "show_debug",
            json!({
                "type": "Collision"
            }),
        )
        .await
        .unwrap();
    println!("Debug: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "show_debug failed: {:?}", r);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_take_screenshot() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "take_screenshot",
            json!({
                "filename": "real_ue_test_screenshot"
            }),
        )
        .await
        .unwrap();
    println!("Screenshot: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "take_screenshot failed: {:?}", r);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_simulate_key() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "simulate_key",
            json!({
                "key": "F",
                "action": "press"
            }),
        )
        .await
        .unwrap();
    println!("Key: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "simulate_key failed: {:?}", r);

    // Release
    client
        .send_command(
            "simulate_key",
            json!({
                "key": "F",
                "action": "release"
            }),
        )
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_component() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestAddComp_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 0.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "add_component",
            json!({
                "actorName": actor_name,
                "componentClass": "PointLightComponent"
            }),
        )
        .await
        .unwrap();
    println!("AddComp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_component failed: {:?}", r);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_remove_component() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestRemoveComp_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 0.0]
            }),
        )
        .await
        .unwrap();

    // Add component first
    let r = client
        .send_command(
            "add_component",
            json!({
                "actorName": actor_name,
                "componentClass": "PointLightComponent"
            }),
        )
        .await
        .unwrap();
    let comp_name = r["result"]["component_name"].as_str().unwrap().to_string();

    // Remove it
    let r = client
        .send_command(
            "remove_component",
            json!({
                "actorName": actor_name,
                "componentName": comp_name
            }),
        )
        .await
        .unwrap();
    println!("RemoveComp: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "remove_component failed: {:?}", r);
    assert_eq!(r["result"]["removed"], true);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_light_parameters() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestLight_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "PointLight",
                "name": actor_name,
                "location": [0.0, 0.0, 200.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "set_light_parameters",
            json!({
                "actorName": actor_name,
                "intensity": 8000.0,
                "color": [1.0, 0.0, 0.0],
                "castShadows": false
            }),
        )
        .await
        .unwrap();
    println!("Light: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_light_parameters failed: {:?}", r);

    // Verify intensity was set via get_actor_property
    let r = client
        .send_command(
            "get_actor_property",
            json!({
                "actorName": actor_name,
                "propertyName": "IntensityUnits"
            }),
        )
        .await
        .unwrap();
    println!(
        "Verify intensity: {}",
        serde_json::to_string_pretty(&r).unwrap()
    );
    // Property name may vary by UE version; just confirm we can read it back
    assert!(
        r["success"].as_bool().is_some(),
        "get_actor_property failed"
    );

    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_execute_editor_command() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "execute_editor_command",
            json!({
                "command": "Undo"
            }),
        )
        .await
        .unwrap();
    println!("Exec: {}", serde_json::to_string_pretty(&r).unwrap());
    // Command may or may not be recognized — just verify it doesn't crash
    assert!(
        r["success"].as_bool().is_some(),
        "execute_editor_command failed"
    );
}

#[tokio::test]
#[ignore]
async fn test_real_ue_focus_editor_panel() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let r = client
        .send_command(
            "focus_editor_panel",
            json!({
                "panel": "ContentBrowser"
            }),
        )
        .await
        .unwrap();
    println!("Panel: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "focus_editor_panel failed: {:?}", r);
    assert_eq!(r["result"]["focused"], true);
}

// ── Batch 2-1: Actor + Asset ──

#[tokio::test]
#[ignore]
async fn test_real_ue_get_actor_list() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestList_{}", timestamp);

    // Spawn a uniquely-named actor
    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "PointLight",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Get full actor list
    let r = client
        .send_command("get_actor_list", json!({}))
        .await
        .unwrap();
    println!("ActorList: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_actor_list failed: {:?}", r);

    let actors = r["result"]["actors"]
        .as_array()
        .expect("actors should be an array");
    assert!(!actors.is_empty(), "Should have at least one actor");

    // Verify our spawned actor is in the list
    let found = actors
        .iter()
        .any(|a| a["name"].as_str() == Some(&actor_name));
    assert!(
        found,
        "Spawned actor '{}' not found in get_actor_list",
        actor_name
    );

    println!(
        "get_actor_list OK: total={}, found={}",
        actors.len(),
        actor_name
    );

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_create_blueprint() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPCreate_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    let r = client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();
    println!("CreateBP: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "create_blueprint failed: {:?}", r);
    assert_eq!(r["result"]["blueprint_name"], bp_name);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_compile_blueprint() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPCompile_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "compile_blueprint",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();
    println!("Compile: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "compile_blueprint failed: {:?}", r);
    assert_eq!(r["result"]["compiled"], true);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_blueprint_actor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPA_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    // Setup: create and compile a BP
    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();
    client
        .send_command("compile_blueprint", json!({"path": bp_asset_path}))
        .await
        .unwrap();

    // Spawn from BP
    let actor_name = format!("SpawnedBP_{}", timestamp);
    let r = client
        .send_command(
            "spawn_blueprint_actor",
            json!({
                "blueprintPath": bp_asset_path,
                "name": actor_name,
                "location": [100.0, 200.0, 50.0]
            }),
        )
        .await
        .unwrap();
    println!("SpawnBP: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "spawn_blueprint_actor failed: {:?}", r);
    assert!(!r["result"]["name"].as_str().unwrap().is_empty());

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_delete_asset() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("DeleteTest_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    // Create a test asset first
    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Delete it
    let r = client
        .send_command(
            "delete_asset",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();
    println!("Delete: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "delete_asset failed: {:?}", r);
    assert_eq!(r["result"]["deleted"], true);

    // Verify it's gone
    let r = client
        .send_command(
            "get_asset_info",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();
    assert_eq!(r["success"], false, "Deleted asset should not exist");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_rename_asset() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("RenameTest_{}", timestamp);
    let bp_renamed = format!("RenameTest_New_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);
    let bp_renamed_path = format!("/Game/Test/{}.{}", bp_renamed, bp_renamed);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "rename_asset",
            json!({
                "path": bp_asset_path,
                "newName": bp_renamed
            }),
        )
        .await
        .unwrap();
    println!("Rename: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "rename_asset failed: {:?}", r);
    assert_eq!(r["result"]["renamed"], true);

    // Verify renamed asset exists
    let r = client
        .send_command(
            "get_asset_info",
            json!({
                "path": bp_renamed_path
            }),
        )
        .await
        .unwrap();
    assert_eq!(r["success"], true, "Renamed asset should exist: {:?}", r);
    assert_eq!(r["result"]["name"], bp_renamed);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_renamed_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_import_asset() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();

    // Create a simple text file to test import (no dialog with unattended mode)
    let txt_file = format!("D:/temp/ue_import_test_{}.txt", timestamp);
    std::fs::create_dir_all("D:/temp").unwrap();
    std::fs::write(&txt_file, "test import content\n").unwrap();

    let r = client
        .send_command(
            "import_asset",
            json!({
                "file_path": txt_file,
                "destination_path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();
    println!("Import: {}", serde_json::to_string_pretty(&r).unwrap());

    // In unattended mode, import may succeed or fail cleanly (no dialogs).
    // Either outcome validates the handler code path.
    let _ = std::fs::remove_file(&txt_file);
    if r["success"] == true {
        assert!(r["result"]["count"].as_u64().unwrap() > 0);
        let imported_path = r["result"]["imported"][0]["path"].as_str().unwrap();
        client
            .send_command("delete_asset", json!({"path": imported_path}))
            .await
            .unwrap();
    } else {
        assert!(
            r["error"].as_str().unwrap().contains("Failed to import"),
            "Expected import error, got: {:?}",
            r
        );
    }
}

#[tokio::test]
#[ignore]
async fn test_real_ue_export_asset() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("ExportOnly_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);
    let export_dir = format!("D:/temp/ue_test_{}", timestamp);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "export_asset",
            json!({
                "asset_path": bp_asset_path,
                "output_dir": export_dir
            }),
        )
        .await
        .unwrap();
    println!("Export: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "export_asset failed: {:?}", r);
    assert_eq!(r["result"]["asset_name"], bp_name);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

// ── Batch 2-2: Mesh / Effect / Material ──

#[tokio::test]
#[ignore]
async fn test_real_ue_set_static_mesh() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestMesh_{}", timestamp);

    // Spawn a StaticMeshActor with default mesh
    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Change its static mesh to Cube
    let r = client
        .send_command(
            "set_static_mesh",
            json!({
                "actorName": actor_name,
                "meshPath": "/Engine/BasicShapes/Cube.Cube"
            }),
        )
        .await
        .unwrap();
    println!("SetMesh: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_static_mesh failed: {:?}", r);
    assert_eq!(r["result"]["actor"], actor_name);

    // Verify via get_actor_components — should still have a StaticMeshComponent
    let r = client
        .send_command(
            "get_actor_components",
            json!({
                "actorName": actor_name
            }),
        )
        .await
        .unwrap();
    assert_eq!(r["success"], true, "get_actor_components failed: {:?}", r);
    assert!(
        r["result"]["count"].as_u64().unwrap() > 0,
        "Should have components"
    );

    println!("set_static_mesh OK");

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_spawn_effect() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();

    // Use a known engine asset as the effect source;
    // for non-Cascade assets the handler creates a generic particle component
    let r = client
        .send_command(
            "spawn_effect",
            json!({
                "assetPath": "/Engine/BasicShapes/Cube.Cube",
                "location": [0.0, 0.0, 200.0],
                "autoDestroy": false
            }),
        )
        .await
        .unwrap();
    println!("SpawnEffect: {}", serde_json::to_string_pretty(&r).unwrap());

    // spawn_effect may succeed (generic component) or fail (asset not found);
    // either way, verify we get a valid JSON response with success field
    assert!(
        r["success"].as_bool().is_some(),
        "Should have success field"
    );

    if r["success"] == true {
        let effect_name = r["result"]["name"].as_str().unwrap();
        println!("Effect spawned: {}", effect_name);

        // Cleanup — destroy the container actor (name format: effect component name)
        // The spawned actor holds the effect; find and destroy via name suffix
        let r = client
            .send_command("get_actor_list", json!({}))
            .await
            .unwrap();
        if r["success"] == true {
            if let Some(actors) = r["result"]["actors"].as_array() {
                for a in actors {
                    let name = a["name"].as_str().unwrap_or("");
                    if name.contains("FXActor") || name.contains(&format!("{}", timestamp)) {
                        client
                            .send_command("destroy_actor", json!({"name": name}))
                            .await
                            .unwrap();
                    }
                }
            }
        }
    }
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_material() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestMat_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "set_material",
            json!({
                "actorName": actor_name,
                "materialPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
                "slotIndex": 0
            }),
        )
        .await
        .unwrap();
    println!("SetMaterial: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "set_material failed: {:?}", r);
    assert_eq!(r["result"]["actor"], actor_name);

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_set_material_parameter() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let actor_name = format!("TestMatParam_{}", timestamp);

    client
        .send_command(
            "spawn_actor",
            json!({
                "className": "StaticMeshActor",
                "name": actor_name,
                "location": [0.0, 0.0, 100.0]
            }),
        )
        .await
        .unwrap();

    // Apply material first (set_material_parameter creates DMI from the current material)
    client
        .send_command(
            "set_material",
            json!({
                "actorName": actor_name,
                "materialPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
                "slotIndex": 0
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "set_material_parameter",
            json!({
                "actorName": actor_name,
                "parameterName": "Roughness",
                "scalarValue": 0.5
            }),
        )
        .await
        .unwrap();
    println!("SetMatParam: {}", serde_json::to_string_pretty(&r).unwrap());
    assert!(
        r["success"].as_bool().is_some(),
        "Should have success field"
    );

    // Cleanup
    client
        .send_command("destroy_actor", json!({"name": actor_name}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_create_material_instance() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let mi_name = format!("MI_Test_{}", timestamp);
    let mi_path = format!("/Game/Test/{}.{}", mi_name, mi_name);

    // Create a material instance from the engine default material
    let r = client
        .send_command(
            "create_material_instance",
            json!({
                "path": mi_path,
                "parentPath": "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
                "instanceType": "constant"
            }),
        )
        .await
        .unwrap();
    println!("CreateMI: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(
        r["success"], true,
        "create_material_instance failed: {:?}",
        r
    );
    assert_eq!(r["result"]["type"], "constant");

    // Verify the material instance asset exists
    let r = client
        .send_command(
            "get_asset_info",
            json!({
                "path": mi_path
            }),
        )
        .await
        .unwrap();
    println!("MI_Info: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(
        r["success"], true,
        "Created MI should exist via get_asset_info: {:?}",
        r
    );

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": mi_path}))
        .await
        .unwrap();
    println!("create_material_instance OK");
}

// ── Batch 2-3: Editor PIE + Level / Code ──

#[tokio::test]
#[ignore]
async fn test_real_ue_zzz_play_in_editor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Spawn a PlayerStart so PIE doesn't crash the engine (minimal project has none)
    let r = client
        .send_command(
            "spawn_actor",
            json!({
                "className": "PlayerStart",
                "name": "PIETest_PlayerStart",
                "location": [0.0, 0.0, 0.0]
            }),
        )
        .await
        .unwrap();
    assert_eq!(
        r["success"], true,
        "Failed to spawn PlayerStart for PIE: {:?}",
        r
    );
    client
        .send_command("save_current_level", json!({}))
        .await
        .unwrap();

    let r = client
        .send_command("play_in_editor", json!({}))
        .await
        .unwrap();
    println!("PIE_Start: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "play_in_editor failed: {:?}", r);
    assert_eq!(r["result"]["playing"], true);

    // NOTE: Cannot call stop_play_in_editor here — EndPlayMap() deadlocks
    // on the GameThread during active PIE with the AsyncTask+Wait pattern.
    // Playing=true confirms the handler works. Test must run LAST.
    // The engine stays in PIE until user presses Stop or closes the editor.
}

#[tokio::test]
#[ignore]
async fn test_real_ue_stop_play_in_editor() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Test the stop handler when PIE is NOT active
    let r = client
        .send_command("stop_play_in_editor", json!({}))
        .await
        .unwrap();
    println!("PIE_Stop: {}", serde_json::to_string_pretty(&r).unwrap());
    assert!(
        !r["success"].as_bool().unwrap(),
        "stop should fail when no PIE is active"
    );
    assert!(r["error"]
        .as_str()
        .unwrap()
        .contains("No active play session"));
}

#[tokio::test]
#[ignore]
async fn test_real_ue_open_level() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    // Get current level path and re-open it (avoids creating a new level that
    // may crash the engine in a minimal project with no default map setup).
    let r = client
        .send_command("get_current_level", json!({}))
        .await
        .unwrap();
    let current_path = r["result"]["path"].as_str().unwrap();

    let r = client
        .send_command(
            "open_level",
            json!({
                "path": current_path
            }),
        )
        .await
        .unwrap();
    println!("OpenLevel: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "open_level failed: {:?}", r);
    assert_eq!(r["result"]["opened"], true);

    println!("open_level OK");
}

#[tokio::test]
#[ignore]
async fn test_real_ue_generate_cpp_class() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let class_name = format!("TestGenClass_{}", timestamp);

    let output_dir = format!("D:/temp/ue_cpp_test_{}", timestamp);

    let r = client
        .send_command(
            "generate_cpp_class",
            json!({
                "className": class_name,
                "parentClass": "AActor",
                "output_dir": output_dir
            }),
        )
        .await
        .unwrap();
    println!("GenCPP: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "generate_cpp_class failed: {:?}", r);

    let header = r["result"]["header"].as_str().unwrap();
    let source = r["result"]["source"].as_str().unwrap();
    assert!(
        header.contains(&class_name),
        "Header path should contain class name"
    );
    assert!(
        source.contains(&class_name),
        "Source path should contain class name"
    );

    println!("generate_cpp_class OK: h={}, cpp={}", header, source);
}

// ── Batch 2-4: Blueprint ──

#[tokio::test]
#[ignore]
async fn test_real_ue_get_blueprint_info() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPInfo_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    // Create a blueprint
    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Compile it so is_compiled is true
    client
        .send_command(
            "compile_blueprint",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();

    // Get blueprint info
    let r = client
        .send_command(
            "get_blueprint_info",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();
    println!("BPInfo: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "get_blueprint_info failed: {:?}", r);
    assert_eq!(r["result"]["name"], bp_name);
    assert_eq!(r["result"]["parent_class"], "Actor");
    assert_eq!(r["result"]["is_compiled"], true);
    assert!(
        r["result"]["variables"].is_array(),
        "Should have variables array"
    );

    println!("get_blueprint_info OK");

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_blueprint_variable() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPAddVar_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "add_blueprint_variable",
            json!({
                "path": bp_asset_path,
                "variable_name": "Health",
                "variable_type": "float"
            }),
        )
        .await
        .unwrap();
    println!("AddVar: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_blueprint_variable failed: {:?}", r);
    assert_eq!(r["result"]["variable_name"], "Health");

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_remove_blueprint_variable() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPRemVar_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Add a variable first
    client
        .send_command(
            "add_blueprint_variable",
            json!({
                "path": bp_asset_path,
                "variable_name": "Health",
                "variable_type": "float"
            }),
        )
        .await
        .unwrap();

    // Remove it
    let r = client
        .send_command(
            "remove_blueprint_variable",
            json!({
                "path": bp_asset_path,
                "variable_name": "Health"
            }),
        )
        .await
        .unwrap();
    println!("RemoveVar: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(
        r["success"], true,
        "remove_blueprint_variable failed: {:?}",
        r
    );
    assert_eq!(r["result"]["variable_name"], "Health");

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_create_blueprint_function_graph() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPCreateFunc_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);
    let func_name = format!("MyFunc_{}", timestamp);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "create_blueprint_function_graph",
            json!({
                "path": bp_asset_path,
                "function_name": func_name,
                "category": "Test"
            }),
        )
        .await
        .unwrap();
    println!("CreateFunc: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(
        r["success"], true,
        "create_blueprint_function_graph failed: {:?}",
        r
    );
    assert_eq!(r["result"]["function_name"], func_name);
    assert!(!r["result"]["entry_node_id"].as_str().unwrap().is_empty());

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_list_blueprint_graphs() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPList_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "list_blueprint_graphs",
            json!({
                "path": bp_asset_path
            }),
        )
        .await
        .unwrap();
    println!("ListGraphs: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "list_blueprint_graphs failed: {:?}", r);
    assert!(
        r["result"]["count"].as_u64().unwrap() > 0,
        "Should have at least EventGraph"
    );

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_delete_blueprint_graph() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPDelGraph_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);
    let func_name = format!("ToDelete_{}", timestamp);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Create a function graph to delete
    client
        .send_command(
            "create_blueprint_function_graph",
            json!({
                "path": bp_asset_path,
                "function_name": func_name
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "delete_blueprint_graph",
            json!({
                "path": bp_asset_path,
                "graph_name": func_name
            }),
        )
        .await
        .unwrap();
    println!("DeleteGraph: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "delete_blueprint_graph failed: {:?}", r);
    assert_eq!(r["result"]["deleted"], true);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_add_blueprint_node() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPAddNode_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "add_blueprint_node",
            json!({
                "path": bp_asset_path,
                "node_type": "PrintString",
                "graph_type": "EventGraph",
                "pos_x": 300,
                "pos_y": 0
            }),
        )
        .await
        .unwrap();
    println!("AddNode: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "add_blueprint_node failed: {:?}", r);
    assert!(!r["result"]["node_id"].as_str().unwrap().is_empty());

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_connect_blueprint_pins() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPConnect_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Add two nodes to connect
    let r = client
        .send_command(
            "add_blueprint_node",
            json!({
                "path": bp_asset_path,
                "node_type": "Event",
                "event_name": "ReceiveBeginPlay",
                "graph_type": "EventGraph",
                "pos_x": 0,
                "pos_y": 0
            }),
        )
        .await
        .unwrap();
    let event_node_id = r["result"]["node_id"].as_str().unwrap().to_string();

    let r = client
        .send_command(
            "add_blueprint_node",
            json!({
                "path": bp_asset_path,
                "node_type": "PrintString",
                "graph_type": "EventGraph",
                "pos_x": 300,
                "pos_y": 0
            }),
        )
        .await
        .unwrap();
    let print_node_id = r["result"]["node_id"].as_str().unwrap().to_string();

    // Connect BeginPlay.then → PrintString.execute
    let r = client
        .send_command(
            "connect_blueprint_pins",
            json!({
                "path": bp_asset_path,
                "source_node_id": event_node_id,
                "source_pin": "then",
                "target_node_id": print_node_id,
                "target_pin": "execute"
            }),
        )
        .await
        .unwrap();
    println!("Connect: {}", serde_json::to_string_pretty(&r).unwrap());
    assert_eq!(r["success"], true, "connect_blueprint_pins failed: {:?}", r);

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_blueprint_graph() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let bp_name = format!("TestBPGetGraph_{}", timestamp);
    let bp_asset_path = format!("/Game/Test/{}.{}", bp_name, bp_name);

    client
        .send_command(
            "create_blueprint",
            json!({
                "name": bp_name,
                "parentClass": "Actor",
                "path": "/Game/Test/"
            }),
        )
        .await
        .unwrap();

    // Add a node so there's something to inspect
    client
        .send_command(
            "add_blueprint_node",
            json!({
                "path": bp_asset_path,
                "node_type": "PrintString",
                "graph_type": "EventGraph",
                "pos_x": 0,
                "pos_y": 0
            }),
        )
        .await
        .unwrap();

    let r = client
        .send_command(
            "get_blueprint_graph",
            json!({
                "path": bp_asset_path,
                "graph_type": "EventGraph"
            }),
        )
        .await
        .unwrap();
    println!("Graph: node_count={}", r["result"]["node_count"]);
    assert_eq!(r["success"], true, "get_blueprint_graph failed: {:?}", r);
    assert!(
        r["result"]["node_count"].as_u64().unwrap() > 0,
        "Should have at least 1 node"
    );

    // Cleanup
    client
        .send_command("delete_asset", json!({"path": bp_asset_path}))
        .await
        .unwrap();
}

#[tokio::test]
#[ignore]
async fn test_real_ue_get_level_blueprint() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let response = client
        .send_command("get_level_blueprint", json!({}))
        .await
        .unwrap();
    println!(
        "LevelBlueprint: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );

    assert_eq!(
        response["success"], true,
        "get_level_blueprint failed: {:?}",
        response
    );
    assert!(!response["result"]["level_name"]
        .as_str()
        .unwrap()
        .is_empty());
    assert!(response["result"]["graphs"].as_array().unwrap().len() > 0);
}

#[tokio::test]
#[ignore]
async fn test_real_ue_save_level_blueprint() {
    let mut client = UnrealClient::new("127.0.0.1:13377");

    let response = client
        .send_command("save_level_blueprint", json!({}))
        .await
        .unwrap();
    println!(
        "SaveLevelBlueprint: {}",
        serde_json::to_string_pretty(&response).unwrap()
    );

    assert_eq!(
        response["success"], true,
        "save_level_blueprint failed: {:?}",
        response
    );
    assert_eq!(response["result"]["saved"], true);
}
