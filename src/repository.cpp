#include "repository.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Infos.hpp>

#include "url.hpp"

#if _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace fileshare
{
	RepositoryConfig::RepositoryConfig(const std::filesystem::path& config_file_path)
		: config_path(absolute(config_file_path))
	{
		load_config();
	}


	void RepositoryConfig::load_config()
	{
		std::ifstream cfg(config_path);

		if (!cfg.is_open())
		{
			std::cout << "Created new fileshare configuration file in " << absolute(config_path).parent_path() <<
				std::endl;
			return;
		}


		nlohmann::json data = nlohmann::json::parse(cfg);
		if (data.contains("remote_domain"))
			remote_domain = data["remote_domain"];
		if (data.contains("remote_repository"))
			remote_repository = data["remote_repository"];
		if (data.contains("remote_directory"))
			remote_directory = data["remote_directory"];
		if (data.contains("auth_token"))
			auth_token = data["auth_token"];
	}

	RepositoryConfig::~RepositoryConfig()
	{
		save_config();
	}


	void RepositoryConfig::save_config() const
	{
		nlohmann::json json;
		json["remote_domain"] = remote_domain;
		json["remote_repository"] = remote_repository;
		json["remote_directory"] = remote_directory;
		json["auth_token"] = auth_token;

		std::ofstream output(config_path, std::ios_base::out);

		if (!output.is_open())
			throw std::runtime_error("Failed to write repository configuration");

		output << json;
	}

	std::optional<std::filesystem::path> RepositoryConfig::search_config_file(const std::filesystem::path& path)
	{
		const auto abs_path = absolute(path);

		for (const auto& file : std::filesystem::directory_iterator(abs_path))
			if (file.is_regular_file() && file.path().filename() == ".fileshare")
				return file;

		if (abs_path.has_parent_path() && abs_path.parent_path() != abs_path)
			search_config_file(abs_path.parent_path());

		return {};
	}

	std::filesystem::path RepositoryConfig::search_config_file_or_error(
		const std::filesystem::path& path)
	{
		if (const auto& file = search_config_file(path))
			return *file;

		throw std::runtime_error("Not a fileshare repository");
	}

	void RepositoryConfig::upload_item(const std::filesystem::path& file)
	{
	}

	std::string RepositoryConfig::get_full_url() const
	{
		if (remote_repository.empty())
			throw std::runtime_error("The remote repos have not been set correctly");
		return remote_domain + "/repos?repos=" + remote_repository + (remote_directory.empty()
			                                                              ? ":"
			                                                              : "&directory=" + remote_directory);
	}

	void RepositoryConfig::set_full_url(const std::string& new_url)
	{
		const Url url(new_url);
		remote_domain = url.get_domain();
		remote_repository = url.get_option("repos") ? *url.get_option("repos") : "";
		remote_directory = url.get_option("directory") ? *url.get_option("directory") : "";
		save_config();
	}

	Directory RepositoryConfig::fetch_repos_status()
	{
		require_connection();

		std::ostringstream repos_status;
		const curlpp::options::WriteStream ws(&repos_status);

		cURLpp::Easy req;
		req.setOpt(new curlpp::options::Url(
			remote_domain + "/repos/tree?repos=" + remote_repository + (remote_directory.empty()
				                                                            ? ""
				                                                            : "&directory=" + remote_directory)));
		req.setOpt(ws);
		req.perform();

		const auto code = curlpp::infos::ResponseCode::get(req);
		if (code == 404)
			throw std::runtime_error("Failed to get repository status. 404 : Not found");
		if (code != 200)
			throw std::runtime_error(std::string("Failed to get repository status : " + std::to_string(code)));

		nlohmann::json json;
		try
		{
			json = nlohmann::json::parse(repos_status.str());
		}
		catch (const std::exception& e)
		{
			throw std::runtime_error(std::string("Failed to get repository status. Parse error : ") + e.what());
		}
		return {json, nullptr};
	}

	void RepositoryConfig::require_connection()
	{
		if (remote_domain.empty() || remote_repository.empty())
			throw std::runtime_error(
				"The remote url is not configured. Please update it with 'fileshare remote set <remote>'");

		if (auth_token.empty()) {

			std::cout << "You are not logged in. Please connect to your account first." << std::endl;

			int code;
			int try_cnt = 1;
			do {

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

				const std::string body = json.dump();

				std::ostringstream token_stream;
				const curlpp::options::WriteStream ws(&token_stream);
				
				cURLpp::Easy req;
				std::list<std::string> header;
				header.push_back("Content-Type: application/json");
				req.setOpt(new curlpp::options::HttpHeader(header));
				req.setOpt(new curlpp::options::Url(remote_domain + "/auth/gen-token"));
				req.setOpt(new curlpp::options::PostFields(body));
				req.setOpt(new curlpp::options::PostFieldSize(static_cast<long>(body.length())));
				req.setOpt(ws);
				req.perform();
				code = curlpp::infos::ResponseCode::get(req);
				if (code == 400)
					throw std::runtime_error("Cannot connect to server : 404 Not Found !");
				if (code == 401)
				{
					std::cerr << "Invalid credentials. Please try again or create a new account." << std::endl;
					continue;
				}
				if (code != 200)
					throw std::runtime_error("Cannot connect to server : " + std::to_string(code));

				auth_token = token_stream.str();

				if (auth_token.empty())
					throw std::runtime_error("Failed to retrieve credentials.");
				save_config();
				std::cout << "Successfully logged in !" << std::endl;
				break;

			} while (code == 401 && try_cnt++ < 3);

			if (code != 200)
				throw std::runtime_error("Connection failed !");
		}
	}
}
