use exitfailure::ExitFailure;
use crate::cli::EditorCommands;

pub struct ActionEditor {}

impl ActionEditor {
    pub fn run(subcommand: Option<EditorCommands>) -> Result<(), ExitFailure> {
        Ok(())
    }
}