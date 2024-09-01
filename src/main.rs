mod repository;
mod cli;
mod actions;

use exitfailure::ExitFailure;
use reqwest::Url;
use serde_derive::{Deserialize, Serialize};

use crate::cli::{FileshareArgs, RootCommands};
use clap::{Parser};
use crate::actions::clone::ActionClone;
use crate::actions::editor::ActionEditor;
use crate::actions::init::ActionInit;
use crate::actions::login::ActionLogin;
use crate::actions::logout::ActionLogout;
use crate::actions::pull::ActionPull;
use crate::actions::push::ActionPush;
use crate::actions::remote::ActionRemote;
use crate::actions::status::ActionStatus;
use crate::actions::sync::ActionSync;

#[derive(Serialize, Deserialize, Debug)]
struct CompanyQuote {
    c: f64,
    h: f64,
    l: f64,
    o: f64,
    pc: f64,
    t: i128,
}

impl CompanyQuote {
    async fn get(symbol: &String, api_key: &String) -> Result<Self, ExitFailure> {
        let url = format!(
            "https://finnhub.io/api/v1/quote?symbol={}&token={}",
            symbol, api_key
        );

        let url = Url::parse(&*url)?;
        let res = reqwest::get(url).await?.json::<CompanyQuote>().await?;

        Ok(res)
    }
}

#[tokio::main]
async fn main() -> Result<(), ExitFailure> {
    let args = FileshareArgs::parse();

    println!("{:?}", args);
    match args.commands {
        RootCommands::Clone{repository_url } => {
            ActionClone::run(repository_url)?;
        }
        RootCommands::Init => {
            ActionInit::run()?;
        }
        RootCommands::Push => {
            ActionPush::run()?;
        }
        RootCommands::Pull => {
            ActionPull::run()?;
        }
        RootCommands::Sync => {
            ActionSync::run()?;
        }
        RootCommands::Status => {
            ActionStatus::run()?;
        }
        RootCommands::Logout => {
            ActionLogout::run()?;
        }
        RootCommands::Login{ name } => {
            ActionLogin::run(name)?;
        }
        RootCommands::Editor{editor } => {
            ActionEditor::run(editor)?;
        }
        RootCommands::Remote{remote } => {
            ActionRemote::run(remote)?;
        }
    }
    Ok(())
    /*

        let api_key = "YOUR API KEY".to_string();
        let args: Vec<String> = env::args().collect();
        let mut symbol: String = "AAPL".to_string();

        if args.len() < 2 {
            println!("Since you didn't specify a company symbol, it has defaulted to AAPL.");
        } else {
            symbol = args[1].clone();
        }

        let res = CompanyQuote::get(&symbol, &api_key).await?;
        println!("{}'s current stock price: {}", symbol, res.c);

        Ok(())
        */
}
