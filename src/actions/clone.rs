use crate::content::diff::Diff;
use crate::repository::Repository;
use failure::Error;
use reqwest::Url;
use std::{env, fs};

pub struct ActionClone {}

impl ActionClone {
    pub async fn run(repository_url: String) -> Result<Repository, Error> {
        let url = Url::parse(repository_url.as_str())?;
        let mut path = url.path_segments().ok_or_else(|| failure::err_msg("Cannot parse path"))?;
        path.next().ok_or(failure::err_msg("Missing user in remote path"))?.to_string();
        let remote_repository = path.next().ok_or(failure::err_msg("Missing repository in remote path"))?.to_string();
        fs::create_dir(format!("./{}", remote_repository))?;
        env::set_current_dir(format!("./{}", remote_repository))?;
        let repository = match Self::try_clone_here(repository_url).await {
            Ok(repository) => { repository }
            Err(error) => {
                env::set_current_dir("..")?;
                fs::remove_dir_all(format!("./{}", remote_repository))?;
                return Err(error);
            }
        };

        env::set_current_dir("..")?;

        Ok(repository)
    }

    async fn try_clone_here(repository_url: String) -> Result<Repository, Error> {
        let mut repository = Repository::init_here(env::current_dir()?)?;
        repository.set_remote_url(repository_url)?;

        let diff = Diff::from_repository(&mut repository).await?;
        repository.apply_actions(diff.actions()).await?;
        Ok(repository)
    }
}