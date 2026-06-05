use rmcp::ServiceExt;
use tracing::info;

mod server;
mod unreal_client;

use server::UnrealMcpServer;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_max_level(tracing::Level::INFO)
        .init();

    let unreal_addr = std::env::var("UNREAL_MCP_ADDR")
        .unwrap_or_else(|_| "127.0.0.1:13377".to_string());

    info!("Starting Unreal MCP Server...");
    info!("Connecting to Unreal at: {}", unreal_addr);

    let service = UnrealMcpServer::new(&unreal_addr).await?;
    let transport = rmcp::transport::stdio();
    let server = service.serve(transport).await?;

    info!("MCP Server initialized, waiting for requests...");

    let quit_reason = server.waiting().await?;
    info!("Server quit: {:?}", quit_reason);

    Ok(())
}
