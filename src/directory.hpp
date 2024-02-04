#pragma once
#include <filesystem>

#include "file.hpp"

namespace fileshare
{

	class Directory
	{
	public:
		Directory(const Directory* parent, std::wstring name);
		static Directory from_json(const nlohmann::json& json, const Directory* parent);
		static Directory from_path(const std::filesystem::path& in_path, const Directory* parent);

		[[nodiscard]] nlohmann::json serialize() const;

		[[nodiscard]] const Directory* find_directory(const std::wstring& in_name) const
		{
			for (const auto& directory : directories)
				if (directory.name == in_name)
					return &directory;
			return nullptr;
		}
		[[nodiscard]] Directory* find_directory(const std::wstring& in_name)
		{
			for (auto& directory : directories)
				if (directory.name == in_name)
					return &directory;
			return nullptr;
		}

		[[nodiscard]] const File* find_file(const std::wstring& in_name) const
		{
			for (const auto& file : files)
				if (file.get_name() == in_name)
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

		void delete_file(const std::wstring& file_name)
		{
			for (auto ite = files.begin(); ite != files.end(); ++ite)
			{
				if (ite->get_name() == file_name) {
					files.erase(ite);
					return;
				}
			}
		}

		[[nodiscard]] static Directory init_saved_state(const Directory& local, const Directory& remote, const Directory* parent);
		[[nodiscard]] std::vector<File> get_files_recursive() const;

		[[nodiscard]] bool is_root() const { return is_root_dir; }

		Directory& add_directory(const std::wstring& new_dir_name)
		{
			directories.emplace_back(this, new_dir_name);
			return directories.back();
		}

		[[nodiscard]] const std::vector<File>& get_files() const { return files; }
		[[nodiscard]] const std::vector<Directory>& get_directories() const { return directories; }
		[[nodiscard]] const std::wstring& get_name() const { return name; }
		[[nodiscard]] const std::filesystem::path& get_path() const { return path; }

	private:
		static Directory from_json_internal(const nlohmann::json& json, const Directory* parent);
		static Directory from_path_internal(const std::filesystem::path& in_path, const Directory* parent);
		bool is_root_dir;
		std::wstring name;
		std::filesystem::path path;
		std::vector<File> files;
		std::vector<Directory> directories;
	};
}
