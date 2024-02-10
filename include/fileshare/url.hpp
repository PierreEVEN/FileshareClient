#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace fileshare
{

	class Url
	{
	public:
		Url(const std::wstring& raw_url);
		[[nodiscard]] const std::string& get_domain() const { return domain; }
		[[nodiscard]] const std::wstring& get_path() const { return path; }
		[[nodiscard]] bool is_https() const { return https; }
		[[nodiscard]] std::optional<std::wstring> get_option(const std::wstring& key) const;
		[[nodiscard]] std::unordered_map<std::wstring, std::wstring> get_options() const { return options; }
		[[nodiscard]] static std::wstring utf8_to_wstring(std::string const& str);
		[[nodiscard]] static std::string wstring_to_utf8(std::wstring const& str);

		template<typename CharT = wchar_t>
		[[nodiscard]] static std::string encode_string(const std::basic_string<CharT>& string)
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
					snprintf(buf, 3, "%.2X", static_cast<unsigned char>(i)); //syntax using snprintf For Linux
#endif
					encoded.append(buf);
				}
			}
			return encoded;
		}

		template<typename CharT = wchar_t>
		[[nodiscard]] static std::basic_string<CharT> decode_string(const std::string& src) {
			std::string ret;
			ret.reserve(src.size());
			for (int i = 0; i < src.length(); i++) {
				if (src[i] == '%') {
					int decoded;
#if _WIN32
					sscanf_s(src.substr(i + 1, 2).c_str(), "%x", &decoded);
#else
                    char* end_ptr;
                    const auto substring = src.substr(i + 1, 2);
                    decoded = static_cast<CharT>(strtoull(substring.c_str(), &end_ptr, 16));
                    if (end_ptr == substring.c_str())
                        throw std::runtime_error("Failed to decode string : " + substring);
#endif
					ret.push_back(static_cast<char>(decoded));
					i += 2;
				}
				else {
					ret.push_back(src[i]);
				}
			}

			// Handle wstring case
			if constexpr (std::is_same_v<wchar_t, CharT>)
				return utf8_to_wstring(ret);
			else
				return ret;
		}
		
	private:
		bool https = false;
		std::string domain;
		std::wstring path;
		std::unordered_map<std::wstring, std::wstring> options;
	};

}
