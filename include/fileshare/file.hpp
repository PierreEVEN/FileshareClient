#pragma once
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace fileshare
{
	class Directory;

	class FileTimeType
	{
	public:
		FileTimeType(const std::filesystem::file_time_type& time);

		FileTimeType(const uint64_t& time) : file_time(time)
		{
		}

		bool operator<(const FileTimeType& other) const { return file_time < other.file_time; }
		bool operator>(const FileTimeType& other) const { return file_time > other.file_time; }
		bool operator==(const FileTimeType& other) const { return file_time == other.file_time; }
		int64_t operator-(const FileTimeType& other) const { return static_cast<int64_t>(file_time) - other.file_time; }

		friend struct nlohmann::adl_serializer<FileTimeType>;
		[[nodiscard]] uint64_t milliseconds_since_epoch() const { return file_time; }
		[[nodiscard]] std::filesystem::file_time_type to_filesystem_time() const;
	private:
		uint64_t file_time;
	};
}

namespace fileshare
{
	class File
	{
		friend class Directory;
        File(const Directory* in_parent, const FileTimeType& last_write_time, size_t size, std::wstring  name );
	public:
        static File from_path(const std::filesystem::path& in_path, const Directory* in_parent);
        static File from_dir_entry(const std::filesystem::directory_entry& in_path, const Directory* in_parent);
        static File from_json(const nlohmann::json& in_path, const Directory* in_parent);

		[[nodiscard]] const size_t& get_file_size() const { return file_size; }
		[[nodiscard]] const std::filesystem::path& get_path() const { return path; }
		[[nodiscard]] const std::wstring& get_name() const { return name; }
		[[nodiscard]] const FileTimeType& get_last_write_time() const { return last_write_time; }
        void set_last_write_time(const FileTimeType& new_write_time) { last_write_time = new_write_time; }

		[[nodiscard]] nlohmann::json serialize() const;
        void update_parent(const Directory* new_parent);
	private:
		std::wstring name;
		std::filesystem::path path;
		FileTimeType last_write_time{0};
		size_t file_size{0};
	};
}
