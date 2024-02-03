#pragma once

#include <fstream>
#include <iostream>
#include <optional>

#include "directory.hpp"

namespace fileshare {

	class Repository
	{
	public:
		Repository(const nlohmann::json& json);

		[[nodiscard]] nlohmann::json serialize() const;

		[[nodiscard]] const std::filesystem::path& get_local_path() const { return local_path; }

	private:
		std::filesystem::path local_path;
		std::wstring remote_url;
		std::wstring user_token;
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