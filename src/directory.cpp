#include "directory.hpp"

namespace fileshare
{
	Directory::Directory() :
		name(""),
		content_size(0),
		num_files(0)
	{
	}

	Directory::Directory(const nlohmann::json& json, Directory* parent) :
		content_size(0),
		num_files(0)
	{
		if (!json.contains("name"))
			throw std::runtime_error("Missing directory name in retrieved data");
		name = json["name"].get<std::filesystem::path>();

		if (parent)
			path = parent->get_path() / name;
		else
			path = name;

		if (json.contains("files"))
			for (const auto& entry : json["files"])
			{
				const auto file = File{entry, this};
				content_size += file.get_file_size();
				num_files += 1;
				files.emplace_back(file);
			}

		if (json.contains("directories"))
			for (const auto& entry : json["directories"])
			{
				const auto directory = Directory{entry, this};
				content_size += directory.content_size;
				num_files += directory.num_files;
				directories.emplace_back(directory);
			}
	}

	Directory::Directory(const std::filesystem::path& in_path, Directory* parent) :
		name(in_path.filename()),
		content_size(0),
		num_files(0)
	{

		if (parent)
			path = parent->get_path() / name;
		else
			path = name;

		for (const auto& entry : std::filesystem::directory_iterator(in_path))
		{
			if (entry.is_regular_file())
			{
				const auto file = File{entry, this};
				content_size += file.get_file_size();
				num_files += 1;
				files.emplace_back(file);
			}
			else if (entry.is_directory())
			{
				const auto dir = Directory{entry.path(), this};
				content_size += dir.content_size;
				num_files += dir.num_files;
				directories.emplace_back(dir);
			}
		}
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

		json["name"] = name;
		json["files"] = files_json;
		json["directories"] = directory_json;

		return json;
	}

	std::vector<Diff> Directory::diff(const Directory& other) const
	{
		std::vector<Diff> diff;

		// Search file diffs
		for (const auto& l : files)
		{
			if (const auto o = other.find_file(l.get_name()))
			{
				if (o->get_last_write_time() > l.get_last_write_time())
					diff.emplace_back(l.get_path(), Diff::Operation::LocalIsOlder);
				else if (l.get_last_write_time() > o->get_last_write_time())
					diff.emplace_back(l.get_path(), Diff::Operation::LocalIsNewer);
			}
			else
			{
				diff.emplace_back(l.get_path(), Diff::Operation::RemoteDoesNotExists);
			}
		}

		for (const auto& o : other.files)
			if (!find_file(o.get_name()))
				diff.emplace_back(o.get_path(), Diff::Operation::LocalDoesNotExists);


		// Search directory diffs
		for (const auto& l : directories)
		{
			if (const auto& o = other.find_directory(l.name))
			{
				for (const auto& l_diffs : l.diff(*o))
					diff.emplace_back(l_diffs);
			}
			else
			{
				for (const auto& diff_files : l.get_files_recursive())
					diff.emplace_back(diff_files.get_path(), Diff::Operation::RemoteDoesNotExists);
				diff.emplace_back(l.get_path(), Diff::Operation::RemoteDoesNotExists);
			}
		}

		for (const auto& o : other.directories)
			if (!find_directory(o.name))
			{
				for (const auto& diff_files : o.get_files_recursive())
					diff.emplace_back(diff_files.get_path(), Diff::Operation::LocalDoesNotExists);
				diff.emplace_back(o.get_path(), Diff::Operation::LocalDoesNotExists);
			}


		return diff;
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
