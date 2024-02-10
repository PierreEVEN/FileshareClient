#include "fileshare/directory.hpp"

#include <fstream>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <iostream>

#include "fileshare/profiler.hpp"
#include "fileshare/url.hpp"
#include "fileshare/repository.hpp"

#if WIN32
#include <io.h>
#include <fcntl.h>
#endif

namespace fileshare
{
	Directory::Directory(const Directory* parent, std::wstring in_name) :
		name(std::move(in_name))
	{
		is_root_dir = !parent;
		if (parent && !parent->is_root())
			path = parent->get_path() / name;
		else
			path = name;
	}

	Directory Directory::from_json(const nlohmann::json& json, const Directory* parent)
	{
		MEASURE_DURATION(GetTreeFromPath, "Get tree from json data");
		auto res = from_json_internal(json, parent);
        return res;
	}

	Directory Directory::from_json_internal(const nlohmann::json& json, const Directory* parent)
	{
		if (RepositoryConfig::is_interrupted())
			throw std::runtime_error("Stopped !");

		const auto& js_name = json.find("name");
		if (js_name == json.end())
			throw std::runtime_error("Missing directory name in retrieved data");
		Directory dir(parent, Url::decode_string(*js_name));

		const auto& js_files = json.find("files");
		if (js_files != json.end()) {
			for (const auto& entry : *js_files)
			{
				dir.files.emplace_back(File::from_json(entry, &dir));
			}
		}
		const auto& js_dirs = json.find("directories");
		if (js_dirs != json.end()) {
			for (const auto& entry : *js_dirs)
			{
				dir.directories.emplace_back(from_json_internal(entry, &dir));
			}
		}
		return dir;
	}

	Directory Directory::from_path(const std::filesystem::path& in_path, const Directory* parent)
	{
		MEASURE_DURATION(GetTreeFromPath, "Get tree from " + in_path.generic_string());
#if WIN32
		_setmode(_fileno(stdout), _O_U16TEXT);
#endif
		const auto res = from_path_internal(in_path, parent);
#if WIN32
		_setmode(_fileno(stdout), _O_TEXT);
#endif
		return res;
	}

	Directory Directory::from_path_internal(const std::filesystem::path& in_path, const Directory* parent)
	{
		if (RepositoryConfig::is_interrupted())
			throw std::runtime_error("Stopped !");

		Directory dir(parent, in_path.filename().wstring());

		std::unordered_set<std::filesystem::path> excluded_files = { ".fileshare" };

		if (exists(in_path / ".fileshareignore"))
		{
			std::wifstream exclusion_file(in_path / ".fileshareignore");

			std::wstring line;
			while (std::getline(exclusion_file, line))
				if (!line.empty())
				{
					excluded_files.insert(line);
				}
		}

		for (const auto& entry : std::filesystem::directory_iterator(in_path))
		{
			if (excluded_files.contains(entry.path().filename()))
				continue;
			if (entry.is_regular_file())
			{
				dir.files.emplace_back(File::from_dir_entry(entry, &dir));
			}
			else if (entry.is_directory())
				dir.directories.emplace_back(from_path_internal(entry.path(), &dir));
		}
		return dir;
	}

	nlohmann::json Directory::serialize() const
	{
		nlohmann::json json;
		std::vector<nlohmann::json> files_json;
		std::vector<nlohmann::json> directory_json;


		files_json.reserve(files.size());
		for (const auto& file : files)
			files_json.emplace_back(file.serialize());

		directory_json.reserve(directories.size());
		for (const auto& directory : directories)
			directory_json.emplace_back(directory.serialize());

		json["name"] = Url::encode_string(name);
		json["files"] = files_json;
		json["directories"] = directory_json;

		return json;
	}

	Directory Directory::init_saved_state(const Directory& local, const Directory& remote, const Directory* parent)
	{
		if (RepositoryConfig::is_interrupted())
			throw std::runtime_error("Stopped !");

		Directory result(parent, local.name);

		for (const auto& local_file : local.files)
		{
			// We only keep the older files if both exists
			if (const auto* remote_file = remote.find_file(local_file.get_name()))
			{
				if (remote_file->get_last_write_time() < local_file.get_last_write_time())
					result.files.emplace_back(*remote_file);
				else
					result.files.emplace_back(local_file);
			}
		}
		for (const auto& local_directory : local.directories)
		{
			if (const auto* remote_directory = remote.find_directory(local_directory.name))
			{
				const auto new_dir = init_saved_state(local_directory, *remote_directory, &result);
				result.directories.emplace_back(new_dir);
			}
		}

		return result;
	}

	std::vector<File> Directory::get_files_recursive() const
	{
		std::vector<File> rec_files = files;
		for (const auto& directory : directories)
			for (const auto& file : directory.get_files_recursive())
				rec_files.emplace_back(file);
		return rec_files;
	}
}
