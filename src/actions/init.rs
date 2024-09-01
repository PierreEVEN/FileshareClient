use std::env;
use exitfailure::ExitFailure;
use paris::info;
use crate::repository::Repository;

pub struct ActionInit {}

impl ActionInit {
    pub fn run() -> Result<Repository, ExitFailure> {
        let repository = Repository::init_here(env::current_dir()?)?;
        info!("Successfully initialized a new fileshare repository.");
        Ok(repository)
    }
}