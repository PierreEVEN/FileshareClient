#include "application.hpp"

#if !_WIN32
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

fileshare::AppConfig::AppConfig(std::optional<std::filesystem::path> in_config_path)
{
	if (in_config_path)
		config_path = *in_config_path;
	else {

#if _WIN32
		char* home_path_chr = nullptr;
		size_t len;
		if (_dupenv_s(&home_path_chr, &len, "USERPROFILE") || len == 0 || !std::filesystem::exists(home_path_chr))
		{
			if (home_path_chr)
				free(home_path_chr);
			if (_dupenv_s(&home_path_chr, &len, "HOMEPATH") || len == 0 || !std::filesystem::exists(home_path_chr))
			{
				if (home_path_chr)
					free(home_path_chr);
				throw std::runtime_error("Cannot find default user directory");
			}
		}
		config_path = std::filesystem::path(home_path_chr) / ".fileshare.config.json";
		free(home_path_chr);
#else
		const char* home_path_chr;
		if ((home_path_chr = getenv("HOME")) == nullptr) {
			home_path_chr = getpwuid(getuid())->pw_dir;
		}
		config_path = std::filesystem::path(home_path_chr) / ".fileshare.config.json";
#endif
	}

	if (!load_config())
		std::cout << "Cannot find existing fileshare configuration, creating a new one" << std::endl;

}
