use crate::content::diff::Diff;
use crate::repository::Repository;
use exitfailure::ExitFailure;
use std::env;

pub struct ActionStatus {}

impl ActionStatus {
    pub async fn run() -> Result<Repository, ExitFailure> {
        let mut repos = Repository::new(env::current_dir()?)?;
        let remote = repos.fetch_remote_content().await?;
        let local = repos.fetch_local_content()?;
        let scanned = repos.scan_local_content()?;

        let diff = Diff::new(&*scanned.read().unwrap(), &*local.read().unwrap(), &*remote.read().unwrap())?;

        for action in diff.actions() {
            paris::warn!("{:?}", action);
        }

        Ok(repos)
    }
}