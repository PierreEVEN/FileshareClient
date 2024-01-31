#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace fileshare
{

	class Url
	{
	public:
		Url(const std::string& raw_url)
		{
			auto url = raw_url;
			if (url.substr(0, 8) == "https://")
			{
				url = url.substr(8);
				https = true;
			}
			else if (url.substr(0, 7) == "http://")
			{
				url = url.substr(7);
				https = false;
			}
			const auto domain_end = url.find('/');
			const auto path_end = url.find('?');
			if (domain_end == std::string::npos)
			{
				if (path_end == std::string::npos)
				{
					domain = url;
					path = "";
				}
				else
				{
					domain = url.substr(0, path_end);
					path = "";
				}
			}
			else
			{
				if (path_end == std::string::npos)
				{
					domain = url.substr(0, domain_end);
					path = url.substr(domain_end + 1);
				}
				else
				{
					domain = url.substr(0, domain_end);
					path = url.substr(domain_end + 1, path_end - domain_end - 1);
				}
			}

			options = {};
			if (path_end != std::string::npos)
			{
				url = url.substr(path_end + 1);
				size_t opt_delim;
				do
				{
					opt_delim = url.find('&');
					const auto left = url.substr(0, opt_delim);
					const auto opt_equals = left.find('=');
					if (opt_equals == std::string::npos)
						throw std::runtime_error("Invalid URL");
					options[left.substr(0, opt_equals)] = left.substr(opt_equals + 1);
					url = url.substr(opt_delim + 1);
				} while (opt_delim != std::string::npos);
			}
		}

		[[nodiscard]] const std::string& get_domain() const { return domain; }
		[[nodiscard]] const std::string& get_path() const { return path; }
		[[nodiscard]] bool is_https() const { return https; }
		[[nodiscard]] std::optional<std::string> get_option(const std::string& key) const
		{
			const auto ite = options.find(key);
			if (ite == options.end())
				return {};
			return ite->second;
		}
		[[nodiscard]] std::unordered_map<std::string, std::string> get_options() const { return options; }

		static std::string encode_url(const std::string& s)
		{
			//RFC 3986 section 2.3 Unreserved Characters (January 2005)
			const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

			std::string escaped;
			for (const char i : s)
			{
				if (unreserved.find_first_of(i) != std::string::npos)
				{
					escaped.push_back(i);
				}
				else
				{
					escaped.append("%");
					char buf[3];
#if _WIN32
					sprintf_s(buf, "%.2X", i);
#else
                    snprintf(buf, 3, "%.2X"); //syntax using snprintf For Linux
#endif
					escaped.append(buf);
				}
			}
			return escaped;
		}


	private:
		bool https = false;
		std::string domain;
		std::string path;
		std::unordered_map<std::string, std::string> options;
	};

}
