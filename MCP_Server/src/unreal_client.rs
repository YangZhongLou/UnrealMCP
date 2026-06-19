use serde_json::{json, Value};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tracing::{info, error};
use anyhow::Result;

#[derive(Debug)]
pub struct UnrealClient {
    addr: String,
    stream: Option<TcpStream>,
}

impl UnrealClient {
    pub fn new(addr: &str) -> Self {
        Self {
            addr: addr.to_string(),
            stream: None,
        }
    }

    pub async fn connect(&mut self) -> Result<()> {
        match TcpStream::connect(&self.addr).await {
            Ok(stream) => {
                info!("Connected to Unreal at {}", self.addr);
                self.stream = Some(stream);
                Ok(())
            }
            Err(e) => {
                error!("Failed to connect to Unreal: {}", e);
                Err(e.into())
            }
        }
    }

    pub fn is_connected(&self) -> bool {
        self.stream.is_some()
    }

    pub async fn send_command(&mut self, method: &str, params: Value) -> Result<Value> {
        if self.stream.is_none() {
            self.connect().await?;
        }

        let request = json!({
            "id": format!("cmd_{}_{}", std::process::id(), method),
            "method": method,
            "params": params
        });

        let message = request.to_string() + "\n";
        let stream = self.stream.as_mut().unwrap();

        stream.write_all(message.as_bytes()).await?;

        // The UE Command Server frames each response with a trailing "\n\n".
        // Responses are pretty-printed JSON and may contain their own single
        // newlines, so we read until we see the double-newline delimiter rather
        // than stopping at the first '\n'.
        let mut buffer = Vec::new();
        let mut temp = [0u8; 8192];
        loop {
            let n = stream.read(&mut temp).await?;
            if n == 0 {
                self.stream = None;
                return Err(anyhow::anyhow!("Connection closed by Unreal"));
            }
            buffer.extend_from_slice(&temp[..n]);
            if buffer.windows(2).any(|w| w == b"\n\n") {
                break;
            }
        }

        let response_str = String::from_utf8_lossy(&buffer);
        let json_str = match response_str.find("\n\n") {
            Some(end) => &response_str[..end],
            None => response_str.trim(),
        };
        let response: Value = serde_json::from_str(json_str.trim())?;

        // The server closes the connection after one response. Drop our side so
        // the next command opens a fresh connection.
        self.stream = None;

        Ok(response)
    }
}
