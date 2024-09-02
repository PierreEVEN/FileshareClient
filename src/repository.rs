use crate::client_string::ClientString;
use crate::content::filesystem::{LocalFilesystem, RemoteFilesystem};
use crate::content::item::{Item, LocalItem, RemoteItem};
use exitfailure::ExitFailure;
use paris::{error, success, warn};
use reqwest::{Response, Url};
use rpassword::read_password;
use serde_derive::{Deserialize, Serialize};
use std::io::{stdout, Write};
use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};
use std::{env, fs, io};

use failure::Error;
use futures_util::StreamExt;
use gethostname::gethostname;
use indicatif::{MultiProgress, ProgressBar, ProgressFinish, ProgressState, ProgressStyle};
use std::fs::File;
use indicatif::style::ProgressTracker;

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

    auth_token: Option<AuthToken>,

    #[serde(skip_deserializing, skip_serializing)]
    root_path: PathBuf,

    #[serde(skip_deserializing, skip_serializing)]
    local_content: Option<Arc<RwLock<LocalFilesystem>>>,
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
            repository.init_directories(path)?;
            Ok(repository)
        } else {
            let mut repository = Self::default();
            repository.init_directories(path)?;
            Ok(repository)
        }
    }

    pub fn init_here(path: PathBuf) -> Result<Self, Error> {
        let fileshare_dir = path.join(".fileshare").join("config.json");
        if fileshare_dir.exists() {
            Err(failure::err_msg("Already a fileshare repository"))
        } else {
            let mut repository = Self::default();
            repository.init_directories(path)?;
            Ok(repository)
        }
    }

    fn init_directories(&mut self, root_path: PathBuf) -> Result<(), Error> {
        self.root_path = root_path;
        Self::create_config_dir(self.root_path.as_path())?;
        Ok(())
    }

    fn create_config_dir(path: &Path) -> Result<(), Error> {
        let fileshare_cfg_path = path.join(".fileshare");
        if !fileshare_cfg_path.exists() {
            fs::create_dir(fileshare_cfg_path.clone())?;
        }
        let fileshare_tmp_download_path = fileshare_cfg_path.join("tmp");
        if !fileshare_tmp_download_path.exists() {
            fs::create_dir(fileshare_tmp_download_path)?;
        }
        Ok(())
    }

    pub fn set_remote_url(&mut self, new_remote_url: String) -> Result<(), Error> {
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

    pub fn repos_path(&self) -> Result<String, Error> {
        if self.remote_user.is_empty() || self.remote_repository.is_empty() {
            Err(failure::err_msg("Invalid remote repository path"))
        } else {
            Ok(format!("{}/{}/", self.remote_user, self.remote_repository))
        }
    }

    pub async fn authenticate(&mut self, username: Option<String>) -> Result<String, Error> {
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

    pub async fn post(&mut self, path: String) -> Result<reqwest::RequestBuilder, Error> {
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

    pub async fn parse_result(&mut self, response: Response) -> Result<Response, Error> {
        if response.status().as_u16() == 401 {
            self.authenticate(None).await?;
        }
        Ok(response)
    }

    pub async fn get(&mut self, path: String) -> Result<reqwest::RequestBuilder, Error> {
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

    pub async fn fetch_remote_content(&mut self) -> Result<Arc<RwLock<RemoteFilesystem>>, Error> {
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

    pub fn fetch_local_content(&mut self) -> Result<Arc<RwLock<LocalFilesystem>>, Error> {
        match &self.local_content {
            None => {}
            Some(local_content) => { return Ok(local_content.clone()); }
        }

        let db_path = self.root_path.join(".fileshare").join("database.json");
        self.local_content = Some(if db_path.exists() {
            Arc::new(RwLock::new(serde_json::from_str::<LocalFilesystem>(fs::read_to_string(db_path)?.as_str())?))
        } else {
            warn!("Failed to find stored database");
            Arc::new(RwLock::new(LocalFilesystem::default()))
        });
        match &self.local_content {
            None => { Err(failure::err_msg("Local content is not valid")) }
            Some(local_content) => { Ok(local_content.clone()) }
        }
    }

    pub fn scan_local_content(&mut self) -> Result<Arc<RwLock<LocalFilesystem>>, Error> {
        let filesystem = Arc::new(RwLock::new(LocalFilesystem::from_fileshare_root(&self.root_path)?));
        Ok(filesystem)
    }

    pub async fn download_item(&mut self, item: &RemoteItem) -> Result<(), Error> {
        let item = &item;
        if item.is_regular_file() {
            let downloaded_path = self.root_path.join(".fileshare").join("tmp").join(format!("download_{}", item.id).as_str());
            let mut data_file = File::create(downloaded_path.clone())?;
            match self.download_file(item, &mut data_file).await {
                Ok(_) => {
                    let final_path = self.root_path.join(item.path_from_root()?);
                    fs::rename(downloaded_path, final_path)?;
                    let local_filesystem = &mut *self.fetch_local_content()?.write().unwrap();
                    match item.get_parent()? {
                        None => {
                            local_filesystem.add_item(Arc::new(RwLock::new(LocalItem::from_filesystem(&final_path, None)?)));
                        }
                        Some(parent) => {
                            local_filesystem.add_item(Arc::new(RwLock::new(
                                LocalItem::from_filesystem(
                                    &final_path,
                                    local_filesystem.find_from_path(&parent.read().unwrap().path_from_root()?)?)?)));
                        }
                    }
                    Ok(())
                }
                Err(err) => {
                    if downloaded_path.exists() {
                        fs::remove_file(downloaded_path)?;
                    }
                    Err(err)
                }
            }
        } else {
            //error!("Todo : handle directory download");
            Ok(())
        }
    }

    async fn download_file(&mut self, item: &RemoteItem, target_container: &mut File) -> Result<(), Error> {
        let mut stream = self.get(format!("{}file/{}", self.repos_path()?, item.id)).await?
            .send().await?
            .error_for_status()?
            .bytes_stream();

        let m = MultiProgress::new();
        let pb = m.add(ProgressBar::new(item.size.unwrap()));
        pb.set_style(ProgressStyle::with_template("{elapsed} [{wide_bar:.yellow/red}] ({bytes} / {total_bytes}) {eta}")?.progress_chars("#>-"));
        pb.set_message(item.name.plain()?);
        let title_pb = m.add(ProgressBar::new(item.size.unwrap()));
        title_pb.set_message(item.name.plain()?);
        title_pb.set_style(ProgressStyle::with_template("Downloading {msg} ({bytes_per_sec}) {spinner:.green} ")?);

        while let Some(chunk_result) = stream.next().await {
            let chunk_result = chunk_result?;
            pb.inc(chunk_result.len() as u64);
            title_pb.inc(chunk_result.len() as u64);
            target_container.write_all(chunk_result.as_ref())?;
        }
        target_container.flush()?;
        title_pb.finish_and_clear();
        pb.set_style(ProgressStyle::with_template(" ✅  {msg} ({total_bytes}) [{wide_bar:.green/red}] {elapsed}")?.progress_chars("->-"));
        pb.finish();
        Ok(())
    }

    pub fn remove_local_item(&mut self, item: &Arc<RwLock<dyn Item>>) -> Result<(), Error> {
        todo!()
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