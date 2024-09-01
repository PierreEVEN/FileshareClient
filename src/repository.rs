use crate::client_string::ClientString;
use crate::content::filesystem::{LocalFilesystem, RemoteFilesystem};
use crate::content::item::RemoteItem;
use exitfailure::ExitFailure;
use paris::{success, warn};
use reqwest::{Response, Url};
use rpassword::read_password;
use serde_derive::{Deserialize, Serialize};
use std::io::{stdout, Write};
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};
use std::{env, fs, io};
use gethostname::gethostname;

#[derive(Serialize, Deserialize, Debug, Clone)]
struct AuthToken {
    token: String,
    expiration_date: u64,
}

#[derive(Serialize, Deserialize, Default)]
pub struct Repository {
    #[serde(default = "empty_string")]
    remote_origin: String,

    #[serde(default = "empty_string")]
    remote_user: String,

    #[serde(default = "empty_string")]
    remote_repository: String,

    #[serde(default = "empty_string")]
    editor_command: String,

    #[serde(skip_deserializing, skip_serializing)]
    root_path: PathBuf,

    auth_token: Option<AuthToken>,
}

fn empty_string() -> String {
    String::new()
}


impl Repository {
    pub fn new(path: PathBuf) -> Result<Self, ExitFailure> {
        let config_path = path.join(".fileshare").join("config.json");
        if config_path.exists() {
            let file_data = fs::read_to_string(config_path)?;
            let mut repository: Self = serde_json::from_str(file_data.as_str())?;
            repository.root_path = path;
            Ok(repository)
        } else {
            let mut repository = Self::default();
            repository.root_path = path;
            Ok(repository)
        }
    }

    pub fn init_here(path: PathBuf) -> Result<Self, failure::Error> {
        let fileshare_dir = path.join(".fileshare").join("config.json");
        if fileshare_dir.exists() {
            Err(failure::err_msg("Already a fileshare repository"))
        } else {
            let mut repository = Self::default();
            repository.root_path = path;
            Ok(repository)
        }
    }

    fn create_config_dir(path: &Path) -> Result<(), ExitFailure> {
        let fileshare_cfg_path = path.join(".fileshare");
        if !fileshare_cfg_path.exists() {
            fs::create_dir(fileshare_cfg_path)?;
        }
        Ok(())
    }

    pub fn set_remote_url(&mut self, new_remote_url: String) -> Result<(), ExitFailure> {
        let url = Url::parse(new_remote_url.as_str())?;
        self.remote_origin = url.origin().ascii_serialization();
        let mut path = url.path_segments().ok_or_else(|| failure::err_msg("Cannot parse path"))?;
        self.remote_user = path.next().ok_or(failure::err_msg("Missing user in remote path"))?.to_string();
        self.remote_repository = path.next().ok_or(failure::err_msg("Missing repository in remote path"))?.to_string();
        Ok(())
    }

    pub fn get_remote_url(&self) -> Result<String, failure::Error> {
        if self.remote_origin.is_empty() ||
            self.remote_user.is_empty() ||
            self.remote_repository.is_empty() {
            return Err(failure::err_msg("Invalid remote url"));
        }

        Ok(format!("{}/{}/{}", self.remote_origin, self.remote_user, self.remote_repository))
    }
    pub fn set_editor_command(&mut self, new_editor_command: String) {
        self.editor_command = new_editor_command;
    }

    pub fn get_editor_command(&self) -> &str { self.editor_command.as_str() }

    pub fn get_origin(&self) -> Result<Url, failure::Error> {
        if self.remote_origin.is_empty() {
            return Err(failure::err_msg("Invalid origin"));
        }
        Ok(Url::parse(self.remote_origin.as_str())?)
    }

