
#include <iostream>

#include "diff.hpp"
#include "option.hpp"
#include "repository.hpp"
#include "http.hpp"
#include "url.hpp"
#include "shell_utils.hpp"

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
	fileshare::Directory repos_status(nullptr, L"");
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
	fileshare::Command root(L"fileshare");

	// Repos status
	root.add_sub_command({
		L"status", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
                        std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file, nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diff = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);

				for (const auto& [left, right] : diff.get_conflicts())
					std::wcout << left.operation_str() << " [X] " << right.operation_str() << " : " << left.get_file().
						get_path().generic_wstring() << std::endl;

				for (const auto& change : diff.get_changes())
					std::wcout << change.operation_str() << " : " << change.get_file().get_path().generic_wstring() << std::endl;
				if (diff.get_changes().empty())
					std::cout << "Up to date !" << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		L"Get synchronization information about the current repository."
	});

	// Clone
	root.add_sub_command({
		L"clone", [&](auto url)
		{
			try
			{
                fileshare::Url parsed(url[0]);
                if (const auto repos = parsed.get_option(L"repos")) {
                    const std::wstring &repos_name = *repos;
                    if (exists(std::filesystem::current_path() / repos_name))
                        throw std::runtime_error(
                                "A directory named " + fileshare::Url::wstring_to_utf8(repos_name) + " already exists");

                    create_directories(std::filesystem::current_path() / repos_name);
                    std::filesystem::current_path(std::filesystem::current_path() / repos_name);

                    fileshare::RepositoryConfig cfg(std::filesystem::current_path());
                    cfg.set_full_url(url[0]);
                    cfg.require_connection();
                    std::cout << "Cloned a new fileshare repository : '" << cfg.get_full_url() << "'" << std::endl;

                    if (!cfg.is_connected())
                        cfg.require_connection();

                    // Get local, saved, and remote tree
                    const auto local_hierarchy = fileshare::Directory::from_path(".", nullptr);
                    const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&] {
                        return cfg.get_saved_state();
                    });
                    const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
                            cfg, [&] { return cfg.fetch_repos_status(); });

                    // Compare diff to saved state
                    const auto diffs = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);

                    fileshare::ProgressBar progress_bar(L"Sending local modifications to " + cfg.get_repository(), diffs.get_changes().size());
                    size_t i = 0;

                    for (const auto &change: diffs.get_changes()) {
                        if (fileshare::RepositoryConfig::is_interrupted())
                            throw std::runtime_error("Stopped !");

                        progress_bar.progress(++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().get_path().generic_wstring());

                        switch (change.get_operation()) {
                            case fileshare::Diff::Operation::RemoteAdded:
                                cfg.download_replace_file(change.get_file());
                                break;
                            default:
                                throw std::runtime_error("This should never happen when cloning");
                        }
                    }
                }
                else
                    throw std::runtime_error("URL is not a valid fileshare repository");
			}
			catch (const std::exception& e)
			{
                std::cerr << e.what() << std::endl;
                fileshare::Url parsed(url[0]);
                if (const auto repos = parsed.get_option(L"repos")) {
                    if (exists(std::filesystem::current_path() / *repos))
                        remove(std::filesystem::current_path() / *repos);
                }
			}
		},
		{L"repository url"},
		L"Clone a distant repository."
	});

	// Init
	root.add_sub_command({
		L"init", [&](auto)
		{
			try
			{
				if (fileshare::RepositoryConfig::search_repos_root(std::filesystem::current_path()))
					throw std::runtime_error("The current path is already a fileshare repository");
				fileshare::RepositoryConfig cfg;
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		L"Configure this directory as a fileshare repository."
	});

	// Pull
	root.add_sub_command({
		L"pull", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
                        std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file, nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diffs = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
                fileshare::ProgressBar progress_bar(L"Sending local modifications to " + cfg.get_repository(), diffs.get_changes().size());
                size_t i = 0;

                for (const auto& change : diffs.get_changes())
				{
                    if (fileshare::RepositoryConfig::is_interrupted())
                        throw std::runtime_error("Stopped !");

					bool updated = false;
                    switch (change.get_operation())
					{
					case fileshare::Diff::Operation::LocalRevert:
						std::wcout << "The local file '" << change.get_file().get_path().generic_wstring() <<
							"' is older than previous version. Would you like to keep this outdated version or replace it with the last version on the server ?"
							<< std::endl;
						updated = true;
						break;

					case fileshare::Diff::Operation::RemoteDelete:
                        cfg.receive_delete_file(change.get_file());
						updated = true;
						break;

					case fileshare::Diff::Operation::RemoteRevert:
						std::wcout << "The remote server contains the file '" << change.get_file().get_path().generic_wstring() <<
							"' that is older than the last saved version. Would you like to retrieve it anyway or keep your local version ?"
							<< std::endl;
						updated = true;
						break;

					case fileshare::Diff::Operation::RemoteAdded:
					case fileshare::Diff::Operation::RemoteNewer:
						cfg.download_replace_file(change.get_file());
						updated = true;
						break;
					default: break;
					}
					if (updated)
						progress_bar.progress(++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().get_path().generic_wstring());
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		L"Retrieve modifications from the server."
	});

	// Push
	root.add_sub_command({
		L"push", [&](const std::vector<std::wstring>&)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
                        std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);

				if (!cfg.is_connected())
					cfg.require_connection();

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file, nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				const auto diffs = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
                fileshare::ProgressBar progress_bar(L"Sending local modifications to " + cfg.get_repository(), diffs.get_changes().size());
                size_t i = 0;
				for (const auto& change : diffs.get_changes())
				{
                    if (fileshare::RepositoryConfig::is_interrupted())
                        throw std::runtime_error("Stopped !");

					bool updated = false;
					switch (change.get_operation())
					{
					case fileshare::Diff::Operation::LocalNewer:
					case fileshare::Diff::Operation::LocalAdded:
                            cfg.upload_file(change.get_file());
							updated = true;
						break;

					case fileshare::Diff::Operation::LocalDelete:
                            cfg.send_delete_file(change.get_file());
							updated = true;
						break;

					case fileshare::Diff::Operation::LocalRevert:
						std::wcout << "The local file '" << change.get_file().get_path().generic_wstring() <<
							"' is older than the last saved version."
							<< std::endl;
						updated = true;
						break;

					case fileshare::Diff::Operation::RemoteRevert:
						std::wcout << "The remote server contains the file '" << change.get_file().get_path().generic_wstring() <<
							"' that is older than the last saved version."
							<< std::endl;
						updated = true;
						break;

					default: break;
					}
					if (updated)
						progress_bar.progress(++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().get_path().generic_wstring());
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		L"Send modifications to the server."
	});

	// Sync
	root.add_sub_command({
		L"sync", [&](auto)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
                        std::filesystem::current_path());

				fileshare::RepositoryConfig cfg(cfg_file);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{},
		L"Synchronise this repository with the server. (equivalent to 'fileshare pull push')"
	});

	// Get-Set remote
	{
		fileshare::Command remote(L"remote");
		remote.add_sub_command({
			L"set", [&](auto url)
			{
				try
				{
					const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
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
			{L"remote"},
			L"Set the url to the remote server."
		});
		remote.add_sub_command({
			L"get", [&](auto)
			{
				try
				{
					const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
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
			L"Get the url to the remote server."
		});
		root.add_sub_command(remote);
	}

	fileshare::Option::parse(argc, argv, root);
}

int main(int argc, char** argv)
{
	fileshare::ShellUtils::init();
	try
	{
		load_options(argc, argv);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Execution failed : " << e.what() << std::endl;
	}
}
