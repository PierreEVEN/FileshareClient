mod repository;
mod cli;
mod actions;
pub mod client_string;
pub mod content;
pub mod serialization_utils;

use exitfailure::ExitFailure;
use std::env;

use crate::actions::clone::ActionClone;
use crate::actions::editor::ActionEditor;
use crate::actions::init::ActionInit;
use crate::actions::login::ActionLogin;
use crate::actions::logout::ActionLogout;
use crate::actions::pull::ActionPull;
use crate::actions::push::ActionPush;
use crate::actions::remote::ActionRemote;
use crate::actions::status::ActionStatus;
use crate::actions::sync::ActionSync;
use crate::cli::{FileshareArgs, RootCommands};
use clap::Parser;
use paris::error;

#[tokio::main]
async fn main() -> Result<(), ExitFailure> {
    let args = FileshareArgs::parse();
    if env::current_dir()?.join(".fileshare").join("config.lock.json").exists() {
        error!("A fileshare process is already running here. Please ensure it is stopped then remove the config.lock.json file");
        return Err(ExitFailure::from(failure::err_msg("A fileshare process is already running here.")));
    }

    match match args.commands {
        RootCommands::Clone { repository_url } => {
            ActionClone::run(repository_url)
        }
        RootCommands::Init => {
            ActionInit::run()
        }
        RootCommands::Push => {
            ActionPush::run()
        }
        RootCommands::Pull => {
            ActionPull::run()
        }
        RootCommands::Sync => {
            ActionSync::run()
        }
        RootCommands::Status => {
            ActionStatus::run().await
        }
        RootCommands::Logout => {
            ActionLogout::run().await
        }
        RootCommands::Login { name } => {
            ActionLogin::run(name).await
        }
        RootCommands::Editor { editor } => {
            ActionEditor::run(editor)
        }
        RootCommands::Remote { remote } => {
            ActionRemote::run(remote)
        }
    } {
        Ok(_) => {}
        Err(error) => {
            error!("{:?}", error);
        }
    }
    Ok(())
}
