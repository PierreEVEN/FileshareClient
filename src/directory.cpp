#include "directory.hpp"

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

	Directory::Directory(const Directory* parent, std::filesystem::path in_name) :
		name(std::move(in_name)),
		content_size(0),
		num_files(0)
	{
		if (parent)
			path = parent->get_path() / name;
		else
			path = name;
	}

	Directory Directory::from_json(const nlohmann::json& json, const Directory* parent)
	{
		if (!json.contains("name"))
			throw std::runtime_error("Missing directory name in retrieved data");
		Directory dir(parent, json["name"].get<std::filesystem::path>());

		if (json.contains("files"))
			for (const auto& entry : json["files"])
			{
				const auto file = File{entry, &dir};
				dir.content_size += file.get_file_size();
				dir.num_files += 1;
				dir.files.emplace_back(file);
			}

		if (json.contains("directories"))
			for (const auto& entry : json["directories"])
			{
				const auto directory = from_json(entry, &dir);
				dir.content_size += directory.content_size;
				dir.num_files += directory.num_files;
				dir.directories.emplace_back(directory);
			}
		return dir;
	}

	Directory Directory::from_path(const std::filesystem::path& in_path, const Directory* parent)
	{
		Directory dir(parent, in_path.filename());
		
		for (const auto& entry : std::filesystem::directory_iterator(in_path))
		{
			if (entry.is_regular_file())
			{
				const auto file = File{ entry, &dir };
				dir.content_size += file.get_file_size();
				dir.num_files += 1;
				dir.files.emplace_back(file);
			}
			else if (entry.is_directory())
			{
				const auto directory = from_path( entry.path(), &dir );
				dir.content_size += directory.content_size;
				dir.num_files += directory.num_files;
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

		json["name"] = name;
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

		// Check all remote files with saved state
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
				result.num_files += 1;
				result.content_size += result.files.back().get_file_size();
			}
		}
		for (const auto& local_directory : local.directories)
		{
			if (const auto* remote_directory = remote.find_directory(local_directory.name))
			{
				const auto new_dir = init_saved_state(local_directory, *remote_directory, &result);
				result.num_files += new_dir.num_files;
				result.content_size += new_dir.content_size;
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
