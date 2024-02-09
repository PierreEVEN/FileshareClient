#include "fileshare/diff.hpp"

#include <iostream>
#include <ranges>

#include "fileshare/profiler.hpp"
#include "fileshare/repository.hpp"

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
					*this += Diff{local_file, Diff::Operation::LocalRevert};
				else if (saved_file->get_last_write_time() < local_file.get_last_write_time())
					*this += Diff{local_file, Diff::Operation::LocalNewer};
			}
			else
			{
				// This local file is totally new
				*this += Diff{local_file, Diff::Operation::LocalAdded};
			}
		}

		// Check for locally removed files
		for (const auto& saved_file : saved_state.get_files())
			if (!local.find_file(saved_file.get_name()))
				*this += Diff{saved_file, Diff::Operation::LocalDelete};

		// Compare all remote files with saved state
		for (const auto& remote_file : remote.get_files())
		{
			if (const auto* saved_file = saved_state.find_file(remote_file.get_name()))
			{
				// Check for remote updates
				if (saved_file->get_last_write_time() > remote_file.get_last_write_time())
					*this += Diff{remote_file, Diff::Operation::RemoteRevert};
				else if (saved_file->get_last_write_time() < remote_file.get_last_write_time())
                    *this += Diff{remote_file, Diff::Operation::RemoteNewer};
			}
			else
			{
				// This remote file is totally new
				*this += Diff{remote_file, Diff::Operation::RemoteAdded};
			}
		}

		// Check for remote removed files
		for (const auto& saved_file : saved_state.get_files())
			if (!remote.find_file(saved_file.get_name()))
				*this += Diff{saved_file, Diff::Operation::RemoteDelete};

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
					*this += Diff{removed_file, Diff::Operation::LocalDelete};

				// Or remote dir is deleted
			else if (found_local_dir && !found_remote_dir)
				for (const auto& removed_file : found_local_dir->get_files_recursive())
					*this += Diff{removed_file, Diff::Operation::RemoteDelete};
		}

		// New directory added locally
		for (const auto& local_dir : local.get_directories())
			if (!saved_state.find_directory(local_dir.get_name()))
				for (const auto& added_file : local_dir.get_files_recursive())
					*this += Diff{added_file, Diff::Operation::LocalAdded};

		// New directory added on remote
		for (const auto& remote_dir : remote.get_directories())
			if (!saved_state.find_directory(remote_dir.get_name()))
				for (const auto& added_file : remote_dir.get_files_recursive())
					*this += Diff{added_file, Diff::Operation::RemoteAdded};

		// Check for directories deleted on both sides
		for (const auto& saved_dir : saved_state.get_directories())
			if (!local.find_directory(saved_dir.get_name()) && !remote.find_directory(saved_dir.get_name()))
				for (const auto& removed_file : saved_dir.get_files_recursive())
				{
					*this += Diff{removed_file, Diff::Operation::LocalDelete};
					*this += Diff{removed_file, Diff::Operation::RemoteDelete};
				}
	}

	DiffResult& DiffResult::operator+=(const Diff& other)
	{
		const auto existing_diff = file_changes.find(other.get_file().get_path());
		// conflict detected
		if (existing_diff != file_changes.end())
		{
			std::optional<Diff> local;
			std::optional<Diff> remote;

			switch (other.get_operation())
			{
			case Diff::Operation::LocalAdded:
			case Diff::Operation::LocalDelete:
			case Diff::Operation::LocalRevert:
			case Diff::Operation::LocalNewer:
				local = other;
				break;
			default:
				remote = other;
			}
			switch (existing_diff->second.get_operation())
			{
			case Diff::Operation::LocalAdded:
			case Diff::Operation::LocalDelete:
			case Diff::Operation::LocalRevert:
			case Diff::Operation::LocalNewer:
				if (local)
					throw std::runtime_error(
						"This conflict is not possible !!\n\t- Previous local " + std::string(local->operation_str()) +
						" : " + local->get_file().get_path().generic_string() + "\n\t- New local " +
						std::string(existing_diff->second.operation_str()) + " : " + existing_diff->second.get_file().
						get_path().generic_string());
				local = existing_diff->second;
				break;
			default:
				if (remote)
					throw std::runtime_error(
						"This conflict is not possible !!\n\t- Previous remote " + std::string(remote->operation_str()) +
						" : " + remote->get_file().get_path().generic_string() + "\n\t- New remote " +
						std::string(existing_diff->second.operation_str()) + " : " + existing_diff->second.get_file().
						get_path().generic_string());
				remote = existing_diff->second;
			}

			file_changes.erase(existing_diff);

			// If the file was added on both side but is the same : clear it from the conflicts
			if (local->get_operation() == Diff::Operation::LocalAdded && remote->get_operation() ==
				Diff::Operation::RemoteAdded)
			{
				if (local->get_file().get_last_write_time() == remote->get_file().get_last_write_time())
				{
					file_conflicts.emplace_back(*local, *remote);
					return *this;
				}

				if (local->get_file().get_last_write_time() > remote->get_file().get_last_write_time())
					file_conflicts.emplace_back(Diff{local->get_file(), Diff::Operation::LocalNewer}, *remote);
				else
					file_conflicts.emplace_back(*local, Diff{remote->get_file(), Diff::Operation::RemoteNewer});
				return *this;
			}

			file_conflicts.emplace_back(*local, *remote);
		}
		else
		{
			file_changes.insert(std::pair(other.get_file().get_path(), other));
		}
		return *this;
	}

	DiffResult& DiffResult::operator+=(const DiffResult& other)
	{
		for (const auto& path : other.file_changes)
		{
			if (file_changes.contains(path.first))
				throw std::runtime_error("Concatenation of same path in DiffResult");
			file_changes.insert(path);
		}
		file_conflicts.insert(file_conflicts.begin(), other.file_conflicts.begin(), other.file_conflicts.end());

		return *this;
	}

	std::vector<Diff> DiffResult::get_changes() const
	{
		std::vector<Diff> changes;
		changes.reserve(file_changes.size());
		for (const auto& val : file_changes | std::views::values)
			changes.emplace_back(val);
		return changes;
	}

	DiffResult measure_diff(const Directory& local, const Directory& saved_state, const Directory& remote)
	{
		MEASURE_DURATION(ComputeDiff, "Compute server delta");
		return {local, saved_state, remote};
	}
}
