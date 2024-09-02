use crate::content::item::{Item, LocalItem, RemoteItem};
use crate::serialization_utils::vec_arc_rwlock_serde;
use failure::Error;
use std::collections::{HashMap, HashSet};
use std::path::PathBuf;
use std::sync::{Arc, RwLock};
use serde_derive::{Deserialize, Serialize};

pub trait Filesystem {
    fn get_roots(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error>;
}

#[derive(Default, Debug)]
pub struct RemoteFilesystem {
    items: HashMap<u64, Arc<RwLock<RemoteItem>>>,
    children: HashMap<u64, HashSet<u64>>,
    roots: HashSet<u64>,
}

impl RemoteFilesystem {
    pub fn add_item(&mut self, item: Arc<RwLock<RemoteItem>>) {
        let (id, parent) = match item.read() {
            Ok(item) => { (item.id, item.parent_item) }
            Err(_) => { panic!() }
        };
        match parent {
            None => {
                self.roots.insert(id);
            }
            Some(parent) => {
                let test = self.children.entry(parent).or_default();
                test.insert(id);
            }
        }
        self.items.insert(id, item);
    }

    pub fn find_item(&self, id: &u64) -> Option<Arc<RwLock<RemoteItem>>> {
        self.items.get(id).cloned()
    }

    pub fn get_children(&self, id: &u64) -> Result<Vec<Arc<RwLock<RemoteItem>>>, Error> {
        let mut children = vec![];
        for child in self.children.get(id).ok_or(failure::err_msg("Cannot find parent item"))? {
            children.push(self.items.get(child).ok_or(failure::err_msg("Cannot find child item"))?.clone());
        }
        Ok(children)
    }
}

impl Filesystem for RemoteFilesystem {
    fn get_roots(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error> {
        let mut roots = vec![];
        for child in &self.roots {
            let item: Arc<RwLock<dyn Item>> = self.items.get(child).ok_or(failure::err_msg("Cannot find child item"))?.clone();
            roots.push(item);
        }
        Ok(roots)
    }
}


#[derive(Default, Debug, Serialize, Deserialize)]
pub struct LocalFilesystem {
    #[serde(with = "vec_arc_rwlock_serde")]
    roots: Vec<Arc<RwLock<LocalItem>>>,
}

impl LocalFilesystem {
    pub fn from_fileshare_root(root_dir: &PathBuf) -> Result<Self, Error> {
        let mut filesystem = Self::default();
        filesystem.scan_dir(root_dir, None)?;
        Ok(filesystem)
    }

    pub fn scan_dir(&mut self, path: &PathBuf, parent: Option<Arc<RwLock<LocalItem>>>) -> Result<(), Error> {
        let dir_data = std::fs::read_dir(path)?;
        for entry in dir_data {
            let entry = entry?;
            let item = LocalItem::from_filesystem(&entry.path(), parent.clone())?;
            let is_regular_file = item.is_regular_file();
            let item = Arc::new(RwLock::new(item));
            if !is_regular_file {
                self.scan_dir(&entry.path(), Some(item.clone()))?;
            }
            match &parent {
                None => {
                    self.roots.push(item);
                }
                Some(parent) => {
                    parent.write().unwrap().add_child(item)
                }
            }
        }
        Ok(())
    }

    pub fn add_item(&mut self, item: Arc<RwLock<LocalItem>>) -> Result<(), Error> {
        let item_copy = item.clone();
        match &item.read().unwrap().get_parent()? {
            None => {self.roots.push(item_copy)}
            Some(parent) => {
                parent.write().unwrap().cast_mut::<LocalItem>().add_child(item_copy);
            }
        };
        Ok(())
    }


    pub fn find_from_path(&mut self, path: &PathBuf) -> Result<Option<Arc<RwLock<LocalItem>>>, Error> {
        todo!();
        Ok(None)
    }
}

impl Filesystem for LocalFilesystem {
    fn get_roots(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error> {
        let mut roots = vec![];
        for root in &self.roots {
            let item: Arc<RwLock<dyn Item>> = root.clone();
            roots.push(item);
        }
        Ok(roots)
    }
}