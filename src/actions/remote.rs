use crate::cli::RemoteCommands;
use crate::repository::Repository;
use exitfailure::ExitFailure;
use paris::{info, warn};
use std::env;

pub struct ActionRemote {}

impl ActionRemote {
    pub fn run(subcommand: Option<RemoteCommands>) -> Result<Repository, ExitFailure> {
        Ok(match subcommand {
            None => {
                let repos = Repository::new(env::current_dir()?)?;
                match repos.get_remote_url() {
                    Ok(url) => {
                        info!("{}", url);
                    }
                    Err(_) => {
                        warn!("Remote url is not set");
                    }
                }
                repos
            }
            Some(remote) => {
                match remote {
                    RemoteCommands::Set { url } => {
                        let mut repos = Repository::new(env::current_dir()?)?;
                        repos.set_remote_url(url)?;
                        match repos.get_remote_url() {
                            Ok(url) => {
                                info!("Updated remote url to '{}'", url);}
                            Err(_) => {
                                return Err(ExitFailure::from(failure::err_msg("Invalid remote url")));
                            }
                        }
                        repos
                    }
                }
            }
        })
    }
}