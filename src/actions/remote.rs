use crate::cli::RemoteCommands;
use exitfailure::ExitFailure;

pub struct ActionRemote {

}

impl ActionRemote {
    pub fn run(subcommand: Option<RemoteCommands>) -> Result<(), ExitFailure> {
        Ok(())
    }
}