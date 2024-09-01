use std::env;
use exitfailure::ExitFailure;
use crate::repository::Repository;

pub struct ActionLogout {

}

impl ActionLogout {
    pub async fn run() -> Result<Repository, ExitFailure> {
        let mut repos = Repository::new(env::current_dir()?)?;
        repos.logout().await?;
        Ok(repos)
    }
}