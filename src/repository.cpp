#include "repository.hpp"

#include <fstream>
#include <iostream>

#include "http.hpp"
#include "url.hpp"
#include "mime-db.hpp"

#include <csignal>
#include <cstdlib>

static bool interrupt = false;

#if _WIN32
#include <windows.h>

BOOL WINAPI consoleHandler(DWORD signal)
{
	std::cout << "Waiting for task completion..." << std::endl;
	interrupt = true;
	return TRUE;
}
#else
#include <termios.h>
#include <unistd.h>


void sigint_handler(int s) {
	std::cout << "Waiting for task completion..." << std::endl;
	interrupt = true;
}
#endif

namespace fileshare
{
	RepositoryConfig::RepositoryConfig(const std::filesystem::path& repos_root)
		: config_dir_path(absolute(repos_root / ".fileshare"))
	{
#if _WIN32
		if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
			throw std::runtime_error("ERROR: Could not set control handler");
#else
        struct sigaction sigIntHandler {};
        sigIntHandler.sa_handler = sigint_handler;
        sigemptyset(&sigIntHandler.sa_mask);
        sigIntHandler.sa_flags = 0;
        sigaction(SIGINT, &sigIntHandler, nullptr);
#endif


		curl_global_init(CURL_GLOBAL_DEFAULT);
		if (!exists(config_dir_path))
			create_directories(config_dir_path);

		const auto config_path = config_dir_path / "config.fileshare";
		const auto config_tmp_path = config_dir_path / "tmp.fileshare";

		if (exists(config_tmp_path))
			remove(config_tmp_path);

		std::ifstream cfg(config_path);

		if (!cfg.is_open())
		{
			std::cout << "Created new fileshare configuration file in " << absolute(config_dir_path).parent_path() <<
				std::endl;
			return;
		}

		nlohmann::json data = nlohmann::json::parse(cfg);
		if (data.contains("saved_state"))
			saved_state = Directory::from_json(data["saved_state"], nullptr);

		if (data.contains("remote_domain"))
			remote_domain = data["remote_domain"];
		if (data.contains("remote_repository"))
			remote_repository = Url::decode_string(data["remote_repository"]);
		if (data.contains("remote_directory"))
			remote_directory = Url::decode_string(data["remote_directory"]);
		if (data.contains("auth_token"))
			auth_token = data["auth_token"];
		if (data.contains("auth_token_exp"))
			auth_token_exp = data["auth_token_exp"];
		curl_global_cleanup();
	}

	RepositoryConfig::~RepositoryConfig()
	{
		nlohmann::json json;
		json["remote_domain"] = remote_domain;
		json["remote_repository"] = Url::encode_string(remote_repository);
		json["remote_directory"] = Url::encode_string(remote_directory);
		json["auth_token"] = auth_token;
		json["auth_token_exp"] = auth_token_exp;
		if (saved_state)
			json["saved_state"] = saved_state->serialize();

		const auto config_tmp_path = config_dir_path / "tmp.fileshare";

		if (exists(config_tmp_path))
			remove(config_tmp_path);

		std::ofstream output(config_tmp_path, std::ios_base::out);

		output << json;
		output.close();
		if (exists(config_dir_path / "config.fileshare"))
			remove(config_dir_path / "config.fileshare");
		// Overwrite old config only if everything was successfull
		try
		{
			std::filesystem::rename(config_tmp_path, config_dir_path / "config.fileshare");
		}
		catch (const std::exception& e)
		{
			std::cerr << "Failed to save repository config : " << e.what() << std::endl;
		}
	}

	std::optional<std::filesystem::path> RepositoryConfig::search_repos_root(const std::filesystem::path& path)
	{
		auto abs_path = absolute(path);

		for (const auto& file : std::filesystem::directory_iterator(abs_path))
			if (file.is_directory() && file.path().filename() == ".fileshare")
				return abs_path;

		if (abs_path.has_parent_path() && abs_path.parent_path() != abs_path)
			search_repos_root(abs_path.parent_path());

		return {};
	}

	std::filesystem::path RepositoryConfig::search_repos_root_or_error(
		const std::filesystem::path& path)
	{
		if (const auto& file = search_repos_root(path))
			return *file;

		throw std::runtime_error("Not a fileshare repository");
	}

	std::string RepositoryConfig::get_full_url() const
	{
		if (remote_repository.empty())
			throw std::runtime_error("The remote repos have not been set correctly");
		return remote_domain + "/repos?repos=" + Url::encode_string(remote_repository) + (remote_directory.empty()
			? ":"
			: "&directory=" + Url::encode_string(remote_directory));
	}

