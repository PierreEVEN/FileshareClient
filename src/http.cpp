#include "http.hpp"

#include <iostream>
#include <curl/curl.h>

namespace fileshare
{
	typedef size_t(*CURL_READWRITEFUNCTION_PTR)(void*, size_t, size_t, void*);

	Http::CurlBox::CurlBox() :
		curl(curl_easy_init())
	{
		if (!curl)
			throw std::runtime_error("Failed to initialize curl !");
	}

	Http::CurlBox::~CurlBox()
	{
		curl_easy_cleanup(curl);
	}

	Http::CurlBox Http::prepareRequest() const
	{
		CurlBox curl;
		
		if (payload)
		{
			curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, *payload);
			curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE_LARGE, payload->size());
		}

		curl_slist* header_list = nullptr;
		for (const auto& header : headers)
			header_list = curl_slist_append(header_list, header.c_str());
		curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, header_list);

		return curl;
	}

	Http::Http(const std::string& auth_token)
	{
		headers.emplace_back("auth-token: " + auth_token);
	}

	nlohmann::json Http::fetch_json_data(const std::string& url)
	{
		const CurlBox curl = prepareRequest();

		std::string json_raw;
		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());

		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
		{
			static_cast<std::string*>(ctx)->append(static_cast<char*>(data), size * nmemb);
			return size * nmemb;
		}));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &json_raw);
		curl_easy_perform(*curl);
		curl_easy_getinfo(*curl, CURLINFO_RESPONSE_CODE, &last_response);

		switch (last_response)
		{
		case 404:
		case 0:
			throw NotFoundException();
		case 403:
			throw AccessDeniedException();
		case 200:
		case 201:
		case 202:
			return nlohmann::json::parse(json_raw);
		default:
			throw HttpError(last_response);
		}
	}

	void Http::fetch_file(const std::string& url, std::ostream& file)
	{
		const CurlBox curl = prepareRequest();

		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
		{
			const size_t real_size = size * nmemb;
			static_cast<std::ostream*>(ctx)->write(static_cast<char*>(data), real_size);
			return real_size;
		}));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &file);

		curl_easy_perform(*curl);
		curl_easy_getinfo(*curl, CURLINFO_RESPONSE_CODE, &last_response);
		switch (last_response)
		{
		case 404:
		case 0:
			throw NotFoundException();
		case 403:
			throw AccessDeniedException();
		case 200:
		case 201:
		case 202:
			return;
		default:
			throw HttpError(last_response);
		}
	}

	nlohmann::json Http::upload_file(const std::string& url, std::istream& file, size_t uploaded_size)
	{
		const CurlBox curl = prepareRequest();

		std::string json_raw;
		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
			{
				static_cast<std::string*>(ctx)->append(static_cast<char*>(data), size * nmemb);
				return size * nmemb;
			}));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &json_raw);

		curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE_LARGE, uploaded_size);
		curl_easy_setopt(*curl, CURLOPT_READFUNCTION, static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nitems, void* ctx) -> size_t
			{
				const size_t real_size = size * nitems;
				static_cast<std::istream*>(ctx)->read(static_cast<char*>(data), real_size);
				return real_size;
			}));
		curl_easy_setopt(*curl, CURLOPT_READDATA, &file);
		curl_easy_setopt(*curl, CURLOPT_CUSTOMREQUEST, "POST");

		size_t crash_g = file.tellg();
		curl_easy_perform(*curl);
		curl_easy_getinfo(*curl, CURLINFO_RESPONSE_CODE, &last_response);

		if (last_response == 200 || last_response == 201 || last_response == 202)
			return nlohmann::json::parse(json_raw);

		// Revert read operation
		file.seekg(crash_g, file.beg);

		switch (last_response)
		{
		case 404:
		case 0:
			throw NotFoundException();
		case 403:
			throw AccessDeniedException();
		default:
			throw HttpError(last_response);
		}
	}
}