#include "app/option.hpp"

#include <iostream>
#include <locale>
#include <stdexcept>
#include <utility>
#include "fileshare/url.hpp"

namespace fileshare
{
	static Option option_singleton;

	void Command::default_callback()
	{
		option_singleton.print_help();
	}

	Command::Command(std::wstring in_name, std::optional<Callback> in_callback, std::vector<std::wstring> in_args,
		std::wstring in_hint) :
		name(std::move(in_name)),
		hint(std::move(in_hint)),
		args(std::move(in_args))
	{
		callback = in_callback ? *in_callback : [&](auto) {throw std::runtime_error("The command option '" + Url::wstring_to_utf8(name) + "' is not valid"); };
	}

	Option& Option::get()
	{
		return option_singleton;
	}


	void Option::print_command(int rec_level, const Command& base) const
	{
		for (int i = 0; i < rec_level; ++i)
			std::wcout << "--";
		std::wcout << L" " << base.name;

		for (const auto& arg : base.args)
			std::wcout << " <" << arg << ">";
		std::wcout << std::endl;

		if (!base.hint.empty())
		{
			for (int i = 0; i < rec_level; ++i)
				std::wcout << "  ";
			std::wcout << "    | " << base.hint << std::endl;
		}

		for (const auto& command : base.sub_commands)
			print_command(rec_level + 1, command);
	}

	void Option::print_help(const std::wstring& option) const
	{
		std::wcout << root.name << " command line tool help" << std::endl;
		for (const auto& command : root.sub_commands)
		{
			print_command(1, command);
		}
	}

	void Option::parse(int argc, char** argv, Command in_root)
	{
		auto& root = option_singleton.root;
		if (argc == 1 && root.args.empty())
		{
			root.callback({});
			return;
		}

		root = std::move(in_root);
		root.add_sub_command(Command{
			L"help", [](auto) { option_singleton.print_help(); }, {}, L"Get help about available commands"
		});

		std::vector<std::wstring> fields(argc);
		for (int i = 0; i < argc; ++i)
			fields[i] = Url::utf8_to_wstring(argv[i]);


		for (auto ite = ++fields.begin(); ite != fields.end();)
		{
			if (!option_singleton.parse_command(ite, fields.end(), root))
				return;
		}
	}

	bool Option::parse_command(std::vector<std::wstring>::iterator& ite, const std::vector<std::wstring>::iterator& end,
	                           const Command& context)
	{
		if (const auto& command = context.find_command(*ite))
		{
			++ite;
			// If no args are provided
			if (ite == end)
			{
				if (command->args.empty())
				{
					// Cool, we don't need them !
					command->callback({});
					return true;
				}
				else
				{
					std::cerr << "missing arguments. Expected";
					for (const auto& arg : command->args)
						std::wcerr << " <" << arg << ">";
					std::cerr << std::endl;
					return false;
				}
			}

			// if this is a leaf command, call it now
			if (command->sub_commands.empty())
			{
				// Read required arguments
				std::vector<std::wstring> arguments;
				for (const auto& arg : command->args)
				{
					if (ite == end)
					{
						// We wan't to read the next arguments but there is no more arguments provided
						std::cerr << "Missing arguments. Expected";
						for (size_t i = arguments.size(); i < command->args.size(); ++i)
							std::wcerr << " <" << arg << ">";
						std::cerr << std::endl;
						return false;
					}
					arguments.emplace_back(*ite);

					++ite;
				}

				command->callback(arguments);
				return ite != end;
			}
			else
			{
				if (ite == end)
				{
					std::wcerr << "The command '" << command->name << "' requires arguments" << std::endl;
					return false;
				}
				return parse_command(ite, end, *command);
			}
		}
		else
		{
			std::wcerr << "'" << *ite << "' is not a valid command" << std::endl;
			print_help();
			return false;
		}
	}
}
