use std::env;
use exitfailure::ExitFailure;
use paris::success;
use crate::content::diff::{Action, Diff};
use crate::content::item::{Item, RemoteItem};
use crate::repository::Repository;

pub struct ActionPull {

}

impl ActionPull {
    pub async fn run() -> Result<Repository, ExitFailure> {

        let mut repos = Repository::new(env::current_dir()?)?;
        let diff = Diff::from_repository(&mut repos).await?;

        for action in diff.actions() {
            match action {
                Action::RemovedOnRemote(item) => {
                    repos.remove_local_item(item)?;
                    success!("Deleted {:?}", item);
                }
                Action::AddedOnRemote(item) => {
                    repos.download_item(item.read().unwrap().cast::<RemoteItem>()).await?;
                }
                Action::RemoveConflict(_) => {}
                Action::AddConflict(_, _) => {}
                _ => {}
            }
        }


        Ok(repos)
    }
}