#include <iostream>
#include <fstream>

#include "fileshare/diff.hpp"
#include "app/option.hpp"
#include "fileshare/repository.hpp"
#include "fileshare/http.hpp"
#include "fileshare/url.hpp"
#include "fileshare/shell_utils.hpp"


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

static void require_connection(fileshare::RepositoryConfig cfg)
{
	if (!cfg.is_connected())
	{
		std::cout << "You are not logged in. Please connect to your account first." << std::endl;

		int try_cnt = 1;
		do
		{
			std::cout << "email/username : ";
			std::string username;
			getline(std::cin, username);

			std::cout << "password : ";
			fileshare::ShellUtils::set_password_mode(true);
			std::string password;
			getline(std::cin, password);
			fileshare::ShellUtils::set_password_mode(false);
			std::cout << std::endl;

			try
			{
				cfg.connect(fileshare::Url::utf8_to_wstring(username), fileshare::Url::utf8_to_wstring(password));
				std::cout << "Successfully logged in !" << std::endl;
				break;
			}
			catch (const std::runtime_error& error)
			{
				if (try_cnt++ < 3)
					std::cerr << error.what() << " Please try again" << std::endl;
				else
					throw;
			}
		}
		while (true);
	}
}

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
			require_connection(config);

			return lambda();
		}
		throw;
	}
}

void merge_conflicts(fileshare::DiffResult& diff, fileshare::RepositoryConfig& cfg)
{
	while (!diff.get_conflicts().empty())
	{
		if (fileshare::RepositoryConfig::is_interrupted())
			throw std::runtime_error("Stopped !");

		auto [local, remote] = diff.pop_conflict();

		std::string side;
		switch (local.get_operation())
		{
		case fileshare::Diff::Operation::LocalAdded:
			switch (remote.get_operation())
			{
			case fileshare::Diff::Operation::RemoteAdded:
				// File was added on both sides
				if (remote.get_file().get_last_write_time() == local.get_file().get_last_write_time())
				// If timestamp are the same, it should be the same file : AUTO-MERGE
					cfg.update_saved_state(remote.get_file());
				else
				{
					std::cerr << "The file " << local.get_file().get_path() <<
						" was added on both side. Please select the one you prefer" << std::endl;
					std::cin >> side;
					throw std::runtime_error("not handled yet");
				}
				break;
			case fileshare::Diff::Operation::RemoteDelete:
				// File was deleted on remote but added locally : AUTO-MERGE
				diff += local;
				break;
			case fileshare::Diff::Operation::RemoteNewer:
				// File was added on both side but the remote is newer : AUTO-MERGE
				diff += remote;
				break;
			default:
				throw std::runtime_error(
					std::string("Unhandled conflict case : ") + local.operation_str() + " x " + remote.operation_str() +
					" : " + local.get_file().get_path().generic_string());
			}
			break;

		case fileshare::Diff::Operation::LocalDelete:
			switch (remote.get_operation())
			{
			case fileshare::Diff::Operation::RemoteAdded:
				// File was added locally but added on remote : AUTO-MERGE
				std::cerr << "The file " << local.get_file().get_path() <<
					" was added on both side. Please select the one you prefer" << std::endl;
				diff += remote;
				break;
			case fileshare::Diff::Operation::RemoteDelete:
				// File was deleted on both sides : AUTO-MERGE
				cfg.update_saved_state(remote.get_file(), true);
				break;
			case fileshare::Diff::Operation::RemoteNewer:
				// File was deleted locally but modified on remote : AUTO-MERGE
				diff += remote;
				break;
			case fileshare::Diff::Operation::RemoteRevert:
				// File was deleted locally but modified on remote : AUTO-MERGE
				diff += remote;
				break;
			default:
				throw std::runtime_error(
					std::string("Unhandled conflict case : ") + local.operation_str() + " x " + remote.operation_str() +
					" : " + local.get_file().get_path().generic_string());
			}
			break;

		case fileshare::Diff::Operation::LocalNewer:
			switch (remote.get_operation())
			{
			case fileshare::Diff::Operation::RemoteNewer:
				// File was modified on both sides
				std::cerr << "The file " << local.get_file().get_path() <<
					" was modified on both side. Please select the one you prefer" << std::endl;
				std::cin >> side;
				throw std::runtime_error("not handled yet");
				break;
			case fileshare::Diff::Operation::RemoteRevert:
				// File was updated locally but altered on remote
				std::cerr << "The remote moved back to an older version of " << local.get_file().get_path() <<
					" and you modified the file locally. Would you like to send your version ?" << std::endl;
				std::cin >> side;
				throw std::runtime_error("not handled yet");
				break;
			case fileshare::Diff::Operation::RemoteDelete:
				// File was updated locally but deleted on remote : AUTO-MERGE
				diff += local;
				break;
			case fileshare::Diff::Operation::RemoteAdded:
				// File was added on remote and the locale version is newer : AUTO-MERGE
				diff += local;
				break;
			default:
				throw std::runtime_error(
					std::string("Unhandled conflict case : ") + local.operation_str() + " x " + remote.operation_str() +
					" : " + local.get_file().get_path().generic_string());
			}
			break;

		case fileshare::Diff::Operation::LocalRevert:
			switch (remote.get_operation())
			{
			case fileshare::Diff::Operation::RemoteNewer:
				// File was updated on remote and altered locally too
				std::cerr << "You moved back to an older version of " << local.get_file().get_path() <<
					" and it was also modified on remote. Would you like to send your version ?" << std::endl;
				std::cin >> side;
				throw std::runtime_error("not handled yet");
				break;
			case fileshare::Diff::Operation::RemoteRevert:
				// File was reverted on both sides
				std::cerr << "The file " << local.get_file().get_path() <<
					" was reverted to an older version on both side. Please select the one you prefer" << std::endl;
				std::cin >> side;
				throw std::runtime_error("not handled yet");
				break;
			case fileshare::Diff::Operation::RemoteDelete:
				// File was updated locally but deleted on remote : AUTO-MERGE
				diff += local;
				break;
			default:
				throw std::runtime_error(
					std::string("Unhandled conflict case : ") + local.operation_str() + " x " + remote.operation_str() +
					" : " + local.get_file().get_path().generic_string());
			}
			break;
		default:
			throw std::runtime_error(
				std::string("Unhandled conflict case : ") + local.operation_str() + " x " + remote.operation_str() +
				" : " + local.get_file().get_path().generic_string());
		}
	}
}

