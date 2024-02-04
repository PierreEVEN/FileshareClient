#include "diff.hpp"

#include "profiler.hpp"
#include "repository.hpp"

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


	DiffResult::DiffResult(const Directory& local, const Directory& saved_state, const Directory& remote)
	{
		if (RepositoryConfig::is_interrupted())
			throw std::runtime_error("Stopped !");
		
		// Check all local files with saved state
		for (const auto& local_file : local.get_files())
		{
			if (const auto* saved_file = saved_state.find_file(local_file.get_name()))
			{
				// Check for local updates
				if (saved_file->get_last_write_time() > local_file.get_last_write_time())
					append({ local_file, Diff::Operation::LocalRevert });
				else if (saved_file->get_last_write_time() < local_file.get_last_write_time())
					append({ local_file, Diff::Operation::LocalNewer });
			}
			else
			{
				// This local file is totally new
				append({ local_file, Diff::Operation::LocalAdded });
			}
		}

		// Check for locally removed files
		for (const auto& saved_file : saved_state.get_files())
			if (!local.find_file(saved_file.get_name()))
				append({ saved_file, Diff::Operation::LocalDelete });

		// Compare all remote files with saved state
		for (const auto& remote_file : remote.get_files())
		{
			if (const auto* saved_file = saved_state.find_file(remote_file.get_name()))
			{
				// Check for remote updates
				if (saved_file->get_last_write_time() > remote_file.get_last_write_time())
					append({ remote_file, Diff::Operation::RemoteRevert });
				else if (saved_file->get_last_write_time() < remote_file.get_last_write_time())
					append({ remote_file, Diff::Operation::RemoteNewer });
			}
			else
			{
				// This remote file is totally new
				append({ remote_file, Diff::Operation::RemoteAdded });
			}
		}

		// Check for remote removed files
		for (const auto& saved_file : saved_state.get_files())
			if (!remote.find_file(saved_file.get_name()))
				append({ saved_file, Diff::Operation::RemoteDelete });

		// Go deeper in directory hierarchy
		for (const auto& saved_dir : saved_state.get_directories())
		{
			const auto* found_local_dir = local.find_directory(saved_dir.get_name());
			const auto* found_remote_dir = remote.find_directory(saved_dir.get_name());

			// Concat with diffs inside
			if (found_local_dir && found_remote_dir)
				*this += DiffResult(*found_local_dir, saved_dir, *found_remote_dir);

			// Or local dir is deleted
			else if (!found_local_dir && found_remote_dir)
				for (const auto& removed_file : found_remote_dir->get_files_recursive())
					append({ removed_file, Diff::Operation::LocalDelete });

			// Or remote dir is deleted
			else if (found_local_dir && !found_remote_dir)
				for (const auto& removed_file : found_local_dir->get_files_recursive())
					append({ removed_file, Diff::Operation::RemoteDelete });
		}

		// New directory added locally
		for (const auto& local_dir : local.get_directories())
			if (!saved_state.find_directory(local_dir.get_name()))
				for (const auto& added_file : local_dir.get_files_recursive())
					append({ added_file, Diff::Operation::LocalAdded });

		// New directory added on remote
		for (const auto& remote_dir : remote.get_directories())
			if (!saved_state.find_directory(remote_dir.get_name()))
				for (const auto& added_file : remote_dir.get_files_recursive())
					append({ added_file, Diff::Operation::RemoteAdded });
	}


	DiffResult& DiffResult::operator+=(const DiffResult& other)
	{
		for (const auto& path : other.visited_paths)
		{
			if (visited_paths.contains(path.first))
				throw std::runtime_error("Concatenation of same path in DiffResult");
			visited_paths.insert(path);
		}
		file_conflicts.insert(file_conflicts.begin(), other.file_conflicts.begin(), other.file_conflicts.end());
		file_changes.insert(file_changes.begin(), other.file_changes.begin(), other.file_changes.end());

		return *this;
	}

	DiffResult measure_diff(const Directory& local, const Directory& saved_state, const Directory& remote)
	{
		MEASURE_DURATION(ComputeDiff, "Compute server delta");
		return { local, saved_state, remote };
	}
}
