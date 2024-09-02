use std::collections::HashMap;
use std::env;
use std::sync::{Arc, RwLock};
use failure::Error;
use crate::content::filesystem::Filesystem;
use crate::content::item::{Item, LocalItem, RemoteItem};
use crate::repository::Repository;

#[derive(Debug, Clone, Default)]
pub struct Diff {
    actions: Vec<Action>,
}

fn sort_items_to_set(items: &Vec<Arc<RwLock<dyn Item>>>) -> HashMap<String, Arc<RwLock<dyn Item>>> {
    let mut hashmap = HashMap::new();
    for item in items {
        hashmap.insert(item.read().unwrap().name().encoded(), item.clone());
    }
    hashmap
}

#[derive(Debug, Clone)]
pub enum Action {
    RemovedLocally(Arc<RwLock<dyn Item>>),
    RemovedOnRemote(Arc<RwLock<dyn Item>>),
    AddedLocally(Arc<RwLock<dyn Item>>),
    AddedOnRemote(Arc<RwLock<dyn Item>>),
    RemoveConflict(Arc<RwLock<dyn Item>>),
    // Scanned - Remote
    AddConflict(Arc<RwLock<dyn Item>>, Arc<RwLock<dyn Item>>),
}

impl Diff {
    pub async fn from_repository(repository: &mut Repository) -> Result<Self, Error> {
        Diff::new(&*repository.scan_local_content()?.read().unwrap(),
                  &*repository.fetch_local_content()?.read().unwrap(),
                  &*repository.fetch_remote_content().await?.read().unwrap())
    }

    pub fn new(scanned: &dyn Filesystem, local: &dyn Filesystem, remote: &dyn Filesystem) -> Result<Self, Error> {
        let mut diff = Self::default();
        diff.compare(sort_items_to_set(&scanned.get_roots()?),
                     sort_items_to_set(&local.get_roots()?),
                     sort_items_to_set(&remote.get_roots()?))?;
        Ok(diff)
    }

    pub fn actions(&self) -> &Vec<Action> {
        &self.actions
    }

    fn compare(&mut self, scanned: HashMap<String, Arc<RwLock<dyn Item>>>, local: HashMap<String, Arc<RwLock<dyn Item>>>, remote: HashMap<String, Arc<RwLock<dyn Item>>>) -> Result<(), Error> {
        for (key, item) in &scanned {
            if !local.contains_key(key) {
                match remote.get(key) {
                    None => {
                        // Item added locally only
                        self.actions.push(Action::AddedLocally(item.clone()))
                    }
                    Some(remote_item) => {
                        // Item added on both sides
                        self.actions.push(Action::AddConflict(item.clone(), remote_item.clone()))
                    }
                }
            } else if !remote.contains_key(key) {
                // Removed on remote
                self.actions.push(Action::RemovedOnRemote(item.clone()))
            }
        }

        for (key, item) in &local {
            if !scanned.contains_key(key) {
                if !remote.contains_key(key) {
                    // Item removed on both sides
                    self.actions.push(Action::RemoveConflict(item.clone()))
                } else {
                    // Item removed locally only
                    self.actions.push(Action::RemovedLocally(item.clone()))
                }
            }
        }

        for (key, item) in &remote {
            if !local.contains_key(key) && !scanned.contains_key(key) {
                // Item added on remote
                self.actions.push(Action::AddedOnRemote(item.clone()))
            }
        }

        Ok(())
    }
}