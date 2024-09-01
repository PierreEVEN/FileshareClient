use std::collections::HashMap;
use std::sync::{Arc, RwLock};
use crate::content::item::{Item, RemoteItem};

pub trait Filesystem {}

#[derive(Default, Debug)]
pub struct RemoteFilesystem {
    items: HashMap<u64, Arc<RwLock<RemoteItem>>>,
    children: HashMap<u64, Vec<Arc<RwLock<RemoteItem>>>>
}

impl Filesystem for RemoteFilesystem {}

impl RemoteFilesystem {
    pub fn add_item(&mut self, mut item: Arc<RwLock<RemoteItem>>) {
        let id = item.read().unwrap().id;
        self.items.insert(id, item);
    }

    pub fn find_item(&self, id: &u64) -> Option<Arc<RwLock<RemoteItem>>> {
        match self.items.get(id) {
            None => { None }
            Some(item) => { Some(item.clone()) }
        }
    }

    pub fn get_children(&self, id: &u64) -> Result<Vec<Arc<RwLock<dyn Item>>>, failure::Error> {
        let mut children: Vec<Arc<RwLock<dyn Item>>> = vec![];

        todo!();
        for child in self.children.get(id).ok_or(failure::err_msg("Cannot find parent item"))? {
            //children.push(child.clone() as Item);
        }
        return Ok(children);
        //Ok(self.children.get(id).ok_or(failure::err_msg("Cannot find parent item"))?.clone())
    }
}