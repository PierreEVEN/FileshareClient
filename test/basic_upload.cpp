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
		for (auto& file : tree.get_files_recursive())
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
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}

TEST(Transfer, UploadClone)
{
	// Create data and send it
	auto& test_env = FileshareTestEnvironment::get_connected();
	const auto& initial_tree = test_env.fill_random_tree();

	// Upload
	EXPECT_NO_THROW({
		for (auto& file : initial_tree.get_files_recursive())
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
		for (auto& file : test_env.get_config().fetch_repos_status().get_files_recursive())
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
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}


TEST(Transfer, EditNewer)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Edit local data
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 1000);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 1000);

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalNewer);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Upload
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);


    // Clean local data
    test_env.reset_environment();

    // Clone
    EXPECT_NO_THROW({
                        for (auto& file : test_env.get_config().fetch_repos_status().get_files_recursive())
                            test_env.get_config().download_replace_file(file);
                    });

    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_dir" / "test_file.bin").size(), data1.size());

    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_dir" / "test_file.bin"), data1);
    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_file_2.bin"), data2);

    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                    for (const auto& file : tree.get_files_recursive())
                        test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}


TEST(Transfer, EditOlder)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Edit local data
    test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 1000);
    test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 1000);
    std::filesystem::last_write_time(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", std::chrono::file_clock::now() - std::chrono::years (1));
    std::filesystem::last_write_time(test_env.get_test_data_dir() / "test_file_2.bin", std::chrono::file_clock::now() - std::chrono::years (1));

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalRevert);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().download_replace_file(file.get_file());
                    });
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);


    // Clean local data
    test_env.reset_environment();

    // Clone
    EXPECT_NO_THROW({
                        for (auto& file : test_env.get_config().fetch_repos_status().get_files_recursive())
                            test_env.get_config().download_replace_file(file);
                    });

    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_dir" / "test_file.bin").size(), data1.size());

    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_dir" / "test_file.bin"), data1);
    EXPECT_EQ(test_env.get_file_data(test_env.get_test_data_dir() / "test_file_2.bin"), data2);

    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (const auto& file : tree.get_files_recursive())
                            test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}


TEST(Transfer, EditDelete)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Delete local files
    std::filesystem::remove(test_env.get_test_data_dir() / "test_dir" / "test_file.bin");
    std::filesystem::remove(test_env.get_test_data_dir() / "test_file_2.bin");

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalDelete);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().download_replace_file(file.get_file());
                    });
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Delete local files
    std::filesystem::remove(test_env.get_test_data_dir() / "test_dir" / "test_file.bin");
    std::filesystem::remove(test_env.get_test_data_dir() / "test_file_2.bin");

    // Search for diffs
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalDelete);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().send_delete_file(file.get_file());
                    });
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    EXPECT_EQ(fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive().size(), 0);

    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (const auto& file : tree.get_files_recursive())
                            test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}

TEST(Transfer, RemoteUpdate)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Make local file older
    for (auto& file : tree.get_files_recursive()) {
        std::filesystem::last_write_time(file.get_path(), std::chrono::file_clock::now() - std::chrono::years(1));
        file.set_last_write_time(fileshare::FileTimeType(std::filesystem::last_write_time(file.get_path())));
        test_env.get_config().update_saved_state(file);
    }

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::RemoteNewer);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().download_replace_file(file.get_file());
                    });
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);


    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (const auto& file : tree.get_files_recursive())
                            test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}

TEST(Transfer, RemoteDelete)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalAdded);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    for (auto& file : tree.get_files_recursive())
        test_env.get_config().update_saved_state(file);

    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::RemoteDelete);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().receive_delete_file(file.get_file());
                    });
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);
}


TEST(Transfer, RemoteAdd)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Make local file older
    for (auto& file : tree.get_files_recursive()) {
        std::filesystem::remove(file.get_path());
        test_env.get_config().update_saved_state(file, true);
    }

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::RemoteAdded);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().download_replace_file(file.get_file());
                    });
    diff = test_env.get_current_diff();

    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);


    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (const auto& file : tree.get_files_recursive())
                            test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}


TEST(Transfer, RemoteRevert)
{
    auto& test_env = FileshareTestEnvironment::get_connected();

    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());

    // Create data and send it
    create_directories(test_env.get_test_data_dir() / "test_dir");
    const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin", 100);
    const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100);
    auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    // Upload
    EXPECT_NO_THROW({
                        for (auto& file : tree.get_files_recursive())
                            test_env.get_config().upload_file(file);
                    });

    // Check
    auto diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Make remote file older
    for (auto& file : tree.get_files_recursive()) {
        std::filesystem::last_write_time(file.get_path(), std::chrono::file_clock::now() + std::chrono::years(1));
        file.set_last_write_time(fileshare::FileTimeType(std::filesystem::last_write_time(file.get_path())));
        test_env.get_config().update_saved_state(file);
    }

    // Search for diffs
    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 2);
    for (const auto& change : diff.get_changes())
        EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::RemoteRevert);
    EXPECT_EQ(diff.get_conflicts().size(), 0);

    // Download replace
    EXPECT_NO_THROW({
                        for (auto& file : diff.get_changes())
                            test_env.get_config().upload_file(file.get_file());
                    });

    diff = test_env.get_current_diff();
    EXPECT_EQ(diff.get_changes().size(), 0);
    EXPECT_EQ(diff.get_conflicts().size(), 0);


    // Cleanup
    tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
    EXPECT_NO_THROW({
                        for (const auto& file : tree.get_files_recursive())
                            test_env.get_config().send_delete_file(file);
                    });
    EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
    for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).get_files_recursive())
        std::filesystem::remove(file.get_path());
}