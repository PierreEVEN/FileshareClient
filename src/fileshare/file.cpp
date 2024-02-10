#include "fileshare/file.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <utility>

#include "fileshare/directory.hpp"
#include "fileshare/url.hpp"

namespace fileshare
{
	nlohmann::json File::serialize() const
	{
		nlohmann::json json;
		json["name"] = Url::encode_string(name);
		json["timestamp"] = last_write_time.milliseconds_since_epoch();
		json["size"] = file_size;
		return json;
	}

    File File::from_path(const std::filesystem::path &path, const Directory *in_parent) {
        return {in_parent, std::filesystem::last_write_time(path), std::filesystem::file_size(path), path.filename().wstring()};
    }

    File File::from_dir_entry(const std::filesystem::directory_entry &entry, const Directory *in_parent) {
        return {in_parent, entry.last_write_time(), entry.file_size(), entry.path().filename().wstring()};
    }

    File File::from_json(const nlohmann::json &json, const Directory *in_parent) {
        const auto js_timestamp = json.find("timestamp");
        FileTimeType last_write_time(0);
        if (js_timestamp != json.end())
            last_write_time = js_timestamp->get<int64_t>();

        size_t file_size = 0;
        const auto js_file_size = json.find("size");
        if (js_file_size != json.end())
            file_size = js_file_size->get<int64_t>();

        return {in_parent, last_write_time, file_size, Url::decode_string(json["name"])};
    }

    File::File(const Directory *in_parent, const FileTimeType &last_write_time, size_t size, std::wstring in_name) :
            name(std::move(in_name)),
            last_write_time(last_write_time),
            file_size(size)
    {
        if (in_parent)
            update_parent(in_parent);
    }

    void File::update_parent(const Directory *new_parent) {
        if (new_parent && !new_parent->is_root())
            path = new_parent->get_path() / name;
        else
            path = name;
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
