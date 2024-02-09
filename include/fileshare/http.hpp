#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace fileshare
{
	class Http
	{
	public:
		Http() = default;
		Http(const std::string& auth_token);

		[[nodiscard]] nlohmann::json fetch_json_data(const std::string& url);
		nlohmann::json post_request(const std::string& url);
		void fetch_file(const std::string& url, std::ostream& file, std::size_t& timestamp);
		nlohmann::json upload_file_part(const std::string& url, std::istream& file, size_t uploaded_size);
		[[nodiscard]] int get_last_response() const { return last_response; }

		void set_payload(const std::string& in_payload)
		{
			payload = in_payload;
		}

		void add_header(const std::string& header) { headers.emplace_back(header); }

		class HttpError : public std::exception
		{
		public:
			HttpError(int code) : error_code(code)
			{
			}

			[[nodiscard]] virtual int code() const noexcept { return error_code; }

			[[nodiscard]] const char* what() const noexcept override;

		protected:
			int error_code;
		private:
			inline static std::string msg;
		};


		class AccessDeniedException : public HttpError
		{
		public:
			AccessDeniedException() : HttpError(403)
			{
			}

			[[nodiscard]] const char* what() const noexcept override
			{
				return "403 : Access denied !";
			}
		};

		class NotFoundException : public HttpError
		{
            const std::string message;
		public:
			NotFoundException(const std::string& context = "") : HttpError(404), message("404 : Not found :" + context)
			{
			}

			[[nodiscard]] const char* what() const noexcept override
			{
				return message.c_str();
			}
		};

	private:

		std::optional<std::string> payload;
		std::vector<std::string> headers = {"no-redirect: true"};
		long last_response = -1;

		class CurlBox final
		{
		public:
			typedef void CURL;
			CurlBox();
			CurlBox(CurlBox&&) = default;
			~CurlBox();
			CURL* operator->() const { return curl; }
			CURL* operator*() const { return curl; }
		private:
			CURL* curl;
		};
		CurlBox prepareRequest() const;
	};
}
