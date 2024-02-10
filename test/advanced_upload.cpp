#include <gtest/gtest.h>
#include <cmath>
#include <fstream>

#include "test_utils.hpp"
#include "fileshare/diff.hpp"
#include "fileshare/url.hpp"

TEST(TransferAdvanced, WeirdNames)
{
	std::wstring forbidden_chars = L"<>:\"/\\|?*\n\r";
	for (int i = 128; i < 160; ++i)
		forbidden_chars += static_cast<wchar_t>(i);

	auto& test_env = FileshareTestEnvironment::get_connected();

	size_t max = UCHAR_MAX;
	size_t start = 32;
	while (start < max)
	{
		size_t end = std::min(max, start + 100);


		std::wstring name;
		for (size_t i = start; i < end; ++i)
		{
			if (forbidden_chars.find_first_of(static_cast<wchar_t>(i)) != std::wstring::npos)
				continue;
			name += static_cast<wchar_t>(i);
		}

		const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / name / name, 10);
		
		auto diff = test_env.get_current_diff();
		EXPECT_EQ(diff.get_changes().size(), 1);
		for (const auto& change : diff.get_changes())
			EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalAdded);
		EXPECT_EQ(diff.get_conflicts().size(), 0);
		
		// Upload
		EXPECT_NO_THROW({
			for (auto& file : diff.get_changes())
				test_env.get_config().upload_file(file.get_file());
			});

		// Check
		diff = test_env.get_current_diff();
		for (const auto& diffs : diff.get_changes())
			std::wcout << diffs.operation_str() << " " << diffs.get_file().get_path() << std::endl;

		for (const auto& diffs : diff.get_changes())
			std::cout << diffs.operation_str() << " " << fileshare::Url::encode_string(
				diffs.get_file().get_path().generic_wstring()) << std::endl;

		EXPECT_EQ(diff.get_changes().size(), 0);
		EXPECT_EQ(diff.get_conflicts().size(), 0);

		start = end;
		break;
	}

	// Cleanup
	auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
	EXPECT_NO_THROW({
		for (const auto& file : tree.get_files_recursive())
		test_env.get_config().send_delete_file(file);
		});
	EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
	for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).
	     get_files_recursive())
		std::filesystem::remove(file.get_path());

}

TEST(TransferAdvanced, UploadBigFile)
{
	auto& test_env = FileshareTestEnvironment::get_connected();

	for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).
	     get_files_recursive())
		std::filesystem::remove(file.get_path());

	// Create data and send it
	create_directories(test_env.get_test_data_dir() / "test_dir");
	const auto data1 = test_env.edit_file(test_env.get_test_data_dir() / "test_dir" / "test_file.bin",
	                                      100 * 1024 * 1024); // 100Mo
	const auto data2 = test_env.edit_file(test_env.get_test_data_dir() / "test_file_2.bin", 100 * 1024 * 1024);
	auto tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);

	auto diff = test_env.get_current_diff();
	EXPECT_EQ(diff.get_changes().size(), 2);
	for (const auto& change : diff.get_changes())
		EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::LocalAdded);
	EXPECT_EQ(diff.get_conflicts().size(), 0);

	// Upload
	EXPECT_NO_THROW({
		for (auto& file : tree.get_files_recursive())
		test_env.get_config().upload_file(file);
		});

	// Check
	diff = test_env.get_current_diff();
	EXPECT_EQ(diff.get_changes().size(), 0);
	EXPECT_EQ(diff.get_conflicts().size(), 0);

	test_env.reset_environment();

	diff = test_env.get_current_diff();
	EXPECT_EQ(diff.get_changes().size(), 2);
	for (const auto& change : diff.get_changes())
		EXPECT_EQ(change.get_operation(), fileshare::Diff::Operation::RemoteAdded);
	EXPECT_EQ(diff.get_conflicts().size(), 0);

	for (auto& change : diff.get_changes())
		test_env.get_config().download_replace_file(change.get_file());

	EXPECT_EQ(data1, test_env.get_file_data(test_env.get_test_data_dir() / "test_dir" / "test_file.bin"));
	EXPECT_EQ(data2, test_env.get_file_data(test_env.get_test_data_dir() / "test_file_2.bin"));

	// Cleanup
	tree = fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr);
	EXPECT_NO_THROW({
		for (const auto& file : tree.get_files_recursive())
		test_env.get_config().send_delete_file(file);
		});
	EXPECT_EQ(test_env.get_config().fetch_repos_status().get_files_recursive().size(), 0);
	for (const auto& file : fileshare::Directory::from_path(test_env.get_test_data_dir(), nullptr).
	     get_files_recursive())
		std::filesystem::remove(file.get_path());
}
