#pragma once

#include <fstream>
#include <iostream>
#include <optional>

#include "directory.hpp"

namespace fileshare {

	class Repository
	{
	public:
		Repository(const nlohmann::json& json) :
			local_path(json["local_path"].get<std::filesystem::path>()),
			remote_url(json["remote_url"]),
			user_token(json["user_token"])
		{
		}

		[[nodiscard]] nlohmann::json serialize() const
		{
			nlohmann::json json;
			json["local_path"] = local_path;
			json["remote_url"] = remote_url;
			json["user_token"] = user_token;
			//json["state"] = state.serialize();
			return json;
		}

		[[nodiscard]] const std::filesystem::path& get_local_path() const { return local_path; }

	private:
		std::filesystem::path local_path;
		std::string remote_url;
		std::string user_token;
	};


	class AppConfig
	{
	public:
		AppConfig(std::optional<std::filesystem::path> in_config_path = {});
		~AppConfig()
		{
			save_config();
		}

		void save_config() const
		{
			nlohmann::json serialized;
			std::vector<nlohmann::json> repositories;
			for (const auto& repository : active_repositories)
				repositories.emplace_back(repository.serialize());
			serialized["repositories"] = repositories;

			std::ofstream output(config_path, std::ios_base::out);

			if (!output.is_open()) {
				std::cerr << "Cannot write fileshare configuration file" << std::endl;
				return;
			}
			output << serialized;
		}

		bool load_config()
		{
			if (!exists(config_path))
				return false;
			std::ifstream f(config_path);
			const auto json = nlohmann::json::parse(f);

			active_repositories.clear();
			for (const auto& repository : json["repositories"])
				active_repositories.emplace_back(Repository{ repository });

			return true;
		}

		[[nodiscard]] const std::vector<Repository>& get_active_repositories() const { return active_repositories; }

	private:
		std::vector<Repository> active_repositories;
		std::filesystem::path config_path;
	};
}