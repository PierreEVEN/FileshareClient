use std::env;
use failure::Error;
use paris::info;
use crate::repository::Repository;

pub struct ActionInit {}

impl ActionInit {
    pub fn run() -> Result<Repository, Error> {
        let repository = Repository::init_here(env::current_dir()?)?;
        info!("Successfully initialized a new fileshare repository.");
        Ok(repository)
    }
}