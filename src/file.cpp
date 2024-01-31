#include "file.hpp"

#include "directory.hpp"

namespace fileshare
{
	File::File(const nlohmann::json& json, const Directory* parent) :
		name(json["name"].get<std::filesystem::path>()),
		last_write_time(json.contains("timestamp") ? json["timestamp"].get<int64_t>() : 0),
		file_size(json.contains("size") ? json["size"].get<int64_t>() : 0)
	{
		if (parent && !parent->is_root())
			path = parent->get_path() / name;
		else
			path = name;
	}

	File::File(const std::filesystem::directory_entry& in_dir_entry, const Directory* parent) :
		name(in_dir_entry.path().filename()),
		last_write_time(in_dir_entry.last_write_time()),
		file_size(in_dir_entry.file_size())
	{
		if (parent && !parent->is_root())
			path = parent->get_path() / name;
		else
			path = name;
	}

	FileTimeType::FileTimeType(const std::filesystem::file_time_type& time) :
		file_time(std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::clock_cast<std::chrono::system_clock>(time)).time_since_epoch().count())
	{
	}

	std::filesystem::file_time_type FileTimeType::to_filesystem_time() const
	{
		const auto ms = std::chrono::milliseconds(file_time);
		const std::chrono::system_clock::time_point ms_clock(ms);
		return std::chrono::clock_cast<std::chrono::file_clock>(ms_clock);
	}
}
