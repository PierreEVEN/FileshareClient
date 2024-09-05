use crate::cli::RemoteCommands;
use crate::repository::Repository;
use failure::Error;
use paris::{info, warn};

pub struct ActionRemote {}

impl ActionRemote {
    pub fn run(subcommand: Option<RemoteCommands>) -> Result<Repository, Error> {
        Ok(match subcommand {
            None => {
                let repos = Repository::new()?;
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
                        let mut repos = Repository::new()?;
                        repos.set_remote_url(url)?;
                        match repos.get_remote_url() {
                            Ok(url) => {
                                info!("Updated remote url to '{}'", url);}
                            Err(_) => {
                                return Err(failure::err_msg("Invalid remote url"));
                            }
                        }
                        repos
                    }
                }
            }
        })
    }
}