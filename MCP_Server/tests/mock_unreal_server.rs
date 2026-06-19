use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use serde_json::{json, Value};

fn build_response(req: &Value) -> Value {
    let method = req["method"].as_str().unwrap_or("unknown");
    match method {
        "get_editor_info" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "engine_version": "5.3.2",
                "project_name": "TestProject"
            }
        }),
        "spawn_actor" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "actor_name": req["params"]["name"].as_str().unwrap_or("Actor_0"),
                "class": req["params"]["className"]
            }
        }),
        "get_actor_list" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "actors": [
                    {"name": "Actor_0", "class": "StaticMeshActor"},
                    {"name": "Light_0", "class": "PointLight"}
                ]
            }
        }),
        "destroy_actor" => json!({
            "id": req["id"],
            "success": true,
            "result": {"destroyed": true}
        }),
        "set_actor_property" => json!({
            "id": req["id"],
            "success": true,
            "result": {"set": true}
        }),
        "get_actor_property" => json!({
            "id": req["id"],
            "success": true,
            "result": {"value": 5000.0}
        }),
        "duplicate_actor" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "actor_name": req["params"]["newName"].as_str().unwrap_or("DuplicatedActor"),
                "source": req["params"]["name"]
            }
        }),
        "open_level" => json!({
            "id": req["id"],
            "success": true,
            "result": {"opened": true}
        }),
        "create_level" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "path": req["params"]["path"].as_str().unwrap_or("/Game/Maps/NewMap"),
                "created": true
            }
        }),
        "get_asset_list" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "assets": [
                    {"name": "BP_Player", "class": "Blueprint", "path": "/Game/BP_Player"},
                    {"name": "M_Ground", "class": "Material", "path": "/Game/M_Ground"}
                ],
                "count": 2
            }
        }),
        "get_asset_info" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "name": "BP_Player",
                "path": "/Game/BP_Player",
                "class": "Blueprint",
                "package_path": "/Game"
            }
        }),
        "delete_asset" => json!({
            "id": req["id"],
            "success": true,
            "result": {"deleted": true}
        }),
        "rename_asset" => json!({
            "id": req["id"],
            "success": true,
            "result": {"renamed": true}
        }),
        "create_material" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "path": req["params"]["path"].as_str().unwrap_or("/Game/Materials/NewMat"),
                "assetName": req["params"]["path"].as_str()
                    .and_then(|p| p.rsplit('/').next())
                    .unwrap_or("NewMat"),
                "shadingModel": req["params"].get("shadingModel")
            }
        }),
        "set_viewport_camera" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "location": req["params"]["location"].clone(),
                "rotation": req["params"]["rotation"].clone()
            }
        }),
        "get_runtime_camera_state" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "location": [0.0, 0.0, 0.0],
                "rotation": [-45.0, 45.0, 0.0],
                "zoom": 2500.0,
                "fov": 90.0,
                "dof_focal_distance": 10000.0,
                "exposure": 0.0,
                "bloom": 0.675,
                "focalLength": 50.0,
                "aperture": 2.8,
                "focusDistance": 10000.0
            }
        }),
        "set_runtime_camera_fov" => json!({
            "id": req["id"],
            "success": true,
            "result": {"fov": req["params"]["fov"]}
        }),
        "set_runtime_camera_dof" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "focal_distance": req["params"]["focalDistance"]
            }
        }),
        "set_runtime_camera_post_process" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "exposure": req["params"].get("exposure").unwrap_or(&json!(0.0)),
                "bloom": req["params"].get("bloom").unwrap_or(&json!(0.675))
            }
        }),
        "set_runtime_camera_transform" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "location": req["params"]["location"].clone(),
                "zoom": req["params"]["zoom"].clone()
            }
        }),
        "focus_runtime_camera_on_actor" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "actor_name": req["params"]["actorName"],
                "target_location": [0.0, 0.0, 100.0]
            }
        }),
        "set_runtime_camera_focal_length" => json!({
            "id": req["id"],
            "success": true,
            "result": {"focalLength": req["params"]["focalLength"]}
        }),
        "set_runtime_camera_aperture" => json!({
            "id": req["id"],
            "success": true,
            "result": {"aperture": req["params"]["aperture"]}
        }),
        "set_runtime_camera_focus_distance" => json!({
            "id": req["id"],
            "success": true,
            "result": {"focusDistance": req["params"]["focusDistance"]}
        }),
        "start_camera_rig" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "rig_name": req["params"]["rigName"],
                "playing": true
            }
        }),
        "stop_camera_rig" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "rig_name": req["params"]["rigName"],
                "playing": false
            }
        }),
        "set_camera_rig_speed" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "rig_name": req["params"]["rigName"],
                "speed": req["params"]["speed"]
            }
        }),
        "switch_camera" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "camera_name": req["params"]["cameraName"],
                "current_camera": req["params"]["cameraName"],
                "blend_time": req["params"].get("blendTime").unwrap_or(&json!(1.0))
            }
        }),
        "next_camera" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "current_camera": "Camera_1",
                "blend_time": req["params"].get("blendTime").unwrap_or(&json!(1.0))
            }
        }),
        "prev_camera" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "current_camera": "Camera_0",
                "blend_time": req["params"].get("blendTime").unwrap_or(&json!(1.0))
            }
        }),
        "get_camera_list" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "cameras": ["Camera_0", "Camera_1", "Camera_2"],
                "current_camera": "Camera_0",
                "count": 3
            }
        }),
        "set_runtime_camera_motion_blur" => json!({
            "id": req["id"],
            "success": true,
            "result": {"motionBlur": req["params"]["amount"]}
        }),
        "set_runtime_camera_vignette" => json!({
            "id": req["id"],
            "success": true,
            "result": {"vignette": req["params"]["intensity"]}
        }),
        "set_runtime_camera_chromatic_aberration" => json!({
            "id": req["id"],
            "success": true,
            "result": {"chromaticAberration": req["params"]["intensity"]}
        }),
        "get_level_blueprint" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "level_name": "TestLevel",
                "blueprint_path": "/Game/Maps/TestLevel_BuiltData.TestLevel_BuiltData",
                "graphs": [{
                    "name": "EventGraph",
                    "type": "EventGraph",
                    "node_count": 2,
                    "nodes": [
                        {
                            "node_id": "A1B2C3D4",
                            "class": "K2Node_Event",
                            "title": "Event BeginPlay",
                            "pos_x": 0.0,
                            "pos_y": 0.0,
                            "pins": [
                                {"name": "then", "direction": "output", "category": "exec"}
                            ]
                        },
                        {
                            "node_id": "E5F6G7H8",
                            "class": "K2Node_CallFunction",
                            "title": "Print String",
                            "pos_x": 200.0,
                            "pos_y": 0.0,
                            "pins": [
                                {"name": "execute", "direction": "input", "category": "exec"},
                                {"name": "InString", "direction": "input", "category": "string"}
                            ]
                        }
                    ]
                }]
            }
        }),
        "remove_blueprint_nodes" => json!({
            "id": req["id"],
            "success": true,
            "result": {
                "removed_count": req["params"]["node_ids"].as_array().map(|a| a.len()).unwrap_or(0) as i32,
                "saved": true
            }
        }),
        "save_level_blueprint" => json!({
            "id": req["id"],
            "success": true,
            "result": {"saved": true}
        }),
        _ => json!({
            "id": req["id"],
            "success": false,
            "error": format!("Unknown method: {}", method)
        }),
    }
}

