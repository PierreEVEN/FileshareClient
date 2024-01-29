#include <curlpp/cURLpp.hpp>

#include "application.hpp"
#include "option.hpp"
#include "repository.hpp"

void load_options(int argc, char** argv)
{
	fileshare::Command root("fileshare");

	// Repos status
	root.add_sub_command({
		"status", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
					std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);

				const auto root_dir = fileshare::Directory(cfg_file.parent_path(), nullptr);

				const auto diffs = root_dir.diff(cfg.fetch_repos_status());
				for (const auto& diff : diffs)
				{
					switch (diff.get_operation())
					{
					case fileshare::Diff::Operation::LocalIsNewer:
						std::cout << ">";
						break;
					case fileshare::Diff::Operation::RemoteDoesNotExists:
						std::cout << "+";
						break;
					case fileshare::Diff::Operation::LocalDoesNotExists:
						std::cout << "-";
						break;
					case fileshare::Diff::Operation::LocalIsOlder:
						std::cout << "<";
						break;
					}
					auto path = diff.get_path().string();
					std::replace(path.begin(), path.end(), '\\', '/');
					std::cout << " | " << path << std::endl;
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		"Get synchronization information about the current repository."
	});

	// Clone
	root.add_sub_command({
		"clone", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
					std::filesystem::current_path());
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{"repository url"},
		"Clone a distant repository."
	});

	// Init
	root.add_sub_command({
		"init", [&](auto)
		{
			try
			{
				if (fileshare::RepositoryConfig::search_config_file(std::filesystem::current_path()))
					throw std::runtime_error("The current path is already a fileshare repository");
				fileshare::RepositoryConfig cfg;
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		"Configure this directory as a fileshare repository."
	});

	// Pull
	root.add_sub_command({
		"pull", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
					std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		"Retrieve modifications from the server."
	});

	// Push
	root.add_sub_command({
		"push", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
					std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		"Send modifications to the server."
	});

	// Sync
	root.add_sub_command({
		"sync", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
					std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		"Synchronise this repository with the server. (equivalent to 'fileshare pull push')"
	});

	// Get-Set remote
	{
		fileshare::Command remote("remote");
		remote.add_sub_command({
			"set", [&](auto url)
			{
				try
				{
					const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
						std::filesystem::current_path());

					fileshare::RepositoryConfig cfg(cfg_file);
					cfg.set_full_url(url[0]);
					std::cout << "Update remote url to '" << cfg.get_full_url() << "'" << std::endl;
				}
				catch (const std::exception& e)
				{
					std::cerr << e.what() << std::endl;
				}
			},
			{"remote"},
			"Set the url to the remote server."
		});
		remote.add_sub_command({
			"get", [&](auto)
			{
				try
				{
					const auto cfg_file = fileshare::RepositoryConfig::search_config_file_or_error(
						std::filesystem::current_path());
					fileshare::RepositoryConfig cfg(cfg_file);
					std::cout << cfg.get_full_url() << std::endl;
				}
				catch (const std::exception& e)
				{
					std::cerr << e.what() << std::endl;
				}
			},
			{},
			"Get the url to the remote server."
		});
		root.add_sub_command(remote);
	}

	fileshare::Option::parse(argc, argv, root);
}

int main(int argc, char** argv)
{
	try
	{
		curlpp::initialize();

		load_options(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Execution failed : " << e.what() << std::endl;
	}
}
