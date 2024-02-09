#include "fileshare/shell_utils.hpp"

#include <utility>
#include <iostream>

#if _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <cstdio>
#include <termios.h>
#include <unistd.h>
#endif

namespace fileshare
{
	ProgressBar::ProgressBar(std::wstring in_title, size_t in_total_task) :
		title(std::move(in_title)),
		total_task(in_total_task),
		current_progress(0)
	{
		ShellUtils::init_progress_bar(*this);
		ShellUtils::refresh_progress_bar(*this, L"starting...");
	}

	void ProgressBar::progress(size_t status, const std::wstring& in_message)
	{
		if (!in_message.empty())
			message = in_message;
		current_progress = status;
		ShellUtils::refresh_progress_bar(*this, message);
	}

	ProgressBar::~ProgressBar()
	{
		ShellUtils::refresh_progress_bar(
			*this, L"Finished " + std::to_wstring(current_progress) + L" / " + std::to_wstring(total_task) + L" tasks",
			true);
	}

	static size_t max_printed_size = 40;

	void ShellUtils::refresh_progress_bar(ProgressBar& pb, const std::wstring& message, bool finished)
	{
		size_t term_size = get_terminal_width();
		if (message.size() + 15 > max_printed_size)
			max_printed_size = message.size() + 15;
		if (term_size == 0)
			term_size = max_printed_size;
		const float percents = static_cast<float>(pb.current_progress) / static_cast<float>(pb.total_task);
		const size_t max_bar = static_cast<int>(static_cast<float>(term_size) * percents);
		constexpr size_t message_start = 5;
		const size_t message_end = message_start + message.length();
		std::string percent_text = finished ? "Done ! " : std::to_string(static_cast<int>(percents * 100)) + "%";
		const size_t percent_start = term_size - percent_text.length() - 3;
		const size_t percent_end = percent_start + percent_text.length();

		set_font_color(Color::Green);
		set_background_color(finished ? Color::LightGray : Color::Black);
#if _WIN32
		const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO cbsi;
		if (GetConsoleScreenBufferInfo(output, &cbsi))
		{
			COORD pos = cbsi.dwCursorPosition;
			pos.X = 0;
			SetConsoleCursorPosition(output, pos);
		}
#else
        std::cout << "\r>";
#endif
		for (size_t i = 0; i < term_size - 1; ++i)
		{
			reset_decorations();
			set_font_color(Color::Green);
			set_background_color(finished ? Color::LightGray : Color::Black);
			if (i < max_bar)
#if _WIN32
				set_background_color(Color::LightGray);
#else
                set_font_mode(Font::Reversed | Font::Bold);
#endif
			else
				set_font_mode(Font::Bold);

			if (i >= message_start && i < message_end)
			{
				std::wcout << message[i - message_start];
			}
			else if (i >= percent_start && i < percent_end)
			{
				std::wcout << percent_text[i - percent_start];
			}
			else
			{
				std::cout << " ";
			}
			fflush(stdout);
		}
		reset_decorations();
		fflush(stdout);
	}

	void ShellUtils::init_progress_bar(ProgressBar& pb)
	{
		set_font_color(Color::Cyan);
		set_background_color(Color::Black);
		set_font_mode(Font::Bold);
		const std::wstring text = L" [ " + pb.title + L" ]";

		size_t term_size = get_terminal_width();
		if (text.size() + 15 > max_printed_size)
			max_printed_size = text.size() + 15;
		if (term_size == 0)
			term_size = max_printed_size;

		for (wchar_t i : text)
			std::wcout << i;

		reset_decorations();
		set_background_color(Color::Black);
		for (int i = 0; i < term_size - text.size(); ++i)
			std::wcout << ' ';
		std::wcout << std::endl;
		//std::wcout << text << std::endl;
		reset_decorations();
		fflush(stdout);
	}


	void ShellUtils::end_progress_bar()
	{
		std::cout << std::endl << "Finished !" << std::endl;
	}

	size_t ShellUtils::get_terminal_width()
	{
#if _WIN32
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		const int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		return columns;
#else
        winsize w{};
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        return w.ws_col;
#endif
	}

#if WIN32
	static int default_console_colors = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	static int stored_color = 0;
	static int stored_bg_color = 0;
#endif

