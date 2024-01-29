#include "file.hpp"

#include "directory.hpp"

namespace fileshare
{
	File::File(const nlohmann::json& json, const Directory* parent) :
		name(json["name"].get<std::filesystem::path>()),
		last_write_time(json.contains("timestamp") ? json["timestamp"] : 0),
		file_size(json.contains("size") ? json["size"] : 0)
	{
		if (parent)
			path = parent->get_path() / name;
		else
			path = name;
	}

	File::File(const std::filesystem::directory_entry& in_dir_entry, const Directory* parent) :
		name(in_dir_entry.path().filename()),
		last_write_time(in_dir_entry.last_write_time()),
		file_size(in_dir_entry.file_size())
	{
		if (parent)
			path = parent->get_path() / name;
		else
			path = name;
	}
}
