use std::env;
use failure::Error;
use crate::repository::Repository;

pub struct ActionLogout {

}

impl ActionLogout {
    pub async fn run() -> Result<Repository, Error> {
        let mut repos = Repository::new(env::current_dir()?)?;
        repos.logout().await?;
        Ok(repos)
    }
}