    pub async fn authenticate(&mut self, username: Option<String>) -> Result<String, failure::Error> {
        #[derive(Serialize, Debug, Default)]
        struct AuthenticationBody {
            username: ClientString,
            password: String,
            device: ClientString,
        }

        let mut body = AuthenticationBody::default();
        body.device = ClientString::from_client(format!("Fileshare client - {} ({}) : {}", gethostname().to_str().unwrap(), env::consts::OS, env::current_dir()?.to_str().unwrap()).as_str());
        body.username = match username {
            None => {
                print!("Username or email : ");
                stdout().flush()?;
                let mut buffer = String::new();
                io::stdin().read_line(&mut buffer)?;
                if let Some('\n') = buffer.chars().next_back() {
                    buffer.pop();
                }
                if let Some('\r') = buffer.chars().next_back() {
                    buffer.pop();
                }
                ClientString::from_client(buffer.as_str())
            }
            Some(username) => { ClientString::from_client(username.as_str()) }
        };

        for _ in 0..3 {
            print!("Password : ");
            stdout().flush()?;
            body.password = read_password()?;
            let client = reqwest::Client::new()
                .post(self.get_origin()?.join("api/create-authtoken")?)
                .json(&body)
                .send().await?;

            if client.status().as_u16() == 401 {
                warn!("Wrong credentials. Please try again...");
                continue;
            }

            self.auth_token = Some(client.error_for_status()?.json::<AuthToken>().await?);
            return match &self.auth_token {
                None => { Err(failure::err_msg("Invalid authentication token")) }
                Some(auth_token) => {
                    Ok(auth_token.token.clone())
                }
            };
        }
        Err(failure::err_msg("Authentication failed"))
    }

    pub async fn logout(&mut self) -> Result<(), ExitFailure> {
        let auth_token = match &self.auth_token {
            None => {
                warn!("Already disconnected");
                return Ok(());
            }
            Some(auth_token) => {
                auth_token.clone()
            }
        };

        self.post(format!("api/delete-authtoken/{}/", auth_token.token)).await?
            .send().await?.error_for_status()?;

        self.auth_token = None;
        success!("Successfully disconnected");

        Ok(())
    }

    pub async fn post(&mut self, path: String) -> Result<reqwest::RequestBuilder, failure::Error> {
        let auth_token = match &self.auth_token {
            None => {
                self.authenticate(None).await?
            }
            Some(auth_token) => { auth_token.token.clone() }
        };

        Ok(reqwest::Client::new()
            .post(self.get_origin()?.join(path.as_str())?)
            .header("content-authtoken", auth_token))
    }

    pub async fn parse_result(&mut self, response: Response) -> Result<Response, failure::Error> {
        if response.status().as_u16() == 401 {
            self.authenticate(None).await?;
        }
        Ok(response)
    }

    pub async fn get(&mut self, path: String) -> Result<reqwest::RequestBuilder, failure::Error> {
        let auth_token = match &self.auth_token {
            None => {
                self.authenticate(None).await?
            }
            Some(auth_token) => { auth_token.token.clone() }
        };

        Ok(reqwest::Client::new()
            .get(self.get_origin()?.join(path.as_str())?)
            .header("content-authtoken", auth_token))
    }

    pub async fn fetch_remote_content(&mut self) -> Result<Arc<RwLock<RemoteFilesystem>>, failure::Error> {
        let response = self.get(self.get_remote_url()? + "/content/").await?.send().await?;

        let mut content: Vec<RemoteItem> = self.parse_result(response).await?
            .error_for_status()?
            .json().await?;

        let filesystem = Arc::new(RwLock::new(RemoteFilesystem::default()));

        for item in &mut content {
            item.set_filesystem(&filesystem);
            filesystem.write().unwrap().add_item(Arc::new(RwLock::new(item.clone())));
        }

        Ok(filesystem)
    }

    pub fn fetch_local_content(&mut self) -> Result<Arc<RwLock<LocalFilesystem>>, failure::Error> {
        let db_path = self.root_path.join(".fileshare").join("database.json");
        if db_path.exists() {
            Ok(Arc::new(RwLock::new(serde_json::from_str::<LocalFilesystem>(fs::read_to_string(db_path)?.as_str())?)))
        }
        else {
            warn!("Failed to find stored database");
            Ok(Arc::new(RwLock::new(LocalFilesystem::default())))
        }
    }

    pub fn scan_local_content(&mut self) -> Result<Arc<RwLock<LocalFilesystem>>, failure::Error> {
        let filesystem = Arc::new(RwLock::new(LocalFilesystem::from_fileshare_root(&self.root_path)?));
        Ok(filesystem)
    }
}

impl Drop for Repository {
    fn drop(&mut self) {
        Self::create_config_dir(self.root_path.as_path()).ok();
        let serialized = serde_json::to_string(self).expect("Failed to serialize configuration data");
        fs::write(self.root_path.join(".fileshare").join("config.lock.json"), serialized).expect("Unable to write configuration lock file");
        fs::rename(self.root_path.join(".fileshare").join("config.lock.json"), self.root_path.join(".fileshare").join("config.json")).expect("Failed to write configuration file : cannot move");
    }
}