	void ShellUtils::set_font_color(const ShellUtils::Color& color)
	{
#if _WIN32
		if ((color & Color::Black) == Color::Black)
			stored_color |= 0;
		if ((color & Color::Red) == Color::Red)
			stored_color |= FOREGROUND_RED;
		if ((color & Color::Green) == Color::Green)
			stored_color |= FOREGROUND_GREEN;
		if ((color & Color::Yellow) == Color::Yellow)
			stored_color |= FOREGROUND_RED | FOREGROUND_GREEN;
		if ((color & Color::Blue) == Color::Blue)
			stored_color |= FOREGROUND_BLUE;
		if ((color & Color::Purple) == Color::Purple)
			stored_color |= FOREGROUND_RED | FOREGROUND_BLUE;
		if ((color & Color::Cyan) == Color::Cyan)
			stored_color |= FOREGROUND_GREEN | FOREGROUND_BLUE;
		if ((color & Color::LightGray) == Color::LightGray)
			stored_color |= FOREGROUND_INTENSITY;
		if ((color & Color::White) == Color::White)
			stored_color |= FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
		                        stored_bg_color | stored_color
			                        ? stored_bg_color | stored_color
			                        : default_console_colors);
#else
        std::string mask;
        if ((color & Color::Black) == Color::Black)
            mask += "30;";
        if ((color & Color::Red) == Color::Red)
            mask += "31;";
        if ((color & Color::Green) == Color::Green)
            mask += "32;";
        if ((color & Color::Yellow) == Color::Yellow)
            mask += "33;";
        if ((color & Color::Blue) == Color::Blue)
            mask += "34;";
        if ((color & Color::Purple) == Color::Purple)
            mask += "35;";
        if ((color & Color::Cyan) == Color::Cyan)
            mask += "36;";
        if ((color & Color::LightGray) == Color::LightGray)
            mask += "37;";
        if ((color & Color::White) == Color::White)
            mask += "97;";

        std::cout << ("\e[" + mask.substr(0, mask.size() - 1) + "m");
#endif
	}

	void ShellUtils::set_background_color(const ShellUtils::Color& color)
	{
#if _WIN32
		if ((color & Color::Black) == Color::Black)
			stored_bg_color |= 0;
		if ((color & Color::Red) == Color::Red)
			stored_bg_color |= BACKGROUND_RED;
		if ((color & Color::Green) == Color::Green)
			stored_bg_color |= BACKGROUND_GREEN;
		if ((color & Color::Yellow) == Color::Yellow)
			stored_bg_color |= BACKGROUND_RED | BACKGROUND_GREEN;
		if ((color & Color::Blue) == Color::Blue)
			stored_bg_color |= BACKGROUND_BLUE;
		if ((color & Color::Purple) == Color::Purple)
			stored_bg_color |= BACKGROUND_RED | BACKGROUND_BLUE;
		if ((color & Color::Cyan) == Color::Cyan)
			stored_bg_color |= BACKGROUND_GREEN | BACKGROUND_BLUE;
		if ((color & Color::LightGray) == Color::LightGray)
			stored_bg_color |= BACKGROUND_INTENSITY;
		if ((color & Color::White) == Color::White)
			stored_bg_color |= BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
		                        stored_bg_color | stored_color
			                        ? stored_bg_color | stored_color
			                        : default_console_colors);
#else
        std::string mask = "";
        if ((color & Color::Black) == Color::Black)
            mask += "40;";
        if ((color & Color::Red) == Color::Red)
            mask += "41;";
        if ((color & Color::Green) == Color::Green)
            mask += "42;";
        if ((color & Color::Yellow) == Color::Yellow)
            mask += "43;";
        if ((color & Color::Blue) == Color::Blue)
            mask += "44;";
        if ((color & Color::Purple) == Color::Purple)
            mask += "45;";
        if ((color & Color::Cyan) == Color::Cyan)
            mask += "46;";
        if ((color & Color::LightGray) == Color::LightGray)
            mask += "47;";
        if ((color & Color::White) == Color::White)
            mask += "107;";

        std::cout << ("\e[" + mask.substr(0, mask.size() - 1) + "m");
#endif
	}

	void ShellUtils::reset_decorations()
	{
#if _WIN32
		stored_bg_color = 0;
		stored_color = 0;
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
		                        stored_bg_color | stored_color
			                        ? stored_bg_color | stored_color
			                        : default_console_colors);
#else
        std::cout << "\e[0m";
#endif
	}

	void ShellUtils::set_font_mode(const ShellUtils::Font& font)
	{
#if _WIN32
#else
        std::string mask = "";
        if ((font & Font::Bold) == Font::Bold)
            mask += "1;";
        if ((font & Font::Underlined) == Font::Underlined)
            mask += "4;";
        if ((font & Font::Blinking) == Font::Blinking)
            mask += "5;";
        if ((font & Font::Reversed) == Font::Reversed)
            mask += "7;";

        std::cout << ("\e[" + mask.substr(0, mask.size() - 1) + "m");
#endif
	}

	void ShellUtils::init()
	{
#if _WIN32
		CONSOLE_SCREEN_BUFFER_INFO info;
		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
			default_console_colors = info.wAttributes;
#endif
	}

	void ShellUtils::set_password_mode(bool enable)
	{
#if _WIN32
		const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		DWORD mode = 0;
		GetConsoleMode(hStdin, &mode);
		if (enable)
			SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);
		else
			SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT);
#else
        termios tty {};
        tcgetattr(STDIN_FILENO, &tty);
		if (enable)
            tty.c_lflag &= ~ECHO;
		else
            tty.c_lflag |= ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
	}
}
