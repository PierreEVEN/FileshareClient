#pragma once

#include <optional>

#include "directory.hpp"

namespace fileshare
{
	class RepositoryConfig
	{
	public:
		RepositoryConfig(const std::filesystem::path& config_file_path = ".fileshare");
		~RepositoryConfig();

		static std::optional<std::filesystem::path> search_config_file(const std::filesystem::path& path);
		static std::filesystem::path search_config_file_or_error(const std::filesystem::path& path);

		void load_config();
		void save_config() const;

		void upload_item(const std::filesystem::path& file);

		[[nodiscard]] std::string get_full_url() const;
		[[nodiscard]] const std::string& get_domain() const { return remote_domain; }
		[[nodiscard]] const std::string& get_repository() const { return remote_repository; }
		[[nodiscard]] const std::string& get_directory() const { return remote_directory; }

		void set_full_url(const std::string& new_url);
		
		[[nodiscard]] Directory fetch_repos_status() const;
		void download_replace_file(const File& file);
        void receive_delete_file(const File& file);
        void upload_file(const File& file) const;
        void send_delete_file(const File& file);

		void require_connection();

		[[nodiscard]] uint64_t get_server_time() const;
		[[nodiscard]] std::filesystem::path get_path() const { return config_path; }

		[[nodiscard]] bool is_sync() const;
		void require_sync() const;

		[[nodiscard]] bool is_connected() const;
		[[nodiscard]] Directory& get_saved_state()
		{
			if (!saved_state)
				init_saved_state();
			return *saved_state;
		}

		void update_saved_state(const File& new_state);

	private:
		static void update_saved_state_dir(const File& new_state, const std::vector<std::filesystem::path>& path, Directory& dir);

		void init_saved_state();
		static Directory init_fill_saved_state_dir(const Directory& local, const Directory& remote);

		std::filesystem::path config_path;
		std::string remote_domain;
		std::string remote_repository;
		std::string remote_directory;
		std::string auth_token;
		uint64_t auth_token_exp;
		std::optional<Directory> saved_state;
	};
}
