#include "fileshare/http.hpp"
#include <curl/curl.h>
#include <format>
#include <iostream>

namespace fileshare
{
	typedef size_t (*CURL_READWRITEFUNCTION_PTR)(void*, size_t, size_t, void*);

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

		//curl_easy_setopt(*curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L);

		if (payload)
		{
			curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, payload->c_str());
			curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE_LARGE, payload->size());
		}

		curl_slist* header_list = nullptr;
		for (const auto& header : headers)
			header_list = curl_slist_append(header_list, header.c_str());
		if (payload) {
			header_list = curl_slist_append(header_list, "Content-Type: application/json");
			header_list = curl_slist_append(header_list, "Accept: application/json");
		}
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

		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION,
		                 static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
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

	nlohmann::json Http::post_request(const std::string& url)
	{
		const CurlBox curl = prepareRequest();
		
		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());

		curl_easy_setopt(*curl, CURLOPT_POST, 1L);
		curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE, 0);
		std::string json_raw;
		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION,
			static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
				{
					static_cast<std::string*>(ctx)->append(static_cast<char*>(data), size * nmemb);
					return size * nmemb;
				}));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &json_raw);

		curl_easy_perform(*curl);
		curl_easy_getinfo(*curl, CURLINFO_RESPONSE_CODE, &last_response);

		switch (last_response)
		{
            case 0:
                throw std::runtime_error("Upload error : 0");
		case 404:
			throw NotFoundException(url);
		case 403:
			throw AccessDeniedException();
		case 200:
		case 201:
		case 202:
			return json_raw;
		default:
			throw HttpError(last_response);
		}
	}

	void Http::fetch_file(const std::string& url, std::ostream& file)
	{
		const CurlBox curl = prepareRequest();

		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION,
		                 static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
		                 {
			                 const size_t real_size = size * nmemb;
			                 static_cast<std::ostream*>(ctx)->write(static_cast<char*>(data), real_size);
			                 return real_size;
		                 }));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &file);
        std::string received_headers;
        std::cout << "A" << std::endl;
        curl_easy_setopt(*curl, CURLOPT_HEADERFUNCTION, [] (char* buffer, size_t size, size_t nitems, void* userdata) -> size_t
        {
            std::cout << "B" << std::endl;
            auto* received_headers = (std::string*) userdata;
            received_headers->append(buffer, nitems * size);
            return nitems * size;
        });
        curl_easy_setopt(*curl, CURLOPT_HEADERDATA, nullptr);

        std::cout << "C" << std::endl;
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
	
	nlohmann::json Http::upload_file_part(const std::string& url, std::istream& file, size_t uploaded_size)
    {
		size_t crash_g = file.tellg();
		headers.emplace_back("Content-Type:application/octet-stream");
		const CurlBox curl = prepareRequest();

		std::string json_raw;
		curl_easy_setopt(*curl, CURLOPT_URL, url.c_str());

		curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION,
		                 static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nmemb, void* ctx)
		                 {
			                 static_cast<std::string*>(ctx)->append(static_cast<char*>(data), size * nmemb);
			                 return size * nmemb;
		                 }));
		curl_easy_setopt(*curl, CURLOPT_WRITEDATA, &json_raw);


        std::vector<char> data(uploaded_size);
        file.read(data.data(), data.size());

		curl_easy_setopt(*curl, CURLOPT_POSTFIELDS, data.data());
		curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE_LARGE, data.size());

        /* PERFORM */
		const CURLcode result = curl_easy_perform(*curl);
        if (result != CURLE_OK)
            throw std::runtime_error("curl_easy_perform() failed: " + std::to_string(result) + "|" + std::string(curl_easy_strerror(result)));
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

	const char* Http::HttpError::what() const noexcept
	{
		msg = std::format("Http error : {}", error_code);
		return msg.c_str();
	}
}