	void RepositoryConfig::set_full_url(const std::wstring& new_url)
	{
		const Url url(new_url);
		remote_domain = url.get_domain();
		remote_repository = url.get_option(L"repos") ? *url.get_option(L"repos") : L"";
		remote_directory = url.get_option(L"directory") ? *url.get_option(L"directory") : L"";
		saved_state = {};
	}

	Directory RepositoryConfig::fetch_repos_status() const
	{
		require_sync();

		try
		{
			Http http(auth_token);
			return Directory::from_json(http.fetch_json_data(
				                            remote_domain + "/repos/tree?repos=" + Url::encode_string(remote_repository)
				                            + (
					                            remote_directory.empty()
						                            ? ""
						                            : "&directory=" + Url::encode_string(remote_directory))), nullptr);
		}
		catch (const std::exception& e)
		{
			throw std::runtime_error(std::string("Failed to get repository status. Parse error : ") + e.what());
		}
	}

	void RepositoryConfig::download_replace_file(const File& file)
	{
		const auto& path = file.get_path();
		require_sync();

		// Move old file TODO add try catch
		std::optional<std::filesystem::path> moved_path;
		if (exists(path))
		{
			moved_path = path.parent_path() / (path.filename().generic_wstring() + L".fileshare_outdated");
			std::filesystem::rename(path, *moved_path);
		}
		else if (!path.parent_path().empty() && !exists(path.parent_path()))
			create_directories(path.parent_path());

		try
		{
			Http http(auth_token);
			std::ofstream downloaded_file(path, std::ios_base::out | std::ios_base::binary);

			http.fetch_file(
				remote_domain + "/repos/file?path=" + Url::encode_string(path.generic_wstring()) + "&repos=" +
				Url::encode_string(remote_repository),
				downloaded_file);

			downloaded_file.close();
			last_write_time(path, file.get_last_write_time().to_filesystem_time());
			update_saved_state(file);
			if (moved_path)
				std::filesystem::remove(*moved_path);
		}
		catch (const std::exception&)
		{
			// Revert or apply change
			if (exists(path))
				std::filesystem::remove(path);
			if (moved_path)
				std::filesystem::rename(*moved_path, path);
			throw;
		}
	}

