#pragma once
#include <filesystem>
#include <iostream>

#include "file.hpp"

namespace fileshare {

	class Diff
	{
	public:
		enum class Operation
		{
			RemoteDoesNotExists,
			LocalDoesNotExists,
			LocalIsOlder,
			LocalIsNewer,
		};

		Diff(std::filesystem::path in_path, Operation in_operation) :
			operation(in_operation),
			path(std::move(in_path))
		{
		}

		[[nodiscard]] Operation get_operation() const { return operation; }
		[[nodiscard]] const std::filesystem::path& get_path() const { return path; }

	private:
		const Operation operation;
		const std::filesystem::path path;
	};

	class Directory
	{
	public:
		Directory();
		Directory(const nlohmann::json& json);

		Directory(std::filesystem::path in_path);

		[[nodiscard]] nlohmann::json serialize() const;

		[[nodiscard]] const Directory* find_directory(const std::filesystem::path& dir_path) const
		{
			for (const auto& directory : directories)
				if (directory.path == dir_path)
					return &directory;
			return nullptr;
		}

		[[nodiscard]] const File* find_file(const std::filesystem::path& file_path) const
		{
			for (const auto& file : files)
				if (file.get_path() == file_path)
					return &file;
			return nullptr;
		}

		/// <summary>
		/// Get local differences agains other directory
		/// </summary>
		/// <param name="other">other directory</param>
		/// <returns></returns>
		[[nodiscard]] std::vector<Diff> diff(const Directory& other) const;

		[[nodiscard]] std::vector<File> get_files_recursive() const;

	private:
		std::filesystem::path path;
		std::vector<File> files;
		std::vector<Directory> directories;
		size_t content_size;
		size_t num_files;
	};
}