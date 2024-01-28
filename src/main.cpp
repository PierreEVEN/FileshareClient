

#include <nlohmann/json.hpp>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <optional>
#include <filesystem>

class FileTimeType {
public:
	FileTimeType(const std::filesystem::file_time_type& time) : 
		file_time(std::chrono::time_point_cast<std::chrono::seconds>(time).time_since_epoch().count()) {}

	FileTimeType(const uint64_t& time) : file_time(time) {}

	bool operator<(const FileTimeType& other) const { return file_time < other.file_time; }
	bool operator>(const FileTimeType& other) const { return file_time > other.file_time; }
	bool operator==(const FileTimeType& other) const { return file_time == other.file_time; }

	friend struct nlohmann::adl_serializer<FileTimeType>;
private:
	uint64_t file_time;
};

namespace nlohmann {
	template <>
	struct adl_serializer<FileTimeType> {
		static void to_json(json& j, const FileTimeType& value) {
			j = value.file_time;
		}

		static void from_json(const json& j, FileTimeType& value) {
			value.file_time = j.template get<uint64_t>();
		}
	};
}

class Diff {
public:
	enum class Operation {
		RemoteDoesNotExists,
		LocalDoesNotExists,
		LocalIsOlder,
		LocalIsNewer,
	};
	Diff(std::filesystem::path in_path, Operation in_operation) : path(std::move(in_path)), operation(std::move(in_operation)) {}
private:
	const Operation operation;
	const std::filesystem::path path;
};

class File {
public:

	File(const nlohmann::json& json) : path(json["path"].get<std::filesystem::path>()), file_size(json["size"]), last_write_time(json["time"]) {

	}
	File(const std::filesystem::directory_entry& in_dir_entry) :
		path(in_dir_entry),
		last_write_time(in_dir_entry.last_write_time()),
		file_size(in_dir_entry.file_size())
	{
	}
	const size_t& get_file_size() const { return file_size; }
	const std::filesystem::path& get_path() const { return path; }
	const FileTimeType& get_last_write_time() const { return last_write_time; }

	nlohmann::json serialize() const {
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

class Directory {
public:

	Directory(const nlohmann::json& json) : path(json["path"].get<std::filesystem::path>()), content_size(0), num_files(0) {
		for (const auto& entry : json["files"]) {
			const auto file = File{ entry };
			content_size += file.get_file_size();
			num_files += 1;
			files.emplace_back(file);
		}		
		for (const auto& entry : json["directories"]) {
			const auto directory = Directory{ entry };
			content_size += directory.content_size;
			num_files += directory.num_files;
			directories.emplace_back(directory);
		}
	}

	Directory(std::filesystem::path in_path) : path(std::move(in_path)), content_size(0), num_files(0) {
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_regular_file()) {
				const auto file = File{ entry };
				content_size += file.get_file_size();
				num_files += 1;
				files.emplace_back(file);
			}
			else if (entry.is_directory()) {
				const auto dir = Directory{ entry.path()};
				content_size += dir.content_size;
				num_files += dir.num_files;
				directories.emplace_back(dir);
			}
		}
	}

	nlohmann::json serialize() const {
		nlohmann::json json;
		std::vector<nlohmann::json> files_json;
		std::vector<nlohmann::json> directory_json;


		for (const auto& file : files)
			files_json.emplace_back(file.serialize());

		for (const auto& directory : directories)
			directory_json.emplace_back(directory.serialize());

		json["files"] = files_json;
		json["directories"] = directory_json;

		return json;
	}

	const Directory* find_directory(const std::filesystem::path& path) const {
		for (const auto& directory : directories)
			if (directory.path == path)
				return &directory;
		return nullptr;
	}

	const File* find_file(const std::filesystem::path& path) const {
		for (const auto& file : files)
			if (file.get_path() == path)
				return &file;
		return nullptr;
	}

	/// <summary>
	/// Get local differences agains other directory
	/// </summary>
	/// <param name="other">other directory</param>
	/// <returns></returns>
	std::vector<Diff> diff(const Directory& other) const {
		std::vector<Diff> diff;

		// Search file diffs
		for (const auto& l : files) {
			if (const auto o = other.find_file(l.get_path())) {
				if (o->get_last_write_time() > l.get_last_write_time())
					diff.emplace_back(Diff{l.get_path(), Diff::Operation::LocalIsOlder});
				else if (l.get_last_write_time() > o->get_last_write_time())
					diff.emplace_back(Diff{ l.get_path(), Diff::Operation::LocalIsNewer });
			}
			else {
				diff.emplace_back(Diff{ l.get_path(), Diff::Operation::RemoteDoesNotExists });
			}
		}

		for (const auto& o : other.files)
			if (!find_file(o.get_path()))
				diff.emplace_back(Diff{ o.get_path(), Diff::Operation::LocalDoesNotExists });


		// Search directory diffs
		for (const auto& l : directories) {
			if (const auto o = other.find_directory(l.path)) {
				for (const auto& l_diffs : l.diff(*o))
					diff.emplace_back(l_diffs);
			}
			else {
				diff.emplace_back(Diff{ l.path, Diff::Operation::RemoteDoesNotExists });
			}
		}

		for (const auto& o : other.directories)
			if (!find_directory(o.path))
				diff.emplace_back(Diff{ o.path, Diff::Operation::LocalDoesNotExists });


		return diff;
	}

private:
	const std::filesystem::path path;
	std::vector<File> files;
	std::vector<Directory> directories;
	size_t content_size;
	size_t num_files;
};

class RepositoryState {
public:
	RepositoryState(const nlohmann::json& json) : root(json["root"]) {

	}

	nlohmann::json serialize() const {
		nlohmann::json json;
		json["root"] = root.serialize();
		return json;
	}
private:
	Directory root;
};

class Repository {
public:

	Repository(const nlohmann::json& json) : 
		local_path(json["local_path"].get<std::filesystem::path>()),
		remote_url(json["remote_url"]),
		user_token(json["user_token"]),
		state(json["state"]) {

	}

	nlohmann::json serialize() const {
		nlohmann::json json;
		json["local_path"] = local_path;
		json["remote_url"] = remote_url;
		json["user_token"] = user_token;
		json["state"] = state.serialize();
		return json;
	}
	const std::filesystem::path& get_local_path() const { return local_path; }

private:
	std::filesystem::path local_path;
	std::string remote_url;
	std::string user_token;
	RepositoryState state;
};


class AppConfig {
public:

	AppConfig(std::filesystem::path in_config_path) : config_path(std::move(in_config_path)) {
		load_config();
	}

	void save_config() {

	}
	void load_config() {

	}

	const std::vector<Repository>& get_active_repositories() const { return active_repositories; }

private:
	std::vector<Repository> active_repositories;
	const std::filesystem::path config_path;
};

int main(int argc, char** argv) {

	AppConfig config("./config.json");

	for (const auto& repo : config.get_active_repositories()) {
		const auto files = Directory(repo.get_local_path());
	}

	cURLpp::initialize(CURL_GLOBAL_ALL);

	cURLpp::Easy test;
}