	void RepositoryConfig::require_connection()
	{
		if (remote_domain.empty() || remote_repository.empty())
			throw std::runtime_error(
				"The remote url is not configured. Please update it with 'fileshare remote set <remote>'");

		if (auth_token.empty())
		{
			std::cout << "You are not logged in. Please connect to your account first." << std::endl;

			int try_cnt = 1;
			do
			{
#if _WIN32
				HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
				DWORD mode = 0;
				GetConsoleMode(hStdin, &mode);
				SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT);
#else
				termios oldt;
				tcgetattr(STDIN_FILENO, &oldt);
				termios newt = oldt;
				newt.c_lflag |= ECHO;
				tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
				std::cout << "email/username : ";
				std::string username;
				getline(std::cin, username);

#if _WIN32
				SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
#else
				newt.c_lflag &= ~ECHO;
				tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
				std::cout << "password : ";
				std::string password;
				getline(std::cin, password);
				std::cout << std::endl;

#if _WIN32
				SetConsoleMode(hStdin, mode);
#else
				tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif

				nlohmann::json json;
				json["username"] = username;
				json["password"] = password;

				Http http;
				http.set_payload(json.dump());

				try
				{
					nlohmann::json token_json = http.fetch_json_data(remote_domain + "/auth/gen-token");
					if (!token_json.contains("token"))
						throw std::runtime_error("Missing token in response");
					auth_token = token_json["token"];
					if (!token_json.contains("expiration_date"))
						throw std::runtime_error("Missing token expiration date in response");
					auth_token_exp = token_json["expiration_date"];

					if (auth_token.empty())
						throw std::runtime_error("Failed to retrieve credentials.");

					std::cout << "Successfully logged in !" << std::endl;

					break;
				}
				catch (const Http::HttpError& error)
				{
					if (error.code() == 404)
						throw std::runtime_error("Cannot connect to server : 404 Not Found !");
					if (error.code() == 401)
					{
						std::cerr << "Invalid credentials. Please try again or create a new account." << std::endl;
						continue;
					}
					throw std::runtime_error("Cannot connect to server : " + std::to_string(error.code()));
				}
			}
			while (try_cnt++ < 3);
		}
	}

	uint64_t RepositoryConfig::get_server_time() const
	{
		Http http;
		return http.fetch_json_data(remote_domain + "/time-epoch")["time_since_epoch"];
	}

	bool RepositoryConfig::is_sync() const
	{
		return std::abs(
				std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch()).count() -
				static_cast<int64_t>(get_server_time()))
			< 1000;
	}

	void RepositoryConfig::require_sync() const
	{
		if (!is_sync())
		{
			const auto server = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count();
			const auto client = static_cast<int64_t>(get_server_time());
			throw std::runtime_error(
				"The server is not synchronized with the client. The client has an offset of " + std::to_string(
					(server - client) / 1000) + " seconds !");
		}
	}

	bool RepositoryConfig::is_connected() const
	{
		return !auth_token.empty();
	}

	void RepositoryConfig::update_saved_state(const File& new_state, bool erase)
	{
		const auto& path = new_state.get_path();
		update_saved_state_dir(new_state, std::vector(path.begin(), path.end()), get_saved_state(), erase);
	}

	void RepositoryConfig::update_saved_state_dir(const File& new_state, const std::vector<std::filesystem::path>& path,
	                                              Directory& dir, bool erase)
	{
		if (path.size() == 1)
		{
			if (erase)
				dir.delete_file(new_state.get_name());
			else
				dir.replace_insert_file(new_state);
			return;
		}
		auto start = path.begin();
		if (const auto bellow = dir.find_directory(*start))
			update_saved_state_dir(new_state, std::vector(++start, path.end()), *bellow, erase);
		else
		{
			auto& new_dir = dir.add_directory(*start);
			update_saved_state_dir(new_state, std::vector(++start, path.end()), new_dir, erase);
		}
	}

	void RepositoryConfig::init_saved_state()
	{
		const Directory local = Directory::from_path(config_dir_path.parent_path(), nullptr);
		const Directory remote = fetch_repos_status();
		saved_state = Directory::init_saved_state(local, remote, nullptr);
	}

	void RepositoryConfig::receive_delete_file(const File& file)
	{
		std::filesystem::remove(file.get_path());
		update_saved_state(file, true);
	}

	void RepositoryConfig::upload_file(const File& file)
	{
		if (!exists(file.get_path()))
			throw std::runtime_error("The uploaded file does not exists");

		constexpr int64_t PACKET_SIZE = 20 * 1024 * 1024;

		int64_t total_size = file.get_file_size();
		int64_t uploaded_size = 0;

		std::ifstream file_read_stream(file.get_path(), std::ios_base::binary | std::ios_base::in);
		std::optional<std::string> content_token;

		while (uploaded_size < total_size && !is_interrupted())
		{
			int64_t packet_size = std::min(PACKET_SIZE, total_size - uploaded_size);
			const bool is_waiting_content_token = !content_token && packet_size != total_size;

			Http http(auth_token);

			if (content_token)
				http.add_header("content-token: " + *content_token);
			else
			{
				http.add_header("content-name: " + Url::encode_string(file.get_name().generic_wstring()));
				http.add_header("content-size: " + std::to_string(file.get_file_size()));
				http.add_header("content-mimetype: " + mime::find(file.get_path()));
				http.add_header("content-path: " + Url::encode_string(file.get_path().parent_path().generic_wstring()));
				http.add_header("content-description:");
				http.add_header(
					"content-timestamp: " + std::to_string(file.get_last_write_time().milliseconds_since_epoch()));
			}
			try
			{
				const auto json = http.upload_file(
					remote_domain + "/repos/upload/file?repos=" + Url::encode_string(remote_repository),
					file_read_stream, packet_size);

				if (is_waiting_content_token && http.get_last_response() == 201)
				{
					if (!json.contains("content-token"))
						throw std::runtime_error(
							"Missing content-token in response, file transfer failed ! Received : " + json.dump());
					content_token = json["content-token"];
					uploaded_size += packet_size;
				}
				else if (http.get_last_response() == 200)
				{
					uploaded_size += packet_size;
				}
				else if (http.get_last_response() == 202)
				{
					// Upload complete
					if (uploaded_size + packet_size != total_size)
						throw std::runtime_error(
							"Upload interrupted too early : " + std::to_string(uploaded_size) + " of " + std::to_string(
								total_size));

					if (!json.contains("status") || json["status"] != "Finished" || !json.contains("file_id"))
						throw std::runtime_error("Something went wrong during file upload process : " + json.dump());
					update_saved_state(file);
					return;
				}
				else
					throw std::runtime_error("Unhandled http response : " + std::to_string(http.get_last_response()));
			}
			catch (const std::exception&)
			{
				throw;
			}
		}
	}

	void RepositoryConfig::send_delete_file(const File& file)
	{
		Http http(auth_token);
		http.post_request(
			remote_domain + "/repos/delete-file?repos=" + Url::encode_string(remote_repository) + "&path=" +
			Url::encode_string(
				file.get_path().generic_wstring()));
		update_saved_state(file, true);
	}

	bool RepositoryConfig::is_interrupted()
	{
		return interrupt;
	}
}
