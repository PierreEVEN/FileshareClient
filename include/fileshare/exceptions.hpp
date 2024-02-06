#pragma once
#include <exception>
#include <string>

namespace fileshare {
	class WrongCredentialsException : std::exception
	{
		std::string context;
	public:
		WrongCredentialsException(const std::string& in_action) : context(
			"'" + in_action + "' failed : wrong credentials !")
		{
		}

		[[nodiscard]] char const* what() const override { return context.c_str(); }
	};

	class BadRequest : std::exception
	{
		std::string context;
	public:
		BadRequest(const std::string& in_context) : context(
			"Bad request : '" + in_context + "'")
		{
		}

		[[nodiscard]] char const* what() const override { return context.c_str(); }
	};
}