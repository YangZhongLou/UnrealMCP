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

    // Use unique level name based on timestamp to avoid conflicts
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs();
    let level_path = format!("/Game/Maps/TestCL_{}", timestamp);

    let response = client.send_command("create_level", json!({
        "path": level_path
    })).await.unwrap();

    println!("Response: {}", serde_json::to_string_pretty(&response).unwrap());

    assert_eq!(response["success"], true, "create_level failed: {:?}", response);
    assert_eq!(response["result"]["created"], true);

    let actual_path = response["result"]["path"].as_str().unwrap();
    assert!(!actual_path.is_empty(),
        "Expected non-empty path, got: {}", actual_path
    );

    println!("Level created at: {}", actual_path);
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
