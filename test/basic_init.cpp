
#include <filesystem>
#include <gtest/gtest.h>

#include "test_utils.hpp"

TEST(Init, Spawn)
{
	FileshareTestEnvironment::get();
}

TEST(Init, IsInit)
{
	auto& test_env = FileshareTestEnvironment::get();
	test_env.reopen_config();
	EXPECT_TRUE(std::filesystem::exists(test_env.get_test_data_dir() / ".fileshare/config.fileshare"));
}
