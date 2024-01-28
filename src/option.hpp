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
		using Callback = std::function<void(const std::vector<std::string>&)>;

		Command(std::string in_name, std::optional<Callback> in_callback = {}, std::vector<std::string> in_args = {},
		        std::string in_hint = "") :
			name(std::move(in_name)),
			hint(std::move(in_hint)),
			args(std::move(in_args))
		{
			callback = in_callback ? *in_callback : [&](auto) {throw std::runtime_error("The command option '" + name + "' is not valid"); };
		}

		void add_sub_command(Command sub_cmd)
		{
			sub_commands.emplace_back(std::move(sub_cmd));
		}

		[[nodiscard]] const Command* find_command(const std::string& command) const
		{
			for (const auto& cmd : sub_commands)
				if (cmd.name == command)
					return &cmd;
			return nullptr;
		}

	private:
		std::vector<Command> sub_commands;
		std::string name;
		std::string hint;
		std::vector<std::string> args;
		Callback callback;
	};

	class Option
	{
	public:
		Option() : root(Command{"fileshare"})
		{
		}

		Option(Command in_root) : root(std::move(in_root))
		{
		}

		static Option& get();

		static void parse(int argc, char** argv, Command root);

		void print_help(const std::string& option = "") const;

	private:

		void print_command(int rec_level, const Command& base) const;
		bool parse_command(std::vector<std::string>::iterator& ite, const std::vector<std::string>::iterator& end, const Command& context);
		Command root;
	};
}
