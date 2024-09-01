use exitfailure::ExitFailure;
use crate::repository::Repository;

pub struct ActionClone {

}

impl ActionClone {
    pub fn run(_repository_url: String) -> Result<Repository, ExitFailure> {
        todo!("Not implemented yet")
    }
}