#pragma once
#include <filesystem>
#include <gtest/gtest.h>

#include "fileshare/directory.hpp"
#include "fileshare/repository.hpp"

class FileshareTestEnvironment : public testing::Environment
{
public:
	FileshareTestEnvironment(int argc, char** argv);
	~FileshareTestEnvironment() override;

	// Override this to define how to set up the environment.
	void SetUp() override;

	// Override this to define how to tear down the environment.
	void TearDown() override;

	[[nodiscard]] std::filesystem::path get_test_data_dir() const;

	static FileshareTestEnvironment& get();
	static FileshareTestEnvironment& get_connected();
	void reset_environment();


	[[nodiscard]] fileshare::Directory fill_random_tree(size_t depth = 2,
	                                                    size_t objects_per_dir = 4) const;

	[[nodiscard]] fileshare::RepositoryConfig& get_config() const { return *cfg; }

	void reopen_config();

	void ensure_is_sync() const;

private:
	std::unique_ptr<fileshare::RepositoryConfig> cfg;
	void make_random_tree_internal(const std::filesystem::path& target, size_t depth, size_t objects_per_dir) const;
};
