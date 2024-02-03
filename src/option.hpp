#pragma once
#include <functional>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace fileshare
{
	class Command
	{
		friend class Option;
	public:
		static void default_callback();
		using Callback = std::function<void(const std::vector<std::wstring>&)>;

		Command(std::wstring in_name, std::optional<Callback> in_callback = {}, std::vector<std::wstring> in_args = {},
			std::wstring in_hint = L"");

		void add_sub_command(Command sub_cmd)
		{
			sub_commands.emplace_back(std::move(sub_cmd));
		}

		[[nodiscard]] const Command* find_command(const std::wstring& command) const
		{
			for (const auto& cmd : sub_commands)
				if (cmd.name == command)
					return &cmd;
			return nullptr;
		}

	private:
		std::vector<Command> sub_commands;
		std::wstring name;
		std::wstring hint;
		std::vector<std::wstring> args;
		Callback callback;
	};

	class Option
	{
	public:
		Option() : root(Command{L"fileshare"})
		{
		}

		Option(Command in_root) : root(std::move(in_root))
		{
		}

		static Option& get();

		static void parse(int argc, char** argv, Command root);

		void print_help(const std::wstring& option = L"") const;

	private:

		void print_command(int rec_level, const Command& base) const;
		bool parse_command(std::vector<std::wstring>::iterator& ite, const std::vector<std::wstring>::iterator& end, const Command& context);
		Command root;
	};
}
