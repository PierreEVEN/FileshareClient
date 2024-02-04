#pragma once
#include <unordered_map>

#include "file.hpp"

namespace fileshare {
	class Diff
	{
	public:
		enum class Operation
		{
			LocalDelete,
			LocalRevert,
			LocalNewer,
			LocalAdded,
			RemoteDelete,
			RemoteRevert,
			RemoteNewer,
			RemoteAdded
		};

		Diff(File in_file, Operation in_operation, int64_t in_time_difference = 0) :
			operation(in_operation),
			file(std::move(in_file)),
			time_difference(in_time_difference)
		{
		}

		[[nodiscard]] const char* operation_str() const;

		[[nodiscard]] Operation get_operation() const { return operation; }
		[[nodiscard]] int64_t get_time_difference() const { return time_difference; }
		[[nodiscard]] const File& get_file() const { return file; }

	private:
		Operation operation;
		File file;
		int64_t time_difference;
	};


	class DiffResult
	{
	public:
		DiffResult(const Directory& local, const Directory& saved_state,
			const Directory& remote);

		void append(const Diff&& diff);

		DiffResult& operator+=(const DiffResult& other);

		[[nodiscard]] const std::vector<std::pair<Diff, Diff>>& get_conflicts() const { return file_conflicts; }
		[[nodiscard]] const std::vector<Diff>& get_changes() const { return file_changes; }

	private:
		DiffResult() = default;
		std::vector<std::pair<Diff, Diff>> file_conflicts;
		std::vector<Diff> file_changes;
		std::unordered_map<std::filesystem::path, Diff> visited_paths;
	};

	DiffResult measure_diff(const Directory& local, const Directory& saved_state, const Directory& remote);
}
