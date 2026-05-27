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
