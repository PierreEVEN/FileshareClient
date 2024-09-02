use crate::content::diff::{Action, Diff};
use crate::repository::Repository;
use exitfailure::ExitFailure;
use std::env;

pub struct ActionPull {}

impl ActionPull {
    pub async fn run() -> Result<Repository, ExitFailure> {
        let mut repos = Repository::new(env::current_dir()?)?;
        let diff = Diff::from_repository(&mut repos).await?;

        for action in diff.actions() {
            match action {
                Action::ResyncLocal(scanned) => {
                    repos.update_local_item_state(&*scanned.read().unwrap())?;
                }
                Action::ConflictAddLocalNewer(scanned, remote) => {}
                Action::ErrorRemoteDowngraded(scanned, remote) => {}
                Action::LocalUpgraded(scanned, remote) => {}
                Action::ConflictBothDowngraded(scanned, local) => {}
                Action::ConflictBothUpgraded(scanned, local) => {}
                Action::ConflictLocalUpgradedRemoteDowngraded(scanned, local, remote) => {}
                Action::ConflictAddRemoteNewer(scanned, remote) => {}
                Action::RemoteUpgraded(scanned, remote) => {
                    repos.download_item(remote.clone()).await?;
                }
                Action::ErrorLocalDowngraded(scanned, remote) => {}
                Action::ConflictLocalDowngradedRemoteUpgraded(scanned, local, remote) => {}
                Action::RemoteRemoved(scanned) => {
                    repos.remove_local_item(scanned)?;
                }
                Action::LocalAdded(scanned) => {}
                Action::LocalRemoved(remote) => {}
                Action::RemoteAdded(remote) => {
                    repos.download_item(remote.clone()).await?;
                }
                Action::RemovedOnBothSides(local) => {}
            }
        }


        Ok(repos)
    }
}