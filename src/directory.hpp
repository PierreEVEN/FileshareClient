#pragma once
#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "file.hpp"

namespace fileshare
{
	class Diff
	{
	public:
		enum class Operation
		{
			LocalDelete,
			LocalRevert,
			LocalNewer,
			LocalAdded,
			RemoteDelete,
			RemoteRevert,
			RemoteNewer,
			RemoteAdded
		};

		Diff(File in_file, Operation in_operation, int64_t in_time_difference = 0) :
			operation(in_operation),
			file(std::move(in_file)),
			time_difference(in_time_difference)
		{
		}

		[[nodiscard]] const char* operation_str() const;

		[[nodiscard]] Operation get_operation() const { return operation; }
		[[nodiscard]] int64_t get_time_difference() const { return time_difference; }
		[[nodiscard]] const File& get_file() const { return file; }

	private:
		Operation operation;
		File file;
		int64_t time_difference;
	};

	class DiffResult
	{
	public:
		void append(const Diff&& diff)
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
			visited_paths.insert(std::pair(diff.get_file().get_path(), diff));
			file_changes.emplace_back(diff);
		}

		DiffResult& operator+=(const DiffResult& other)
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

		[[nodiscard]] const std::vector<std::pair<Diff, Diff>>& get_conflicts() const { return file_conflicts; }
		[[nodiscard]] const std::vector<Diff>& get_changes() const { return file_changes; }

	private:
		std::vector<std::pair<Diff, Diff>> file_conflicts;
		std::vector<Diff> file_changes;
		std::unordered_map<std::filesystem::path, Diff> visited_paths;
	};


	class Directory
	{
	public:
		Directory(const Directory* parent, std::filesystem::path name);
		static Directory from_json(const nlohmann::json& json, const Directory* parent);
		static Directory from_path(const std::filesystem::path& in_path, const Directory* parent);

		[[nodiscard]] nlohmann::json serialize() const;

		[[nodiscard]] const Directory* find_directory(const std::filesystem::path& name) const
		{
			for (const auto& directory : directories)
				if (directory.name == name)
					return &directory;
			return nullptr;
		}
		[[nodiscard]] Directory* find_directory(const std::filesystem::path& name)
		{
			for (auto& directory : directories)
				if (directory.name == name)
					return &directory;
			return nullptr;
		}

		[[nodiscard]] const File* find_file(const std::filesystem::path& name) const
		{
			for (const auto& file : files)
				if (file.get_name() == name)
					return &file;
			return nullptr;
		}

		void replace_insert_file(const File& new_file)
		{
			for (auto& file : files)
			{
				if (file.get_name() == new_file.get_name()) {
					file = new_file;
					file.path = get_path() / file.get_name();
					return;
				}
			}
			files.emplace_back(new_file);
			files.back().path = get_path() / files.back().get_name();
		}

		[[nodiscard]] static DiffResult diff(const Directory& local, const Directory& saved_state,
		                                     const Directory& remote);
		[[nodiscard]] static Directory init_saved_state(const Directory& local, const Directory& remote, const Directory* parent);
		[[nodiscard]] std::vector<File> get_files_recursive() const;

		[[nodiscard]] const std::filesystem::path& get_path() const { return path; }
		[[nodiscard]] bool is_root() const { return is_root_dir; }

		Directory& add_directory(const std::filesystem::path& new_dir_name)
		{
			directories.emplace_back(this, new_dir_name);
			return directories.back();
		}

	private:
		bool is_root_dir;
		std::filesystem::path name;
		std::filesystem::path path;
		std::vector<File> files;
		std::vector<Directory> directories;
	};
}
