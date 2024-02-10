
#include "test_config.hpp"
#include "test_utils.hpp"
#include "fileshare/url.hpp"
#include "fileshare/exceptions.hpp"

TEST(Login, IsConnected)
{
	const auto& test_env = FileshareTestEnvironment::get();
	if (test_env.get_config().is_connected())
		test_env.get_config().logout();
	EXPECT_FALSE(test_env.get_config().is_connected());
}

TEST(Login, SetGetUrl)
{
	auto& test_env = FileshareTestEnvironment::get();
	EXPECT_FALSE(test_env.get_config().is_connected());

	test_env.get_config().set_full_url(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_REPOS));
	test_env.reopen_config();
	EXPECT_EQ(test_env.get_config().get_full_url(), std::string(FILESHARE_TEST_REPOS));
	EXPECT_FALSE(test_env.get_config().is_connected());
}

TEST(Login, LogFail)
{
	auto& test_env = FileshareTestEnvironment::get();
	test_env.get_config().set_full_url(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_REPOS));
	EXPECT_FALSE(test_env.get_config().is_connected());

	EXPECT_THROW({
				 test_env.get_config().connect(fileshare::Url::utf8_to_wstring("WRONG USER"), fileshare::Url::
					 utf8_to_wstring("WRONG PASSWORD"));
		}, fileshare::WrongCredentialsException);
	EXPECT_THROW({
				 test_env.get_config().connect(fileshare::Url::utf8_to_wstring("WRONG USER"), fileshare::Url::
					 utf8_to_wstring(FILESHARE_TEST_PASSWORD));
		}, fileshare::WrongCredentialsException);
	EXPECT_THROW({
				 std::stringstream str;
				 test_env.get_config().connect(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_USER), fileshare::Url::
					 utf8_to_wstring("WRONG PASSWORD"));
		}, fileshare::WrongCredentialsException);
	EXPECT_FALSE(test_env.get_config().is_connected());
}

TEST(Login, LogSucceed)
{
	auto& test_env = FileshareTestEnvironment::get();
	test_env.get_config().set_full_url(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_REPOS));
	EXPECT_FALSE(test_env.get_config().is_connected());
	EXPECT_NO_THROW({
		test_env.get_config().connect(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_USER), fileshare::Url::
			utf8_to_wstring(FILESHARE_TEST_PASSWORD));
		});
	EXPECT_TRUE(test_env.get_config().is_connected());
	test_env.reopen_config();
	EXPECT_TRUE(test_env.get_config().is_connected());
	test_env.get_config().logout();
}

TEST(Login, LogOut)
{
	auto& test_env = FileshareTestEnvironment::get();
	test_env.get_config().set_full_url(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_REPOS));
	EXPECT_FALSE(test_env.get_config().is_connected());

	EXPECT_NO_THROW({
		test_env.get_config().connect(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_USER), fileshare::Url::
			utf8_to_wstring(FILESHARE_TEST_PASSWORD));
		});
	EXPECT_TRUE(test_env.get_config().is_connected());
	test_env.reopen_config();
	EXPECT_TRUE(test_env.get_config().is_connected());
	test_env.get_config().logout();
	test_env.reopen_config();
	EXPECT_FALSE(test_env.get_config().is_connected());
}