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

    info!("Starting Unreal MCP Server...");

    let service = UnrealMcpServer::new("127.0.0.1:13377").await?;
    let transport = rmcp::transport::stdio();
    let server = service.serve(transport).await?;

    info!("MCP Server initialized, waiting for requests...");

    let quit_reason = server.waiting().await?;
    info!("Server quit: {:?}", quit_reason);

    Ok(())
}
