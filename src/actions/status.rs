use std::env;
use exitfailure::ExitFailure;
use crate::repository::Repository;

pub struct ActionStatus {}

impl ActionStatus {
    pub async fn run() -> Result<Repository, ExitFailure> {
        let mut repos = Repository::new(env::current_dir()?)?;
        repos.fetch_content().await?;
        Ok(repos)
    }
}