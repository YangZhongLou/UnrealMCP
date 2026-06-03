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
async fn test_create_material() {
    let mock = MockUnrealServer::start(13388).await;

    let mut client = UnrealClient::new("127.0.0.1:13388");
    let response = client.send_command("create_material", json!({
        "path": "/Game/Materials/JadeMaterial",
        "shadingModel": "subsurface_profile",
        "baseColor": [0.1, 0.8, 0.3],
        "roughness": 0.3,
        "reuse": true
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["path"], "/Game/Materials/JadeMaterial");

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

#[tokio::test]
async fn test_set_viewport_camera() {
    let mock = MockUnrealServer::start(13389).await;

    let mut client = UnrealClient::new("127.0.0.1:13389");
    let response = client.send_command("set_viewport_camera", json!({
        "location": [100.0, 200.0, 300.0],
        "rotation": [-45.0, 45.0, 0.0]
    })).await.unwrap();

    assert_eq!(response["success"], true);
    let loc = response["result"]["location"].as_array().unwrap();
    assert_eq!(loc[0], 100.0);
    assert_eq!(loc[1], 200.0);
    assert_eq!(loc[2], 300.0);
    let rot = response["result"]["rotation"].as_array().unwrap();
    assert_eq!(rot[0], -45.0);
    assert_eq!(rot[1], 45.0);
    assert_eq!(rot[2], 0.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_get_runtime_camera_state() {
    let mock = MockUnrealServer::start(13390).await;

    let mut client = UnrealClient::new("127.0.0.1:13390");
    let response = client.send_command("get_runtime_camera_state", json!({})).await.unwrap();

    assert_eq!(response["success"], true);
    assert!(response["result"].get("fov").is_some());
    assert!(response["result"].get("zoom").is_some());
    assert!(response["result"].get("exposure").is_some());

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_fov() {
    let mock = MockUnrealServer::start(13391).await;

    let mut client = UnrealClient::new("127.0.0.1:13391");
    let response = client.send_command("set_runtime_camera_fov", json!({"fov": 60.0})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["fov"], 60.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_dof() {
    let mock = MockUnrealServer::start(13392).await;

    let mut client = UnrealClient::new("127.0.0.1:13392");
    let response = client.send_command("set_runtime_camera_dof", json!({
        "focalDistance": 500.0,
        "focalRegion": 200.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["focal_distance"], 500.0);
    assert_eq!(response["result"]["focal_region"], 200.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_post_process() {
    let mock = MockUnrealServer::start(13393).await;

    let mut client = UnrealClient::new("127.0.0.1:13393");
    let response = client.send_command("set_runtime_camera_post_process", json!({
        "exposure": 2.0,
        "bloom": 1.5
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["exposure"], 2.0);
    assert_eq!(response["result"]["bloom"], 1.5);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_transform() {
    let mock = MockUnrealServer::start(13394).await;

    let mut client = UnrealClient::new("127.0.0.1:13394");
    let response = client.send_command("set_runtime_camera_transform", json!({
        "location": [1000.0, 2000.0, 0.0],
        "zoom": 1500.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    let loc = response["result"]["location"].as_array().unwrap();
    assert_eq!(loc[0], 1000.0);
    assert_eq!(loc[1], 2000.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_focus_runtime_camera_on_actor() {
    let mock = MockUnrealServer::start(13395).await;

    let mut client = UnrealClient::new("127.0.0.1:13395");
    let response = client.send_command("focus_runtime_camera_on_actor", json!({
        "actorName": "TestCube"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["actor_name"], "TestCube");

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_focal_length() {
    let mock = MockUnrealServer::start(13396).await;

    let mut client = UnrealClient::new("127.0.0.1:13396");
    let response = client.send_command("set_runtime_camera_focal_length", json!({
        "focalLength": 85.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["focalLength"], 85.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_aperture() {
    let mock = MockUnrealServer::start(13397).await;

    let mut client = UnrealClient::new("127.0.0.1:13397");
    let response = client.send_command("set_runtime_camera_aperture", json!({
        "aperture": 1.4
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["aperture"], 1.4);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_focus_distance() {
    let mock = MockUnrealServer::start(13398).await;

    let mut client = UnrealClient::new("127.0.0.1:13398");
    let response = client.send_command("set_runtime_camera_focus_distance", json!({
        "focusDistance": 500.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["focusDistance"], 500.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_start_camera_rig() {
    let mock = MockUnrealServer::start(13399).await;

    let mut client = UnrealClient::new("127.0.0.1:13399");
    let response = client.send_command("start_camera_rig", json!({
        "rigName": "CameraRig_0"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["rig_name"], "CameraRig_0");
    assert_eq!(response["result"]["playing"], true);

    mock.stop().await;
}

#[tokio::test]
async fn test_stop_camera_rig() {
    let mock = MockUnrealServer::start(13400).await;

    let mut client = UnrealClient::new("127.0.0.1:13400");
    let response = client.send_command("stop_camera_rig", json!({
        "rigName": "CameraRig_0"
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["rig_name"], "CameraRig_0");
    assert_eq!(response["result"]["playing"], false);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_camera_rig_speed() {
    let mock = MockUnrealServer::start(13401).await;

    let mut client = UnrealClient::new("127.0.0.1:13401");
    let response = client.send_command("set_camera_rig_speed", json!({
        "rigName": "CameraRig_0",
        "speed": 1000.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["speed"], 1000.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_switch_camera() {
    let mock = MockUnrealServer::start(13402).await;

    let mut client = UnrealClient::new("127.0.0.1:13402");
    let response = client.send_command("switch_camera", json!({
        "cameraName": "Camera_1",
        "blendTime": 2.0
    })).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["camera_name"], "Camera_1");

    mock.stop().await;
}

#[tokio::test]
async fn test_next_camera() {
    let mock = MockUnrealServer::start(13403).await;

    let mut client = UnrealClient::new("127.0.0.1:13403");
    let response = client.send_command("next_camera", json!({})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["current_camera"], "Camera_1");

    mock.stop().await;
}

#[tokio::test]
async fn test_prev_camera() {
    let mock = MockUnrealServer::start(13404).await;

    let mut client = UnrealClient::new("127.0.0.1:13404");
    let response = client.send_command("prev_camera", json!({"blendTime": 0.5})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["current_camera"], "Camera_0");

    mock.stop().await;
}

#[tokio::test]
async fn test_get_camera_list() {
    let mock = MockUnrealServer::start(13405).await;

    let mut client = UnrealClient::new("127.0.0.1:13405");
    let response = client.send_command("get_camera_list", json!({})).await.unwrap();

    assert_eq!(response["success"], true);
    let cameras = response["result"]["cameras"].as_array().unwrap();
    assert_eq!(cameras.len(), 3);
    assert_eq!(response["result"]["count"], 3);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_motion_blur() {
    let mock = MockUnrealServer::start(13406).await;

    let mut client = UnrealClient::new("127.0.0.1:13406");
    let response = client.send_command("set_runtime_camera_motion_blur", json!({"amount": 0.5})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["motionBlur"], 0.5);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_vignette() {
    let mock = MockUnrealServer::start(13407).await;

    let mut client = UnrealClient::new("127.0.0.1:13407");
    let response = client.send_command("set_runtime_camera_vignette", json!({"intensity": 2.0})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["vignette"], 2.0);

    mock.stop().await;
}

#[tokio::test]
async fn test_set_runtime_camera_chromatic_aberration() {
    let mock = MockUnrealServer::start(13408).await;

    let mut client = UnrealClient::new("127.0.0.1:13408");
    let response = client.send_command("set_runtime_camera_chromatic_aberration", json!({"intensity": 1.5})).await.unwrap();

    assert_eq!(response["success"], true);
    assert_eq!(response["result"]["chromaticAberration"], 1.5);

    mock.stop().await;
}
