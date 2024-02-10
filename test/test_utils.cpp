#include "test_utils.hpp"

#include <filesystem>
#include <fstream>

#include "test_config.hpp"
#include "fileshare/diff.hpp"
#include "fileshare/repository.hpp"
#include "fileshare/url.hpp"

static FileshareTestEnvironment* __SINGLETON__ = nullptr;
static std::filesystem::path __RUN_ENV__;

FileshareTestEnvironment::FileshareTestEnvironment(int argc, char** argv)
{
	srand(45687895);

	assert(!__SINGLETON__);
	__SINGLETON__ = this;

	__RUN_ENV__ = std::filesystem::absolute(argv[0]).parent_path();
}

FileshareTestEnvironment::~FileshareTestEnvironment()
{
	__SINGLETON__ = nullptr;
}

void FileshareTestEnvironment::SetUp()
{
	Environment::SetUp();

	if (exists(get_test_data_dir()))
		remove_all(get_test_data_dir());

	create_directories(get_test_data_dir());
	current_path(get_test_data_dir());

	reopen_config();
}

void FileshareTestEnvironment::TearDown()
{
	current_path(__RUN_ENV__);
	cfg = nullptr;
	
	if (exists(get_test_data_dir()))
		remove_all(get_test_data_dir());

	Environment::TearDown();
}

std::filesystem::path FileshareTestEnvironment::get_test_data_dir() const
{
	return absolute(__RUN_ENV__ / "test-data-pool");
}

FileshareTestEnvironment& FileshareTestEnvironment::get()
{
	assert(__SINGLETON__);
	return *__SINGLETON__;
}

FileshareTestEnvironment& FileshareTestEnvironment::get_connected()
{
	auto& inst = get();

	if (!inst.get_config().is_connected())
	{
		inst.get_config().set_full_url(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_REPOS));
		inst.get_config().connect(fileshare::Url::utf8_to_wstring(FILESHARE_TEST_USER),
		                          fileshare::Url::utf8_to_wstring(FILESHARE_TEST_PASSWORD));
	}
	return inst;
}

void FileshareTestEnvironment::reset_environment()
{
	const bool connected = get_config().is_connected();
	TearDown();
	SetUp();
	if (connected)
		get_connected();
}

fileshare::Directory FileshareTestEnvironment::fill_random_tree(size_t depth,
                                                                size_t objects_per_dir) const
{
	const auto path = get_test_data_dir();
	make_random_tree_internal(path, depth, objects_per_dir);
	return fileshare::Directory::from_path(path, nullptr);
}

void FileshareTestEnvironment::reopen_config()
{
	cfg = nullptr;
	cfg = std::make_unique<fileshare::RepositoryConfig>(get_test_data_dir());
	current_path(get_test_data_dir());
}

void FileshareTestEnvironment::make_random_tree_internal(const std::filesystem::path& target, size_t depth,
                                                         size_t objects_per_dir) const
{
	for (size_t i = 0; i < objects_per_dir; ++i)
	{
		if ((i == 0 || rand() % 3 == 0) && depth != 0)
		{
			const auto new_path = target / (L"Directory_" + std::to_wstring(depth) + L"_" + std::to_wstring(i));
			create_directory(new_path);
			make_random_tree_internal(new_path, depth - 1, objects_per_dir);
		}
		else
		{
			std::ofstream fs;
			fs.open(target / (L"File_" + std::to_wstring(depth) + L"_" + std::to_wstring(i)), std::ios_base::out);
			fs.close();
		}
	}
}

std::vector<char> FileshareTestEnvironment::edit_file(const std::filesystem::path& file,size_t data_inside) {
    if (!exists(file.parent_path()))
        create_directories(file.parent_path());
    if (!exists(file.parent_path()))
        throw std::runtime_error("Failed to create parent dir");

    std::ofstream f(file, std::ios_base::binary | std::ios_base::out);
    if (!f.is_open())
        throw std::runtime_error("Failed to open file");
    std::vector<char> rand_data(data_inside);
    for (auto& data : rand_data)
        data = static_cast<char>(rand());
    f.write(rand_data.data(), data_inside);
    f.close();
    return rand_data;
}

fileshare::DiffResult FileshareTestEnvironment::get_current_diff() const
{
    return {fileshare::Directory::from_path(get_test_data_dir(), nullptr), get_config().get_saved_state(), get_config().fetch_repos_status()};
}

std::vector<char> FileshareTestEnvironment::get_file_data(const std::filesystem::path &file) {

    if (!exists(file))
        throw std::runtime_error("File does not exists");
    std::ifstream f(file, std::ios_base::binary | std::ios_base::in);
    std::vector<char> buffer(std::istreambuf_iterator<char>(f), {});
    return buffer;
}