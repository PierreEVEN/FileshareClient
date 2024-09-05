use crate::repository::Repository;
use failure::Error;
use paris::success;

pub struct ActionLogin {}

impl ActionLogin {
    pub async fn run(username: Option<String>) -> Result<Repository, Error> {
        let mut repos = Repository::new()?;
        repos.authenticate(username).await?;
        success!("Successfully logged in !");
        Ok(repos)
    }
}