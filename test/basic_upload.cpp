#include <gtest/gtest.h>

#include "test_utils.hpp"
#include "fileshare/diff.hpp"

TEST(Transfer, UploadDelete)
{
	const auto& test_env = FileshareTestEnvironment::get_connected();

	// Create
	const auto& tree = test_env.fill_random_tree();

	// Check
	EXPECT_NE(tree.get_files_recursive().size(), 0);
	const auto diffs = fileshare::DiffResult(tree, test_env.get_config().get_saved_state(),
	                                         test_env.get_config().fetch_repos_status());
	EXPECT_EQ(diffs.get_changes().size(), tree.get_files_recursive().size());
	EXPECT_EQ(diffs.get_conflicts().size(), 0);
	for (const auto& diff : diffs.get_changes())
		EXPECT_EQ(diff.get_operation(), fileshare::Diff::Operation::LocalAdded);

	std::cout << std::filesystem::current_path() <<std::endl;

	// Upload
	EXPECT_NO_THROW({
		for (const auto& file : tree.get_files_recursive())
		test_env.get_config().upload_file(file);
		});

	// Check
	const auto diffs2 = fileshare::DiffResult(tree, test_env.get_config().get_saved_state(),
	                                          test_env.get_config().fetch_repos_status());
	EXPECT_EQ(diffs2.get_changes().size(), 0);
	EXPECT_EQ(diffs2.get_conflicts().size(), 0);
	for (const auto& diff : diffs2.get_changes())
		EXPECT_EQ(diff.get_operation(), fileshare::Diff::Operation::LocalAdded);

	// Delete
	EXPECT_NO_THROW({
		{
		for (const auto& file : tree.get_files_recursive())
		test_env.get_config().send_delete_file(file);
		}
		});

	// Check
	EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
}

TEST(Transfer, UploadClone)
{
	// Create data and send it
	auto& test_env = FileshareTestEnvironment::get_connected();
	const auto& initial_tree = test_env.fill_random_tree();

	// Upload
	EXPECT_NO_THROW({
		for (const auto& file : initial_tree.get_files_recursive())
		test_env.get_config().upload_file(file);
		});

	// Check
	EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(),
	          initial_tree.get_files_recursive().size());

	// Clean local data
	test_env.reset_environment();

	// Check
	const auto empty_tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
	EXPECT_EQ(empty_tree.get_files_recursive().size(), 0);

	const auto diffs2 = fileshare::DiffResult(empty_tree, test_env.get_config().get_saved_state(),
	                                         test_env.get_config().fetch_repos_status());
	EXPECT_EQ(diffs2.get_changes().size(), initial_tree.get_files_recursive().size());
	EXPECT_EQ(diffs2.get_conflicts().size(), 0);
	for (const auto& diff : diffs2.get_changes())
		EXPECT_EQ(diff.get_operation(), fileshare::Diff::Operation::RemoteAdded);

	// Clone
	EXPECT_NO_THROW({
		for (const auto& file : test_env.get_config().fetch_repos_status().get_files_recursive())
		test_env.get_config().download_replace_file(file);
		});

	// Check
	const auto filled_tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);

	const auto diffs3 = fileshare::DiffResult(filled_tree, test_env.get_config().get_saved_state(), initial_tree);
	EXPECT_EQ(diffs3.get_changes().size(), 0);
	EXPECT_EQ(diffs3.get_conflicts().size(), 0);

	// Cleanup
	EXPECT_NO_THROW({
		{
		for (const auto& file : initial_tree.get_files_recursive())
		test_env.get_config().send_delete_file(file);
		}
		});

	// Check
	EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
}
