#include "directory.hpp"

#include <fstream>

#include "url.hpp"

#if WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fileshare
{
	const char* Diff::operation_str() const
	{
		switch (operation)
		{
		case Operation::LocalDelete:
			return "-| ";
		case Operation::LocalRevert:
			return "<| ";
		case Operation::LocalNewer:
			return ">| ";
		case Operation::LocalAdded:
			return "+| ";
		case Operation::RemoteDelete:
			return " |-";
		case Operation::RemoteRevert:
			return " |>";
		case Operation::RemoteNewer:
			return " |<";
		case Operation::RemoteAdded:
			return " |+";
		}
		return "O";
	}

	void DiffResult::append(const Diff&& diff)
	{
		const auto existing_diff = visited_paths.find(diff.get_file().get_path());
		// conflict detected
		if (existing_diff != visited_paths.end())
		{
			const auto& other = existing_diff->second;
			bool is_a_conflict = true;
			// If file was removed on both sides
			if (
				(diff.get_operation() == Diff::Operation::LocalDelete || diff.get_operation() ==
					Diff::Operation::RemoteDelete) &&
				(other.get_operation() == Diff::Operation::LocalDelete || other.get_operation() ==
					Diff::Operation::RemoteDelete))
				is_a_conflict = false;

			if (is_a_conflict)
				file_conflicts.emplace_back(other, diff);
		}
		else {
			visited_paths.insert(std::pair(diff.get_file().get_path(), diff));
			file_changes.emplace_back(diff);
		}
	}

	Directory::Directory(const Directory* parent, std::filesystem::path in_name) :
		name(std::move(in_name))
	{
		is_root_dir = !parent;
		if (parent && !parent->is_root())
			path = parent->get_path() / name;
		else
			path = name;
	}

	Directory Directory::from_json(const nlohmann::json& json, const Directory* parent)
	{
		if (!json.contains("name"))
			throw std::runtime_error("Missing directory name in retrieved data");
		Directory dir(parent, Url::decode_string(json["name"]));

		if (json.contains("files"))
			for (const auto& entry : json["files"])
			{
				const auto file = File{entry, &dir};
				dir.files.emplace_back(file);
			}

		if (json.contains("directories"))
			for (const auto& entry : json["directories"])
			{
				const auto directory = from_json(entry, &dir);
				dir.directories.emplace_back(directory);
			}
		return dir;
	}

	Directory Directory::from_path(const std::filesystem::path& in_path, const Directory* parent)
	{
#if WIN32
		_setmode(_fileno(stdout), _O_U16TEXT);
#endif

		Directory dir(parent, in_path.filename());

		std::unordered_set<std::filesystem::path> excluded_files = {".fileshare"};
		
		if (exists(in_path / ".fileshareignore"))
		{
			std::wifstream exclusion_file(in_path / ".fileshareignore");

			std::wstring line;
			while (std::getline(exclusion_file, line))
				if (!line.empty()) {
					excluded_files.insert(line);
				}
		}

		for (const auto& entry : std::filesystem::directory_iterator(in_path))
		{
			if (excluded_files.contains(entry.path().filename()))
				continue;
			if (entry.is_regular_file())
			{
				if (entry.file_size() == 0)
					continue;
				const auto file = File{entry, &dir};
				dir.files.emplace_back(file);
			}
			else if (entry.is_directory())
			{
				const auto directory = from_path(entry.path(), &dir);
				dir.directories.emplace_back(directory);
			}
		}
		return dir;
	}

	nlohmann::json Directory::serialize() const
	{
		nlohmann::json json;
		std::vector<nlohmann::json> files_json;
		std::vector<nlohmann::json> directory_json;


		files_json.reserve(files.size());
		for (const auto& file : files)
			files_json.emplace_back(file.serialize());

		directory_json.reserve(directories.size());
		for (const auto& directory : directories)
			directory_json.emplace_back(directory.serialize());

		json["name"] = Url::encode_string(name.generic_wstring());
		json["files"] = files_json;
		json["directories"] = directory_json;

		return json;
	}

	DiffResult Directory::diff(const Directory& local, const Directory& saved_state, const Directory& remote)
	{
		DiffResult diffs;

		// Check all local files with saved state
		for (const auto& local_file : local.files)
		{
			if (const auto* saved_file = saved_state.find_file(local_file.get_name()))
			{
				// Check for local updates
				if (saved_file->get_last_write_time() > local_file.get_last_write_time())
					diffs.append({local_file, Diff::Operation::LocalRevert});
				else if (saved_file->get_last_write_time() < local_file.get_last_write_time())
					diffs.append({local_file, Diff::Operation::LocalNewer});
			}
			else
			{
				// This local file is totally new
				diffs.append({local_file, Diff::Operation::LocalAdded});
			}
		}

		// Check for locally removed files
		for (const auto& saved_file : saved_state.files)
			if (!local.find_file(saved_file.get_name()))
				diffs.append({saved_file, Diff::Operation::LocalDelete});

		// Compare all remote files with saved state
		for (const auto& remote_file : remote.files)
		{
			if (const auto* saved_file = saved_state.find_file(remote_file.get_name()))
			{
				// Check for remote updates
				if (saved_file->get_last_write_time() > remote_file.get_last_write_time())
					diffs.append({remote_file, Diff::Operation::RemoteRevert});
				else if (saved_file->get_last_write_time() < remote_file.get_last_write_time())
					diffs.append({remote_file, Diff::Operation::RemoteNewer});
			}
			else
			{
				// This remote file is totally new
				diffs.append({remote_file, Diff::Operation::RemoteAdded});
			}
		}

		// Check for remote removed files
		for (const auto& saved_file : saved_state.files)
			if (!remote.find_file(saved_file.get_name()))
				diffs.append({ saved_file, Diff::Operation::RemoteDelete });

		// Go deeper in directory hierarchy
		for (const auto& saved_dir : saved_state.directories)
		{
			const auto* found_local_dir = local.find_directory(saved_dir.name);
			const auto* found_remote_dir = remote.find_directory(saved_dir.name);

			// Concat with diffs inside
			if (found_local_dir && found_remote_dir)
				diffs += diff(*found_local_dir, saved_dir, *found_remote_dir);

				// Or local dir is deleted
			else if (!found_local_dir && found_remote_dir)
				for (const auto& removed_file : found_remote_dir->get_files_recursive())
					diffs.append({removed_file, Diff::Operation::LocalDelete});

				// Or remote dir is deleted
			else if (found_local_dir && !found_remote_dir)
				for (const auto& removed_file : found_local_dir->get_files_recursive())
					diffs.append({removed_file, Diff::Operation::RemoteDelete});
		}

		// New directory added locally
		for (const auto& local_dir : local.directories)
			if (!saved_state.find_directory(local_dir.name))
				for (const auto& added_file : local_dir.get_files_recursive())
					diffs.append({added_file, Diff::Operation::LocalAdded});

		// New directory added on remote
		for (const auto& remote_dir : remote.directories)
			if (!saved_state.find_directory(remote_dir.name))
				for (const auto& added_file : remote_dir.get_files_recursive())
					diffs.append({added_file, Diff::Operation::RemoteAdded});

		return diffs;
	}

	Directory Directory::init_saved_state(const Directory& local, const Directory& remote, const Directory* parent)
	{
		Directory result(parent, local.name);

		for (const auto& local_file : local.files)
		{
			// We only keep the older files if both exists
			if (const auto* remote_file = remote.find_file(local_file.get_name()))
			{
				if (remote_file->get_last_write_time() < local_file.get_last_write_time())
					result.files.emplace_back(*remote_file);
				else
					result.files.emplace_back(local_file);
			}
		}
		for (const auto& local_directory : local.directories)
		{
			if (const auto* remote_directory = remote.find_directory(local_directory.name))
			{
				const auto new_dir = init_saved_state(local_directory, *remote_directory, &result);
				result.directories.emplace_back(new_dir);
			}
		}

		return result;
	}

	std::vector<File> Directory::get_files_recursive() const
	{
		std::vector<File> rec_files = files;
		for (const auto& directory : directories)
			for (const auto& file : directory.get_files_recursive())
				rec_files.emplace_back(file);
		return rec_files;
	}
}
