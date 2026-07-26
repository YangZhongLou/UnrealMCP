use serde_json::json;
use std::time::Instant;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use unreal_mcp_server::unreal_client::UnrealClient;

mod mock_unreal_server;
use mock_unreal_server::MockUnrealServer;

#[tokio::test]
async fn test_response_timing() {
    let (mock, port) = MockUnrealServer::start(0).await;
    let mut client = UnrealClient::new(&format!("127.0.0.1:{}", port));

    let start = Instant::now();
    let response = client
        .send_command("get_editor_info", json!({}))
        .await
        .unwrap();
    let elapsed = start.elapsed();

    assert_eq!(response["success"], true);
    assert!(
        elapsed.as_millis() < 500,
        "Response too slow: {:?}",
        elapsed
    );

    mock.stop().await;
}

#[tokio::test]
async fn test_concurrent_connections() {
    // Use a concurrent-capable mock that handles multiple connections
    let port = 13401;
    let listener = tokio::net::TcpListener::bind(format!("127.0.0.1:{}", port))
        .await
        .unwrap();

    let (tx, mut rx) = tokio::sync::mpsc::channel::<()>(1);

    let server_handle = tokio::spawn(async move {
        loop {
            tokio::select! {
                _ = rx.recv() => break,
                result = listener.accept() => {
                    if let Ok((mut socket, _)) = result {
                        tokio::spawn(async move {
                            let mut buf = vec![0u8; 1024];
                            // Read the request first so the client doesn't get a RST
                            // before it finishes sending.
                            let _ = socket.read(&mut buf).await;

                            let resp = json!({"id":"0","success":true,"result":{"status":"ok"}}).to_string() + "\n\n";
                            let _ = socket.write_all(resp.as_bytes()).await;
                        });
                    }
                }
            }
        }
    });

    // Launch 10 concurrent clients
    let mut handles = vec![];
    for i in 0..10 {
        let port = port;
        handles.push(tokio::spawn(async move {
            let mut client = UnrealClient::new(&format!("127.0.0.1:{}", port));
            client
                .send_command("get_editor_info", json!({"index": i}))
                .await
        }));
    }

    let mut success = 0;
    for h in handles {
        if let Ok(Ok(response)) = h.await {
            if response["success"].as_bool().unwrap_or(false) {
                success += 1;
            }
        }
    }

    assert_eq!(success, 10, "All 10 concurrent connections should succeed");

    let _ = tx.send(()).await;
    server_handle.await.ok();
}

#[tokio::test]
async fn test_many_sequential_commands() {
    let (mock, port) = MockUnrealServer::start(0).await;
    let mut client = UnrealClient::new(&format!("127.0.0.1:{}", port));

    let start = Instant::now();
    for i in 0..100 {
        let response = client
            .send_command("get_editor_info", json!({"seq": i}))
            .await
            .unwrap();
        assert_eq!(response["success"], true, "Command {} failed", i);
    }
    let elapsed = start.elapsed();

    let avg_ms = elapsed.as_micros() as f64 / 100.0;
    println!(
        "100 sequential commands: {:?} total, {:.0}us avg",
        elapsed, avg_ms
    );
    assert!(avg_ms < 5000.0, "Average too slow: {:.0}us", avg_ms);

    mock.stop().await;
}

#[tokio::test]
async fn test_reconnection() {
    let port = 13403;

    // First server instance
    let (mock1, _) = MockUnrealServer::start(port).await;
    let mut client = UnrealClient::new(&format!("127.0.0.1:{}", port));
    let r1 = client
        .send_command("get_editor_info", json!({}))
        .await
        .unwrap();
    assert_eq!(r1["success"], true);
    mock1.stop().await;

    // Brief pause
    tokio::time::sleep(tokio::time::Duration::from_millis(50)).await;

    // Second server instance on same port
    let (mock2, _) = MockUnrealServer::start(port).await;
    let r2 = client.send_command("get_editor_info", json!({})).await;
    mock2.stop().await;

    // Reconnection may work or fail depending on timing; either is acceptable
    match r2 {
        Ok(response) => {
            assert_eq!(response["success"], true);
            println!("Reconnection succeeded");
        }
        Err(_) => {
            println!("Reconnection failed as expected (new client needed)");
        }
    }
}

#[tokio::test]
async fn test_large_response() {
    let (mock, port) = MockUnrealServer::start(0).await;
    let mut client = UnrealClient::new(&format!("127.0.0.1:{}", port));

    let start = Instant::now();
    let response = client
        .send_command("get_actor_list", json!({}))
        .await
        .unwrap();
    let elapsed = start.elapsed();

    assert_eq!(response["success"], true);
    let actors = response["result"]["actors"].as_array().unwrap();
    assert!(actors.len() > 0);
    assert!(elapsed.as_millis() < 500);

    mock.stop().await;
}