std::filesystem::path tmp_file(const std::string& name)
{
	int id = 0;
	std::filesystem::path path;
	do
	{
		path = std::filesystem::temp_directory_path() / (std::to_string(id++) + name);
	}
	while (exists(path));
	return path;
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
					require_connection(cfg);

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

				if (diff.get_conflicts().size() > 50)
				{
					const auto tmp_path = tmp_file("Conflicts-detected.txt");
					std::wofstream tmp_stream(tmp_path);
					for (const auto& [left, right] : diff.get_conflicts())
						tmp_stream << left.operation_str() << " [X] " << right.operation_str() << " : " << left.
							get_file().
							get_path().generic_wstring() << std::endl;
					tmp_stream.close();

					const auto cmd = fileshare::Url::wstring_to_utf8(cfg.get_editor()) + " \"" + tmp_path.
						generic_string() + "\"";

					try
					{
						system(cmd.c_str());
					}
					catch (const std::exception& e)
					{
						throw std::runtime_error(
							std::string(
								"Failed to open editor, you can set a different editor with `fileshare set-editor <editor>` : ")
							+ e.what());
					}
				}
				else
				{
					for (const auto& [left, right] : diff.get_conflicts())
						std::wcout << left.operation_str() << " [X] " << right.operation_str() << " : " << left.
							get_file().
							get_path().generic_wstring() << std::endl;
				}

				if (diff.get_changes().size() > 50)
				{
					const auto tmp_path = tmp_file("Modifications-detected.txt");
					std::wofstream tmp_stream(tmp_path);
					for (const auto& change : diff.get_changes())
						tmp_stream << change.operation_str() << " : " << change.get_file().get_path().generic_wstring()
							<<
							std::endl;
					tmp_stream.close();

					const auto cmd = fileshare::Url::wstring_to_utf8(cfg.get_editor()) + " \"" + tmp_path.
						generic_string() + "\"";

					try
					{
						system(cmd.c_str());
					}
					catch (const std::exception& e)
					{
						throw std::runtime_error(
							std::string(
								"Failed to open editor, you can set a different editor with `fileshare set-editor <editor>` : ")
							+ e.what());
					}
				}
				else
				{
					for (const auto& change : diff.get_changes())
						std::wcout << change.operation_str() << " : " << change.get_file().get_path().generic_wstring()
							<<
							std::endl;
				}

				if (diff.get_changes().empty() && diff.get_conflicts().empty())
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
				if (const auto repos = parsed.get_option(L"repos"))
				{
					const std::wstring& repos_name = *repos;
					if (exists(std::filesystem::current_path() / repos_name))
						throw std::runtime_error(
							"A directory named " + fileshare::Url::wstring_to_utf8(repos_name) + " already exists");

					create_directories(std::filesystem::current_path() / repos_name);
					current_path(std::filesystem::current_path() / repos_name);

					fileshare::RepositoryConfig cfg(std::filesystem::current_path());
					cfg.set_full_url(url[0]);
					require_connection(cfg);
					std::cout << "Cloned a new fileshare repository : '" << cfg.get_full_url() << "'" << std::endl;

					if (!cfg.is_connected())
						require_connection(cfg);

					// Get local, saved, and remote tree
					const auto local_hierarchy = fileshare::Directory::from_path(".", nullptr);
					const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
					{
						return cfg.get_saved_state();
					});
					const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
						cfg, [&] { return cfg.fetch_repos_status(); });

					// Compare diff to saved state
					const auto diffs = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);

					fileshare::ProgressBar progress_bar(L"Cloning new local repository from " + cfg.get_repository(),
					                                    diffs.get_changes().size());
					size_t i = 0;

					for (const auto& change : diffs.get_changes())
					{
						if (fileshare::RepositoryConfig::is_interrupted())
							throw std::runtime_error("Stopped !");

						progress_bar.progress(
							++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().
							get_path().generic_wstring());

						switch (change.get_operation())
						{
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
				if (const auto repos = parsed.get_option(L"repos"))
				{
					if (exists(std::filesystem::current_path() / *repos))
						remove(std::filesystem::current_path() / *repos);
				}
			}
		},
		{L"repository url"},
		L"Clone a distant repository."
	});

	// set-editor
	root.add_sub_command({
		L"set-editor", [&](auto editor)
		{
			try
			{
				const auto cfg_file = fileshare::RepositoryConfig::search_repos_root_or_error(
					std::filesystem::current_path());
				fileshare::RepositoryConfig cfg;

				cfg.set_editor(editor[0]);
			}
			catch (const std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		},
		{L"text editor app"},
		L"Set the default application to edit files."
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
					require_connection(cfg);

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file, nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				auto diffs = fileshare::measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
				merge_conflicts(diffs, cfg);

				fileshare::ProgressBar progress_bar(L"Retrieving remote modifications to " + cfg.get_repository(),
				                                    diffs.get_changes().size());
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
						std::wcout << "The remote server contains the file '" << change.get_file().get_path().
							generic_wstring() <<
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
						progress_bar.progress(
							++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().
							get_path().generic_wstring());
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
					require_connection(cfg);

				// Get local, saved, and remote tree
				const auto local_hierarchy = fileshare::Directory::from_path(cfg_file, nullptr);
				const auto saved_hierarchy = execute_with_auth<fileshare::Directory>(cfg, [&]
				{
					return cfg.get_saved_state();
				});
				const auto remote_hierarchy = execute_with_auth<fileshare::Directory>(
					cfg, [&] { return cfg.fetch_repos_status(); });

				// Compare diff to saved state
				auto diffs = measure_diff(local_hierarchy, saved_hierarchy, remote_hierarchy);
				merge_conflicts(diffs, cfg);

				fileshare::ProgressBar progress_bar(L"Sending local modifications to " + cfg.get_repository(),
				                                    diffs.get_changes().size());
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
						std::wcout << "The remote server contains the file '" << change.get_file().get_path().
							generic_wstring() <<
							"' that is older than the last saved version."
							<< std::endl;
						updated = true;
						break;

					default: break;
					}
					if (updated)
						progress_bar.progress(
							++i, fileshare::Url::utf8_to_wstring(change.operation_str()) + L" " + change.get_file().
							get_path().generic_wstring());
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