async fn handle_mock_client(mut socket: TcpStream) {
    let mut buf = vec![0u8; 65536];
    match socket.read(&mut buf).await {
        Ok(n) if n > 0 => {
            let req_str = String::from_utf8_lossy(&buf[..n]);
            if let Ok(req) = serde_json::from_str::<Value>(req_str.trim()) {
                let response = build_response(&req);
                let resp_str = response.to_string() + "\n\n";
                let _ = socket.write_all(resp_str.as_bytes()).await;
            }
        }
        _ => {}
    }
}

pub struct MockUnrealServer {
    shutdown: tokio::sync::mpsc::Sender<()>,
}

impl MockUnrealServer {
    pub async fn start(port: u16) -> (Self, u16) {
        let listener = TcpListener::bind(format!("127.0.0.1:{}", port)).await.unwrap();
        let bound_port = listener.local_addr().unwrap().port();
        let (tx, mut rx) = tokio::sync::mpsc::channel::<()>(1);

        tokio::spawn(async move {
            loop {
                tokio::select! {
                    _ = rx.recv() => break,
                    result = listener.accept() => {
                        if let Ok((socket, _)) = result {
                            tokio::spawn(handle_mock_client(socket));
                        }
                    }
                }
            }
        });

        (Self { shutdown: tx }, bound_port)
    }

    pub async fn stop(self) {
        let _ = self.shutdown.send(()).await;
    }
}
