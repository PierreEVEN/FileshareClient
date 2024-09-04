use crate::content::item::{Item, LocalItem, RemoteItem};
use crate::serialization_utils::vec_arc_rwlock_serde;
use failure::Error;
use serde_derive::{Deserialize, Serialize};
use std::collections::{HashMap, HashSet};
use std::env;
use std::path::PathBuf;
use std::sync::{Arc, RwLock};
use tokio::fs;

pub trait Filesystem {
    fn get_roots(&self) -> Result<Vec<Arc<RwLock<dyn Item>>>, Error>;
    fn find_from_path(&self, path: &PathBuf) -> Result<Option<Arc<RwLock<dyn Item>>>, Error>;
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

        if let Some(children_set) = self.children.get(id) {
            for child in children_set {
                children.push(self.items.get(child).ok_or(failure::err_msg("Cannot find child item"))?.clone());
            }
        }
        Ok(children)
    }

    fn find_from_path_internal(&self, path: &PathBuf, items: &HashSet<u64>) -> Result<Option<Arc<RwLock<dyn Item>>>, Error> {
        let mut path_iter = path.iter();
        let mut p = path_iter.next().ok_or(failure::err_msg("Empty path"))?;

        while p == "." {
            p = path_iter.next().ok_or(failure::err_msg("Invalid path"))?;
        }

        for item_id in items {
            match self.find_item(item_id) {
                None => { continue; }
                Some(item) => {
                    if item.read().unwrap().name.plain()?.as_str() == p {
                        let remaining_path = PathBuf::from(path_iter.as_path());
                        match path_iter.next() {
                            None => {
                                return Ok(Some(item.clone() as Arc<RwLock<dyn Item>>));
                            }
                            Some(_) => {
                                match self.children.get(item_id) {
                                    None => {}
                                    Some(children) => {
                                        return self.find_from_path_internal(&remaining_path, children);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Err(failure::err_msg(format!("File {} not found in remote filesystem", path.display())))
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

    fn find_from_path(&self, path: &PathBuf) -> Result<Option<Arc<RwLock<dyn Item>>>, Error> {
        self.find_from_path_internal(path, &self.roots)
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

    pub fn scan_dir(&mut self, path: &PathBuf, parent: Option<Arc<RwLock<dyn Item>>>) -> Result<(), Error> {
        let dir_data = std::fs::read_dir(path)?;
        for entry in dir_data {
            let entry = entry?;

            if entry.file_name() == ".fileshare" {
                continue;
            }

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
                    parent.write().unwrap().cast_mut::<LocalItem>().add_child(item)
                }
            }
        }
        Ok(())
    }

    pub fn update_item_from_filesystem(&mut self, item: &Arc<RwLock<LocalItem>>) -> Result<(), Error> {
        let item_copy = item.clone();
        match &item.read().unwrap().get_parent()? {
            None => { self.roots.push(item_copy) }
            Some(parent) => {
                parent.write().unwrap().cast_mut::<LocalItem>().add_child(item_copy);
            }
        };
        Ok(())
    }

    fn find_from_path_internal(&self, path: &PathBuf, items: &Vec<Arc<RwLock<LocalItem>>>) -> Result<Option<Arc<RwLock<dyn Item>>>, Error> {
        let mut path_iter = path.iter();
        let mut p = path_iter.next().ok_or(failure::err_msg("Empty path"))?;

        while p == "." {
            p = path_iter.next().ok_or(failure::err_msg("Invalid path"))?;
        }

        for item in items {
            if item.read().unwrap().name().plain()?.as_str() == p {
                let remaining_path = PathBuf::from(path_iter.as_path());
                match path_iter.next() {
                    None => {
                        return Ok(Some(item.clone() as Arc<RwLock<dyn Item>>));
                    }
                    Some(_) => {
                        return self.find_from_path_internal(&remaining_path, item.read().unwrap().children());
                    }
                }
            }
        }
        Ok(None)
    }

    pub async fn remove_item(&mut self, item: &LocalItem) -> Result<(), Error> {
        let item_path = env::current_dir()?.join(item.path_from_root()?);
        if item_path.exists() {
            if item_path.is_file() {
                fs::remove_file(item_path).await?;
            } else {
                fs::remove_dir_all(item_path).await?;
            }
        }

        match item.get_parent()? {
            None => {
                for (i, root) in self.roots.iter().enumerate() {
                    if root.read().unwrap().name().encoded() == item.name().encoded() {
                        self.roots.remove(i);
                        break;
                    }
                }
            }
            Some(parent) => {
                parent.write().unwrap().cast_mut::<LocalItem>().remove_child(&item.name())?;
            }
        }

        Ok(())
    }

    pub fn post_deserialize(&mut self) {
        for item in &self.roots {
            Self::fix_parents_for(item);
        }
    }

    fn fix_parents_for(parent_item: &Arc<RwLock<LocalItem>>) {
        for item_ref in parent_item.read().unwrap().children() {
            {
                let mut child = item_ref.write().unwrap();
                {
                    let parent: Arc<RwLock<dyn Item>> = parent_item.clone();
                    child.set_parent(&parent);
                }
            }
            Self::fix_parents_for(item_ref);
        }
    }
    
    pub fn add_to_root(&mut self, new_item: Arc<RwLock<LocalItem>>) {
        self.roots.push(new_item);
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

    fn find_from_path(&self, path: &PathBuf) -> Result<Option<Arc<RwLock<dyn Item>>>, Error> {
        self.find_from_path_internal(path, &self.roots)
    }
}