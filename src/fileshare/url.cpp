#include "fileshare/url.hpp"

#include <locale>
#include <stdexcept>

#if _WIN32
#include <windows.h>
#else
#include <codecvt>
#endif

namespace fileshare
{
	Url::Url(const std::wstring& raw_url)
	{
		auto url = raw_url;
		if (url.substr(0, 8) == L"https://")
		{
			url = url.substr(8);
			https = true;
		}
		else if (url.substr(0, 7) == L"http://")
		{
			url = url.substr(7);
			https = false;
		}
		const auto domain_end = url.find('/');
		const auto path_end = url.find('?');
		if (domain_end == std::wstring::npos)
		{
			if (path_end == std::wstring::npos)
			{
				domain = wstring_to_utf8(url);
				path.clear();
			}
			else
			{
				domain = wstring_to_utf8(url.substr(0, path_end));
				path.clear();
			}
		}
		else
		{
			if (path_end == std::wstring::npos)
			{
				domain = wstring_to_utf8(url.substr(0, domain_end));
				path = url.substr(domain_end + 1);
			}
			else
			{
				domain = wstring_to_utf8(url.substr(0, domain_end));
				path = url.substr(domain_end + 1, path_end - domain_end - 1);
			}
		}

		options = {};
		if (path_end != std::wstring::npos)
		{
			url = url.substr(path_end + 1);
			size_t opt_delim;
			do
			{
				opt_delim = url.find('&');
				const auto left = url.substr(0, opt_delim);
				const auto opt_equals = left.find('=');
				if (opt_equals == std::wstring::npos)
					throw std::runtime_error("Invalid URL");
				options[left.substr(0, opt_equals)] = left.substr(opt_equals + 1);
				url = url.substr(opt_delim + 1);
			} while (opt_delim != std::wstring::npos);
		}
	}

	std::optional<std::wstring> Url::get_option(const std::wstring& key) const
	{
		const auto ite = options.find(key);
		if (ite == options.end())
			return {};
		return ite->second;
	}

#if _WIN32
	std::wstring Url::utf8_to_wstring(std::string const& str)
	{
		if (str.empty())
			return {};

		const int required = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
		if (0 == required)
			return {};

		std::wstring str2;
		str2.resize(required);
		const int converted = ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), str2.data(), static_cast<int>(str2.capacity()));
		if (0 == converted)
			return {};

		return str2;
	}

	std::string Url::wstring_to_utf8(std::wstring const& str)
	{
		if (str.empty())
			return {};

		const int required = ::WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
		if (0 == required)
			return {};

		std::string str2;
		str2.resize(required);
		const int converted = ::WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), str2.data(), static_cast<int>(str2.capacity()), NULL, NULL);
		if (0 == converted)
			return {};

		return str2;
	}
#else
	std::wstring Url::utf8_to_wstring(std::string const& str)
	{
		std::wstring_convert<std::conditional_t<
			sizeof(wchar_t) == 4,
			std::codecvt_utf8<wchar_t>,
			std::codecvt_utf8_utf16<wchar_t>>> converter;
		return converter.from_bytes(str);
	}

	std::string Url::wstring_to_utf8(std::wstring const& str)
	{
		std::wstring_convert<std::conditional_t<
			sizeof(wchar_t) == 4,
			std::codecvt_utf8<wchar_t>,
			std::codecvt_utf8_utf16<wchar_t>>> converter;
		return converter.to_bytes(str);
	}
#endif
}
