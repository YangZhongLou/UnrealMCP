use unreal_mcp_server::unreal_client::UnrealClient;
use serde_json::json;

mod mock_unreal_server;
use mock_unreal_server::MockUnrealServer;

#[tokio::test]
async fn test_tcp_connection_and_command() {
    let mock = MockUnrealServer::start(13378).await;

    let mut client = UnrealClient::new("127.0.0.1:13378");
    let response = client.send_command("get_editor_info", json!({})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["engine_version"], "5.3.2");

    mock.stop().await;
}

#[tokio::test]
async fn test_get_asset_list() {
    let mock = MockUnrealServer::start(13384).await;

    let mut client = UnrealClient::new("127.0.0.1:13384");
    let response = client.send_command("get_asset_list", json!({
        "path": "/Game"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    let assets = response["result"]["assets"].as_array().unwrap();
    assert_eq!(assets.len(), 2);
    assert_eq!(assets[0]["name"], "BP_Player");

    mock.stop().await;
}

#[tokio::test]
async fn test_get_asset_info() {
    let mock = MockUnrealServer::start(13385).await;

    let mut client = UnrealClient::new("127.0.0.1:13385");
    let response = client.send_command("get_asset_info", json!({
        "path": "/Game/BP_Player"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["name"], "BP_Player");
    assert_eq!(response["result"]["class"], "Blueprint");

    mock.stop().await;
}

#[tokio::test]
async fn test_delete_and_rename_asset() {
    let mock = MockUnrealServer::start(13386).await;

    let mut client = UnrealClient::new("127.0.0.1:13386");

    // delete
    let response = client.send_command("delete_asset", json!({
        "path": "/Game/OldAsset"
    })).await.unwrap();
    assert_eq!(response["success"], true);

    // rename
    let response = client.send_command("rename_asset", json!({
        "path": "/Game/OldName",
        "newName": "NewName"
    })).await.unwrap();
    assert_eq!(response["success"], true);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_and_get_actor_property() {
    let mock = MockUnrealServer::start(13381).await;

    let mut client = UnrealClient::new("127.0.0.1:13381");

    // set property
    let response = client.send_command("set_actor_property", json!({
        "actorName": "TestLight",
        "propertyName": "Intensity",
        "value": {"FloatValue": 5000.0}
    })).await.unwrap();
    assert_eq!(response["success"], true);

    // get property
    let response = client.send_command("get_actor_property", json!({
        "actorName": "TestLight",
        "propertyName": "Intensity"
    })).await.unwrap();
    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["value"], 5000.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_duplicate_actor() {
    let mock = MockUnrealServer::start(13382).await;

    let mut client = UnrealClient::new("127.0.0.1:13382");
    let response = client.send_command("duplicate_actor", json!({
        "name": "TestLight",
        "newName": "TestLight_Copy"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["actor_name"], "TestLight_Copy");

    mock.stop().await;
}

#[tokio::test]
async fn test_open_level() {
    let mock = MockUnrealServer::start(13383).await;

    let mut client = UnrealClient::new("127.0.0.1:13383");
    let response = client.send_command("open_level", json!({
        "path": "/Game/Maps/TestMap"
    })).await.unwrap();

    assert_eq!(response["success"], true);

    mock.stop().await;
}

#[tokio::test]
async fn test_spawn_actor() {
    let mock = MockUnrealServer::start(13379).await;

    let mut client = UnrealClient::new("127.0.0.1:13379");
    let response = client.send_command("spawn_actor", json!({
        "className": "PointLight",
        "name": "TestLight"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["actor_name"], "TestLight");

    mock.stop().await;
}

#[tokio::test]
async fn test_get_actor_list() {
    let mock = MockUnrealServer::start(13380).await;

    let mut client = UnrealClient::new("127.0.0.1:13380");
    let response = client.send_command("get_actor_list", json!({})).await.unwrap();

    assert_eq!(response["success"], true);
    let actors = response["result"]["actors"].as_array().unwrap();
    assert_eq!(actors.len(), 2);

    mock.stop().await;
}

#[tokio::test]
async fn test_create_level() {
    let mock = MockUnrealServer::start(13387).await;

    let mut client = UnrealClient::new("127.0.0.1:13387");
    let response = client.send_command("create_level", json!({
        "path": "/Game/Maps/TestLevel"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["created"], true);
    assert_eq!(response["result"]["path"], "/Game/Maps/TestLevel");

    mock.stop().await;
}
