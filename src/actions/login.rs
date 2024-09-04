use std::env;
use failure::Error;
use paris::success;
use crate::repository::Repository;

pub struct ActionLogin {}

impl ActionLogin {
    pub async fn run(username: Option<String>) -> Result<Repository, Error> {
        let mut repos = Repository::new(env::current_dir()?)?;
        repos.authenticate(username).await?;
        success!("Successfully logged in !");
        Ok(repos)
    }
}