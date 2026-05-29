use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use serde_json::{json, Value};

pub struct MockUnrealServer {
    addr: String,
    shutdown: tokio::sync::mpsc::Sender<()>,
}

impl MockUnrealServer {
    pub async fn start(port: u16) -> Self {
        let listener = TcpListener::bind(format!("127.0.0.1:{}", port)).await.unwrap();
        let addr = listener.local_addr().unwrap().to_string();
        let (tx, mut rx) = tokio::sync::mpsc::channel::<()>(1);

        tokio::spawn(async move {
            let (mut socket, _) = listener.accept().await.unwrap();

            let mut buf = vec![0u8; 65536];
            loop {
                tokio::select! {
                    _ = rx.recv() => break,
                    result = socket.read(&mut buf) => {
                        match result {
                            Ok(0) => break,
                            Ok(n) => {
                                let req_str = String::from_utf8_lossy(&buf[..n]);
                                let req: Value = serde_json::from_str(&req_str.trim()).unwrap();
                                let method = req["method"].as_str().unwrap_or("unknown");

                                let response = match method {
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
                                    _ => json!({
                                        "id": req["id"],
                                        "success": false,
                                        "error": format!("Unknown method: {}", method)
                                    }),
                                };

                                let resp_str = response.to_string() + "\n";
                                socket.write_all(resp_str.as_bytes()).await.unwrap();
                            }
                            Err(_) => break,
                        }
                    }
                }
            }
        });

        Self { addr, shutdown: tx }
    }

    pub async fn stop(self) {
        let _ = self.shutdown.send(()).await;
    }
}
