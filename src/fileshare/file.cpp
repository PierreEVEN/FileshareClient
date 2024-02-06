#include "fileshare/file.hpp"

#include <nlohmann/json.hpp>
#include <chrono>

#include "fileshare/directory.hpp"
#include "fileshare/url.hpp"

namespace fileshare
{
	File::File(const nlohmann::json& json, const Directory* parent) :
		name(Url::decode_string(json["name"]))
	{
		const auto js_timestamp = json.find("timestamp");
		if (js_timestamp != json.end())
			last_write_time = js_timestamp->get<int64_t>();

		const auto js_file_size = json.find("timestamp");
		if (js_file_size != json.end())
			file_size = js_file_size->get<int64_t>();

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

	nlohmann::json File::serialize() const
	{
		nlohmann::json json;
		json["name"] = Url::encode_string(name);
		json["timestamp"] = last_write_time.milliseconds_since_epoch();
		json["size"] = file_size;
		return json;
	}

	FileTimeType::FileTimeType(const std::filesystem::file_time_type& time) :
		file_time(std::chrono::time_point_cast<std::chrono::milliseconds>(
			std::chrono::clock_cast<std::chrono::system_clock>(time)).time_since_epoch().count())
	{
	}

	std::filesystem::file_time_type FileTimeType::to_filesystem_time() const
	{
		const auto ms = std::chrono::milliseconds(file_time);
		const std::chrono::system_clock::time_point ms_clock(ms);
		return std::chrono::clock_cast<std::chrono::file_clock>(ms_clock);
	}
}
