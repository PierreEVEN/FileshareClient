#pragma once
#include <codecvt>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace fileshare
{

	class Url
	{
	public:
		Url(const std::wstring& raw_url)
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

		[[nodiscard]] const std::string& get_domain() const { return domain; }
		[[nodiscard]] const std::wstring& get_path() const { return path; }
		[[nodiscard]] bool is_https() const { return https; }
		[[nodiscard]] std::optional<std::wstring> get_option(const std::wstring& key) const
		{
			const auto ite = options.find(key);
			if (ite == options.end())
				return {};
			return ite->second;
		}
		[[nodiscard]] std::unordered_map<std::wstring, std::wstring> get_options() const { return options; }


#pragma warning(push)
#pragma warning(disable : 4996)
		// convert UTF-8 string to wstring
		static std::wstring utf8_to_wstring(std::string const& str)
		{
			std::wstring_convert<std::conditional_t<
				sizeof(wchar_t) == 4,
				std::codecvt_utf8<wchar_t>,
				std::codecvt_utf8_utf16<wchar_t>>> converter;
			return converter.from_bytes(str);
		}

		// convert wstring to UTF-8 string
		static std::string wstring_to_utf8(std::wstring const& str)
		{
			std::wstring_convert<std::conditional_t<
				sizeof(wchar_t) == 4,
				std::codecvt_utf8<wchar_t>,
				std::codecvt_utf8_utf16<wchar_t>>> converter;
			return converter.to_bytes(str);
		}

		template<typename CharT = wchar_t>
		static std::string encode_string(const std::basic_string<CharT>& string)
		{
			//RFC 3986 section 2.3 Unreserved Characters (January 2005)
			static const std::wstring unreserved = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

			// Handle wstring case
			std::string normalized_string;
			if constexpr (std::is_same_v<wchar_t, CharT>) {
				normalized_string = wstring_to_utf8(string);
			}
			else 
				normalized_string = string;

			std::string encoded;
			encoded.reserve(normalized_string.length());
			for (const char i : normalized_string)
			{
				if (unreserved.find_first_of(static_cast<unsigned char>(i)) != std::basic_string<CharT>::npos)
					encoded.push_back(i);
				else
				{
					encoded.append("%");
					char buf[3];
#if _WIN32
					sprintf_s(buf, "%.2X", static_cast<unsigned char>(i));
#else
					snprintf(buf, 3, "%.2X", i); //syntax using snprintf For Linux
#endif
					encoded.append(buf);
				}
			}
			return encoded;
		}

		template<typename CharT = wchar_t>
		static std::basic_string<CharT> decode_string(const std::string& SRC) {
			std::string ret;
			for (int i = 0; i < SRC.length(); i++) {
				if (SRC[i] == '%') {
					int decoded;
#if _WIN32
					sscanf_s(SRC.substr(i + 1, 2).c_str(), "%x", &decoded);
#else
                    char* end_ptr;
                    const auto substring = SRC.substr(i + 1, 2);
                    decoded = static_cast<CharT>(strtoul(substring.c_str(), &end_ptr, 10));
                    if (end_ptr == substring.c_str())
                        throw std::runtime_error("Failed to decode string : " + substring);
#endif
					ret.push_back(static_cast<char>(decoded));
					i += 2;
				}
				else if (SRC[i] == '+') {
					ret.push_back(' ');
				}
				else {
					ret.push_back(SRC[i]);
				}
			}

			// Handle wstring case
			if constexpr (std::is_same_v<wchar_t, CharT>)
				return utf8_to_wstring(ret);
			else
				return ret;
		}
#pragma warning(pop)
		
	private:
		bool https = false;
		std::string domain;
		std::wstring path;
		std::unordered_map<std::wstring, std::wstring> options;
	};

}
