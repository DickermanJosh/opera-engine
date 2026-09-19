//! Opera UCI executable: async protocol I/O with worker-owned C++ search.
#![deny(unsafe_code)]
use anyhow::Result;
fn main() -> Result<()> {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()?;
    let result = runtime.block_on(run());
    // The event loop joins its search worker before returning. Tokio's stdio
    // blocking tasks cannot be cancelled, however: a GUI that stops reading can
    // leave a stdout write blocked after its async timeout. Do not wait forever
    // for that OS write while tearing down this executable's runtime.
    runtime.shutdown_timeout(std::time::Duration::from_millis(100));
    result
}

async fn run() -> Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env().unwrap_or_else(|_| "warn".into()),
        )
        .with_writer(std::io::stderr)
        .with_ansi(false)
        .init();
    let args: Vec<String> = std::env::args().skip(1).collect();
    if args == ["--version"] {
        println!("{} {}", opera_uci::NAME, opera_uci::VERSION);
        return Ok(());
    }
    let engine = std::sync::Arc::new(opera_uci::uci::UCIEngine::new());
    let mut i = 0;
    while i < args.len() {
        if args[i] == "--debug" {
            engine.process_command("debug on").await?;
            i += 1;
            continue;
        }
        let name = match args[i].as_str() {
            "--hash-size" => "Hash",
            "--threads" => "Threads",
            "--morphy-style" => "MorphyStyle",
            _ => anyhow::bail!(
                "Unsupported argument {}; use UCI setoption for supported options",
                args[i]
            ),
        };
        let value = args
            .get(i + 1)
            .ok_or_else(|| anyhow::anyhow!("Missing value for {}", args[i]))?;
        engine
            .process_command(&format!("setoption name {name} value {value}"))
            .await?;
        i += 2;
    }
    opera_uci::uci::UCIEventLoop::new(engine)?.run().await?;
    Ok(())
}
