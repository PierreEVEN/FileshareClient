mod repository;
mod cli;
mod actions;
pub mod client_string;
pub mod content;
pub mod serialization_utils;

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
use failure::Error;
use paris::error;
use std::env;

#[tokio::main]
async fn main() -> Result<(), Error> {
    let args = FileshareArgs::parse();
    if env::current_dir()?.join(".fileshare").join("config.lock.json").exists() {
        error!("A fileshare process is already running here. Please ensure it is stopped then remove the config.lock.json file");
        return Err(failure::err_msg("A fileshare process is already running here."));
    }

    match match args.commands {
        RootCommands::Clone { repository_url } => {
            ActionClone::run(repository_url).await
        }
        RootCommands::Init => {
            ActionInit::run()
        }
        RootCommands::Push => {
            ActionPush::run().await
        }
        RootCommands::Pull => {
            ActionPull::run().await
        }
        RootCommands::Sync => {
            ActionSync::run().await
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

    /*
    let mut cmd = FileshareArgs::command();
    fn print_completions(gen: Shell, cmd: &mut clap::Command) -> Result<(), Error> {
        let install_dir = env::current_exe()?.parent().unwrap().join("cli");
        let file_dir = install_dir.join(cmd.get_name());
        info!("Generating completion file for {} to {}", Shell::PowerShell, install_dir.display());
        fs::create_dir_all(install_dir.clone())?;
        generate_to(gen, cmd, cmd.get_name().to_string(), install_dir)?;

        match gen {
            Shell::Bash => {}
            Shell::Elvish => {}
            Shell::Fish => {}
            Shell::PowerShell => {
                let output = Command::new("powershell").args(&[file_dir]).output().expect("Failed to install powershell commands");
                match &output.status.success() {
                    true => { success!("Successfully installed powershell commands") }
                    false => { error!("Failed to install powershell commands : {}", output.status) }
                };
            }
            Shell::Zsh => {}
            _ => {}
        }

        Ok(())
    }

    print_completions(Shell::Bash, &mut cmd)?;
    print_completions(Shell::Elvish, &mut cmd)?;
    print_completions(Shell::Fish, &mut cmd)?;
    print_completions(Shell::PowerShell, &mut cmd)?;
    print_completions(Shell::Zsh, &mut cmd)?;
*/
    Ok(())
}
