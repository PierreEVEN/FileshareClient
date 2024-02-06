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

		Diff(File in_file, Operation in_operation) :
			operation(in_operation),
			file(std::move(in_file))
		{
		}

		[[nodiscard]] const char* operation_str() const;

		[[nodiscard]] Operation get_operation() const { return operation; }
		[[nodiscard]] const File& get_file() const { return file; }

	private:
		Operation operation;
		File file;
	};


	class DiffResult
	{
	public:
		DiffResult(const Directory& local, const Directory& saved_state,
			const Directory& remote);

		DiffResult& operator+=(const Diff& other);
		DiffResult& operator+=(const DiffResult& other);

		[[nodiscard]] const std::vector<std::pair<Diff, Diff>>& get_conflicts() const { return file_conflicts; }
		[[nodiscard]] std::vector<Diff> get_changes() const;
		[[nodiscard]] std::pair<Diff, Diff> pop_conflict()
		{
			const auto last = file_conflicts.back();
			file_conflicts.pop_back();
			return last;
		}

	private:
		DiffResult() = default;
		std::vector<std::pair<Diff, Diff>> file_conflicts;
		std::unordered_map<std::filesystem::path, Diff> file_changes;
	};

	DiffResult measure_diff(const Directory& local, const Directory& saved_state, const Directory& remote);
}
