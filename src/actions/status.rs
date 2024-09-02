use crate::content::diff::Diff;
use crate::repository::Repository;
use exitfailure::ExitFailure;
use std::env;

pub struct ActionStatus {}

impl ActionStatus {
    pub async fn run() -> Result<Repository, ExitFailure> {
        let mut repos = Repository::new(env::current_dir()?)?;
        let diff = Diff::from_repository(&mut repos).await?;

        for action in diff.actions() {
            paris::warn!("{:?}", action);
        }

        Ok(repos)
    }
}