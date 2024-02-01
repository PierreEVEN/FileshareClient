#include "application.hpp"
#include "option.hpp"
#include "repository.hpp"
#include "http.hpp"

std::ostream& human_readable_time(std::ostream& os, int64_t millis)
{
	auto ms = std::chrono::milliseconds(millis);
	using namespace std;
	using namespace std::chrono;
	typedef duration<int, ratio<86400>> days;
	char fill = os.fill();
	os.fill('0');
	auto d = duration_cast<days>(ms);
	ms -= d;
	auto h = duration_cast<hours>(ms);
	ms -= h;
	auto m = duration_cast<minutes>(ms);
	ms -= m;
	auto s = duration_cast<seconds>(ms);
	os << setw(2) << d.count() << "d:"
		<< setw(2) << h.count() << "h:"
		<< setw(2) << m.count() << "m:"
		<< setw(2) << s.count() << 's';
	os.fill(fill);
	return os;
};

template <typename R>
R execute_with_auth(fileshare::RepositoryConfig& config, std::function<R()> lambda)
{
	fileshare::Directory repos_status(nullptr, "");
	try
	{
		return lambda();
	}
	catch (const fileshare::Http::AccessDeniedException&)
	{
		if (!config.is_connected())
		{
			config.require_connection();

			return lambda();
		}
		throw;
	}
}

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

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file.parent_path(), nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diff = fileshare::Directory::diff(local_hierarchy, saved_hierarchy, remote_hierarchy);

				for (const auto& [left, right] : diff.get_conflicts())
					std::cout << left.operation_str() << " [X] " << right.operation_str() << " : " << left.get_file().
						get_path() << std::endl;

				for (const auto& change : diff.get_changes())
					std::cout << change.operation_str() << " : " << change.get_file().get_path() << std::endl;
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
		"clone", [&](auto url)
		{
			try
			{
                std::cout << "todo : create a new dir and don't clone here (also check if dir exists)" << std::endl;
				if (fileshare::RepositoryConfig::search_config_file(std::filesystem::current_path()))
					throw std::runtime_error("The current path is already a fileshare repository");
				fileshare::RepositoryConfig cfg;
				cfg.set_full_url(url[0]);
				cfg.require_connection();
				std::cout << "Cloned new fileshare repository : '" << cfg.get_full_url() << "'" << std::endl;
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

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file.parent_path(), nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diffs = fileshare::Directory::diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
				for (const auto& change : diffs.get_changes())
				{
					switch (change.get_operation())
					{
					case fileshare::Diff::Operation::LocalRevert:
						std::cout << "The local file '" << change.get_file().get_path() <<
							"' is older than previous version. Would you like to keep this outdated version or replace it with the last version on the server ?"
							<< std::endl;
					// Update local state to dismiss if needed
						break;

					case fileshare::Diff::Operation::RemoteDelete:
						std::cout << "Removing '" << change.get_file().get_path() << "'" << std::endl;
                        cfg.receive_delete_file(change.get_file());
					// Remove local file
						break;

					case fileshare::Diff::Operation::RemoteRevert:
						std::cout << "The remote server contains the file '" << change.get_file().get_path() <<
							"' that is older than the last saved version. Would you like to retrieve it anyway or keep your local version ?"
							<< std::endl;
					// Update local state to dismiss if needed
						break;

					case fileshare::Diff::Operation::RemoteAdded:
					case fileshare::Diff::Operation::RemoteNewer:
						std::cout << "Updating '" << change.get_file().get_path() << "'" << std::endl;
						cfg.download_replace_file(change.get_file());
					// Download from remote
						break;
					default: break;
					}
				}
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

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file.parent_path(), nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diffs = fileshare::Directory::diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
				for (const auto& change : diffs.get_changes())
				{
					switch (change.get_operation())
					{
					case fileshare::Diff::Operation::LocalNewer:
					case fileshare::Diff::Operation::LocalAdded:
						std::cout << "Sending '" << change.get_file().get_path() << "'" << std::endl;
                            cfg.upload_file(change.get_file());
					// Upload file
						break;

					case fileshare::Diff::Operation::LocalDelete:
						std::cout << "Removing '" << change.get_file().get_path() << "'" << std::endl;
                            cfg.send_delete_file(change.get_file());
					// Delete on remote
						break;

					case fileshare::Diff::Operation::LocalRevert:
						std::cout << "The local file '" << change.get_file().get_path() <<
							"' is older than the last saved version."
							<< std::endl;
					// Update local state to dismiss if needed
						break;

					case fileshare::Diff::Operation::RemoteRevert:
						std::cout << "The remote server contains the file '" << change.get_file().get_path() <<
							"' that is older than the last saved version."
							<< std::endl;
					// Update local state to dismiss if needed
						break;

					default: break;
					}
				}
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
		load_options(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Execution failed : " << e.what() << std::endl;
	}
}
