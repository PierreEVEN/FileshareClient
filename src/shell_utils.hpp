#pragma once
#include <cstdlib>
#include <string>

namespace fileshare {

    class ProgressBar {
        friend class ShellUtils;
    public:
        ProgressBar(std::wstring title, size_t total_task);
        ~ProgressBar();
        void progress(size_t status, const std::wstring& message);
    private:
        std::wstring title;
        std::wstring message;
        size_t total_task;
        size_t current_progress;
    };

    class ShellUtils {
        friend class ProgressBar;
    public:

        enum class Color {
            Black = 1,
            Red = 1 << 1,
            Green = 1 << 2,
            Yellow = 1 << 3,
            Blue = 1 << 4,
            Purple = 1 << 5,
            Cyan = 1 << 6,
            LightGray = 1 << 7,
            White = 1 << 8,
        };

        enum class Font {
            Bold = 1,
            Underlined = 1 << 1,
            Blinking = 1 << 2,
            Reversed = 1 << 3,
        };
        static size_t get_terminal_width();

        static void reset_decorations();
        static void set_font_color(const Color& color);
        static void set_background_color(const Color& color);
        static void set_font_mode(const Font& font);
        static void init();
        static void set_password_mode(bool enable);
    private:

        static void init_progress_bar(ProgressBar& pb);
        static void refresh_progress_bar(ProgressBar& pb, const std::wstring& message, bool finished = false);
        static void end_progress_bar();
    };

    constexpr inline ShellUtils::Color operator~ (ShellUtils::Color a) { return static_cast<ShellUtils::Color>( ~static_cast<std::underlying_type<ShellUtils::Color>::type>(a) ); }
    constexpr inline ShellUtils::Color operator| (ShellUtils::Color a, ShellUtils::Color b) { return static_cast<ShellUtils::Color>( static_cast<std::underlying_type<ShellUtils::Color>::type>(a) | static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }
    constexpr inline ShellUtils::Color operator& (ShellUtils::Color a, ShellUtils::Color b) { return static_cast<ShellUtils::Color>( static_cast<std::underlying_type<ShellUtils::Color>::type>(a) & static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }
    constexpr inline ShellUtils::Color operator^ (ShellUtils::Color a, ShellUtils::Color b) { return static_cast<ShellUtils::Color>( static_cast<std::underlying_type<ShellUtils::Color>::type>(a) ^ static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }
    inline ShellUtils::Color& operator|= (ShellUtils::Color& a, ShellUtils::Color b) { return reinterpret_cast<ShellUtils::Color&>( reinterpret_cast<std::underlying_type<ShellUtils::Color>::type&>(a) |= static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }
    inline ShellUtils::Color& operator&= (ShellUtils::Color& a, ShellUtils::Color b) { return reinterpret_cast<ShellUtils::Color&>( reinterpret_cast<std::underlying_type<ShellUtils::Color>::type&>(a) &= static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }
    inline ShellUtils::Color& operator^= (ShellUtils::Color& a, ShellUtils::Color b) { return reinterpret_cast<ShellUtils::Color&>( reinterpret_cast<std::underlying_type<ShellUtils::Color>::type&>(a) ^= static_cast<std::underlying_type<ShellUtils::Color>::type>(b) ); }

    constexpr inline ShellUtils::Font operator~ (ShellUtils::Font a) { return static_cast<ShellUtils::Font>( ~static_cast<std::underlying_type<ShellUtils::Font>::type>(a) ); }
    constexpr inline ShellUtils::Font operator| (ShellUtils::Font a, ShellUtils::Font b) { return static_cast<ShellUtils::Font>( static_cast<std::underlying_type<ShellUtils::Font>::type>(a) | static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
    constexpr inline ShellUtils::Font operator& (ShellUtils::Font a, ShellUtils::Font b) { return static_cast<ShellUtils::Font>( static_cast<std::underlying_type<ShellUtils::Font>::type>(a) & static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
    constexpr inline ShellUtils::Font operator^ (ShellUtils::Font a, ShellUtils::Font b) { return static_cast<ShellUtils::Font>( static_cast<std::underlying_type<ShellUtils::Font>::type>(a) ^ static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
    inline ShellUtils::Font& operator|= (ShellUtils::Font& a, ShellUtils::Font b) { return reinterpret_cast<ShellUtils::Font&>( reinterpret_cast<std::underlying_type<ShellUtils::Font>::type&>(a) |= static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
    inline ShellUtils::Font& operator&= (ShellUtils::Font& a, ShellUtils::Font b) { return reinterpret_cast<ShellUtils::Font&>( reinterpret_cast<std::underlying_type<ShellUtils::Font>::type&>(a) &= static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
    inline ShellUtils::Font& operator^= (ShellUtils::Font& a, ShellUtils::Font b) { return reinterpret_cast<ShellUtils::Font&>( reinterpret_cast<std::underlying_type<ShellUtils::Font>::type&>(a) ^= static_cast<std::underlying_type<ShellUtils::Font>::type>(b) ); }
}
