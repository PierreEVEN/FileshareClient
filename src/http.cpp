#include "http.hpp"

#include <iostream>
#include <curl/curl.h>

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

	static int recv_any(CURL* curl)
	{
		size_t rlen;
		const struct curl_ws_frame* meta;
		char buffer[256];
		CURLcode result = curl_ws_recv(curl, buffer, sizeof(buffer), &rlen, &meta);
		if (result)
			throw std::runtime_error("recv_init() failed: " + std::string(curl_easy_strerror(result)));

		return 0;
	}

	void Http::upload_file_ws(const std::string& url, std::istream& file)
	{

		CurlBox curl;
		curl_easy_setopt(*curl, CURLOPT_VERBOSE, 1L);

		// Send headers with auth tokens...
		curl_slist* header_list = nullptr;
		for (const auto& header : headers)
			header_list = curl_slist_append(header_list, header.c_str());
		curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, header_list);

		// Set URL
		const std::string ws_url = "ws://" + url;
		curl_easy_setopt(*curl, CURLOPT_URL, ws_url.c_str());

		curl_easy_setopt(*curl, CURLOPT_CONNECT_ONLY, 2L);

		// Connect
		auto res = curl_easy_perform(*curl);
		if (res != CURLE_OK) {
			curl_easy_getinfo(*curl, CURLINFO_RESPONSE_CODE, &last_response);
			throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)) + " : code=" + std::to_string(last_response));
		}
		std::cout << "PERFORM PASSED" << std::endl;


		recv_any(*curl);


		size_t sent;
		CURLcode result = curl_ws_send(*curl, "coucou toto", strlen("coucou toto"), &sent, 0, CURLWS_PING);
		if (result != CURLE_OK)
			throw std::runtime_error("curl_ws_send FIRST() failed: " + std::to_string(result) + "|" + std::string(curl_easy_strerror(result)));

		// Receive data through websocket
		char response[1024];  // Adjust the buffer size as needed
		size_t rlen;
		const curl_ws_frame* meta;
		const auto ws_recv_res = curl_ws_recv(*curl, response, sizeof(response), &rlen, &meta);
		if (ws_recv_res != CURLE_OK) {
			throw std::runtime_error("Failed to get websocket data: " + std::string(curl_easy_strerror(ws_recv_res)));
		}

		std::cout << "WebSocket Handshake Successful" << std::endl;


		// Send data through websocket

		result = curl_ws_send(*curl, "coucou ca va ?", strlen("coucou ca va ?"), &sent, 0, CURLWS_TEXT);
		if (result != CURLE_OK)
			throw std::runtime_error("curl_ws_send() failed: " + std::to_string(result) + "|" + std::string(curl_easy_strerror(result)));
		
		(void)curl_ws_send(*curl, "", 0, &sent, 0, CURLWS_CLOSE);
	}

	nlohmann::json Http::upload_file(const std::string& url, std::istream& file, size_t uploaded_size)
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

		curl_easy_setopt(*curl, CURLOPT_POSTFIELDSIZE_LARGE, uploaded_size);
		curl_easy_setopt(*curl, CURLOPT_READFUNCTION,
		                 static_cast<CURL_READWRITEFUNCTION_PTR>([](void* data, size_t size, size_t nitems,
		                                                            void* ctx) -> size_t
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
