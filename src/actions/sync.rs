use crate::content::diff::Diff;
use crate::repository::Repository;
use std::env;
use failure::Error;

pub struct ActionSync {

}

impl ActionSync {
    pub async fn run() -> Result<Repository, Error> {
        let mut repos = Repository::new(env::current_dir()?)?;
        let diff = Diff::from_repository(&mut repos).await?;
        repos.apply_actions(diff.actions()).await?;
        Ok(repos)
    }
}