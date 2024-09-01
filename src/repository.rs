use std::fs;
use std::path::{Path, PathBuf};
use serde_derive::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Default)]
pub struct Repository {
    repository_url: String,

    #[serde(skip_serializing)]
    root_path: PathBuf,
}


impl Repository {
    pub fn new(path: &Path) -> Result<Self, exitfailure::ExitFailure> {
        let config_path = path.join(".fileshare").join("config.json");
        if config_path.exists() {
            let file_data = fs::read_to_string(path.join(".fileshare"))?;
            let mut repository: Self = serde_json::from_str(file_data.as_str())?;
            repository.root_path = path.to_path_buf();
            Ok(repository)
        } else {
            let mut repository = Self::default();
            repository.root_path = path.to_path_buf();
            Ok(repository)
        }
    }

    pub fn load(&mut self) {}

    fn create_config_dir(path: &Path) -> Result<(), exitfailure::ExitFailure> {
        let fileshare_cfg_path = path.join(".fileshare");
        if !fileshare_cfg_path.exists() {
            fs::create_dir(fileshare_cfg_path)?;
        }
        Ok(())
    }
}

impl Drop for Repository {
    fn drop(&mut self) {
        Self::create_config_dir(self.root_path.as_path()).ok();
        let serialized = serde_json::to_string(self).expect("Failed to write configuration file");
        fs::write(self.root_path.join(".fileshare_tmp"), serialized).expect("Unable to write file");
    }
}