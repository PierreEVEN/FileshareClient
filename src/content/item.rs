use crate::client_string::ClientString;
use crate::content::filesystem::RemoteFilesystem;
use failure::Error;
use serde_derive::{Deserialize, Serialize};
use std::sync::{Arc, RwLock, Weak};

pub trait Item {
    fn is_regular_file(&self) -> bool;
    fn name(&self) -> ClientString;
    fn size(&self) -> u64;
    fn timestamp(&self) -> u64;
    fn mime_type(&self) -> ClientString;
    fn get_parent(&self) -> Result<Option<Arc<RwLock<dyn Item>>>, Error>;
    fn get_children(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error>;
}


#[derive(Serialize, Deserialize, Default, Debug, Clone)]
pub struct RemoteItem {
    pub id: u64,
    pub repos: u64,
    pub owner: u64,
    pub name: ClientString,
    pub is_regular_file: bool,
    pub description: Option<ClientString>,
    pub parent_item: Option<u64>,
    pub is_trash: Option<bool>,
    pub size: Option<u64>,
    pub mimetype: Option<ClientString>,
    pub timestamp: Option<u64>,
    pub absolute_path: Option<ClientString>,
    pub open_upload: Option<bool>,

    #[serde(skip_deserializing, skip_serializing)]
    filesystem: Option<Weak<RwLock<RemoteFilesystem>>>,
}

impl RemoteItem {
    pub fn set_filesystem(&mut self, filesystem: &Arc<RwLock<RemoteFilesystem>>) {
        self.filesystem = Some(Arc::downgrade(filesystem));
    }
}

impl Item for RemoteItem {
    fn is_regular_file(&self) -> bool {
        self.is_regular_file
    }

    fn name(&self) -> ClientString {
        self.name.clone()
    }

    fn size(&self) -> u64 {
        self.size.unwrap_or(0)
    }

    fn timestamp(&self) -> u64 {
        self.timestamp.unwrap_or(0)
    }

    fn mime_type(&self) -> ClientString {
        self.mimetype.clone().unwrap_or(ClientString::default())
    }

    fn get_parent(&self) -> Result<Option<Arc<RwLock<dyn Item>>>, Error> {
        match &self.parent_item {
            None => {
                Ok(None)
            }
            Some(parent_item) => {
                match &self.filesystem {
                    None => { Err(failure::err_msg("Filesystem have not been defined")) }
                    Some(filesystem) => {
                        Ok(Some(filesystem.upgrade().ok_or(failure::err_msg("Invalid filesystem"))?
                            .read().unwrap()
                            .find_item(parent_item).ok_or(failure::err_msg("Parent item not found"))?.clone()))
                    }
                }
            }
        }
    }

    fn get_children(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error> {
        match &self.filesystem {
            None => { Err(failure::err_msg("Filesystem have not been defined")) }
            Some(filesystem) => {
                Ok(filesystem.upgrade().ok_or(failure::err_msg("Invalid filesystem"))?
                    .read().unwrap()
                    .get_children(&self.id)?.clone())
            }
        }
    }
}