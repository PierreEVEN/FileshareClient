#pragma once
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

namespace fileshare
{
	class FileTimeType
	{
	public:
		FileTimeType(const std::filesystem::file_time_type& time) :
			file_time(std::chrono::time_point_cast<std::chrono::seconds>(time).time_since_epoch().count())
		{
		}

		FileTimeType(const uint64_t& time) : file_time(time)
		{
		}

		bool operator<(const FileTimeType& other) const { return file_time < other.file_time; }
		bool operator>(const FileTimeType& other) const { return file_time > other.file_time; }
		bool operator==(const FileTimeType& other) const { return file_time == other.file_time; }

		friend struct nlohmann::adl_serializer<FileTimeType>;
	private:
		uint64_t file_time;
	};
}

template <>
struct ::nlohmann::adl_serializer<fileshare::FileTimeType>
{
	static void to_json(json& j, const fileshare::FileTimeType& value)
	{
		j = value.file_time;
	}

	static void from_json(const json& j, fileshare::FileTimeType& value)
	{
		value.file_time = j.get<uint64_t>();
	}
};

namespace fileshare
{
	class File
	{
	public:
		File(const nlohmann::json& json) :
			path(json["path"].get<std::filesystem::path>()),
			last_write_time(json["time"]),
			file_size(json["size"])
		{
		}

		File(const std::filesystem::directory_entry& in_dir_entry) :
			path(in_dir_entry),
			last_write_time(in_dir_entry.last_write_time()),
			file_size(in_dir_entry.file_size())
		{
		}

		[[nodiscard]] const size_t& get_file_size() const { return file_size; }
		[[nodiscard]] const std::filesystem::path& get_path() const { return path; }
		[[nodiscard]] const FileTimeType& get_last_write_time() const { return last_write_time; }

		[[nodiscard]] nlohmann::json serialize() const
		{
			nlohmann::json json;
			json["path"] = path;
			json["time"] = last_write_time;
			json["size"] = file_size;
			return json;
		}

	private:
		const std::filesystem::path path;
		const FileTimeType last_write_time;
		const size_t file_size;
	};
}
