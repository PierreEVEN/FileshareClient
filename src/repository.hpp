#pragma once

#include <optional>

#include "directory.hpp"

namespace fileshare
{
	class RepositoryConfig
	{
	public:
		RepositoryConfig(const std::filesystem::path& repos_root = "./");
		~RepositoryConfig();

		static std::optional<std::filesystem::path> search_repos_root(const std::filesystem::path& path);
		static std::filesystem::path search_repos_root_or_error(const std::filesystem::path& path);
		
		[[nodiscard]] std::string get_full_url() const;
		[[nodiscard]] const std::string& get_domain() const { return remote_domain; }
		[[nodiscard]] const std::wstring& get_repository() const { return remote_repository; }
		[[nodiscard]] const std::wstring& get_directory() const { return remote_directory; }

		void set_full_url(const std::wstring& new_url);
		
		[[nodiscard]] Directory fetch_repos_status() const;
		void download_replace_file(const File& file);
        void receive_delete_file(const File& file);
        void upload_file(const File& file);
        void send_delete_file(const File& file);

		void require_connection();

		[[nodiscard]] uint64_t get_server_time() const;
		[[nodiscard]] std::filesystem::path get_path() const { return config_dir_path; }

		[[nodiscard]] bool is_sync() const;
		void require_sync() const;

		[[nodiscard]] bool is_connected() const;
		[[nodiscard]] Directory& get_saved_state()
		{
			if (!saved_state)
				init_saved_state();
			return *saved_state;
		}

		void update_saved_state(const File& new_state, bool erase = false);

        static void force_stop();

        static bool is_interrupted();

	private:
		static void update_saved_state_dir(const File& new_state, const std::vector<std::filesystem::path>& path, Directory& dir, bool erase);

		void init_saved_state();

		std::filesystem::path config_dir_path;
		std::string remote_domain;
		std::wstring remote_repository;
		std::wstring remote_directory;
		std::string auth_token;
		uint64_t auth_token_exp;
		std::optional<Directory> saved_state;
	};
}
