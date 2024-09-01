use crate::cli::EditorCommands;
use crate::repository::Repository;
use exitfailure::ExitFailure;
use paris::{info, warn};
use std::env;

pub struct ActionEditor {}

impl ActionEditor {
    pub fn run(subcommand: Option<EditorCommands>) -> Result<Repository, ExitFailure>
    {
        Ok(match subcommand {
        None => {
            let repos = Repository::new(env::current_dir()?)?;
            if repos.get_editor_command().is_empty() {
                warn!("Editor executable path is not set");
            } else {
                info!("{}", repos.get_editor_command());
            }
            repos
        }
        Some(remote) => {
            match remote {
                EditorCommands::Set { url } => {
                    let mut repos = Repository::new(env::current_dir()?)?;
                    repos.set_editor_command(url);
                    info!("Updated editor executable path to '{}'", repos.get_editor_command());
                    repos
                }
            }
        }
    })
    }
}