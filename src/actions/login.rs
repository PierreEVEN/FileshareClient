use exitfailure::ExitFailure;

pub struct ActionLogin {

}

impl ActionLogin {
    pub fn run(username: String) -> Result<(), ExitFailure> {
        Ok(())
    }
}