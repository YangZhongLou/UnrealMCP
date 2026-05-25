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
