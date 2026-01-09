#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <memory>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <type_traits>

// --------------------------------
//  Error definitions
// --------------------------------

namespace nite
{
    /// Converts a wide character to mutiple byte string
    std::string wc_to_str(const wchar_t wc);
    /// Converts a mutiple byte string to wide character
    wchar_t str_to_wc(const std::string &str);
    /// Converts a single byte (char) to wide character
    wchar_t c_to_wc(const char c);
    /// Converts a multiple byte string to wide string
    std::wstring str_to_wstr(const std::string &str);
    /// Converts a wide string to multiple byte string
    std::string wstr_to_str(const std::wstring &str);

    class Result {
        bool success;
        std::string message;

        Result(const std::string &message) : success(false), message(message) {}

        Result(bool success) : success(success), message() {}

      public:
        static const Result Ok;

        template<typename... Args>
        static Result Error(std::format_string<Args...> msg, Args... args) {
            return Result(std::format<Args...>(msg, std::forward<Args>(args)...));
        }

        static Result Error() {
            return Result(false);
        }

        Result() = delete;
        Result(const Result &) = default;
        Result(Result &&) = default;
        Result &operator=(const Result &) = default;
        Result &operator=(Result &&) = default;
        ~Result() = default;

        operator bool() const {
            return success;
        }

        std::string what() const {
            return message;
        }
    };

    inline const Result Result::Ok = Result(true);
}    // namespace nite

// --------------------------------
//  Style definitions
// --------------------------------

namespace nite
{
    /**
     * Represent a color in RGB format
     */
    struct Color {
        uint8_t r = 0, g = 0, b = 0;

        constexpr static Color from_rgb(const uint8_t value) {
            return Color{.r = value, .g = value, .b = value};
        }

        constexpr static Color from_rgb(const uint8_t r, const uint8_t g, const uint8_t b) {
            return Color{.r = r, .g = g, .b = b};
        }

        constexpr static Color from_hex(const uint32_t hex) {
            // hex is encoded as 0x00rrggbb
            return Color{
                    .r = static_cast<uint8_t>((hex >> 16) & 0xFF),
                    .g = static_cast<uint8_t>((hex >> 8) & 0xFF),
                    .b = static_cast<uint8_t>((hex >> 0) & 0xFF)
            };
        }

        constexpr uint32_t get_hex() const {
            return (r << 16) | (g << 8) | b;
        }

        constexpr Color invert() const {
            return Color{.r = static_cast<uint8_t>(255 - r), .g = static_cast<uint8_t>(255 - g), .b = static_cast<uint8_t>(255 - b)};
        }

        constexpr bool operator==(const Color &other) const {
            return r == other.r && g == other.g && b == other.b;
        }

        constexpr bool operator!=(const Color &other) const {
            return !(*this == other);
        }

        std::string to_string_hex() const {
            return std::format("{:02X}{:02X}{:02X}", r, g, b);
        }
    };

#define COLOR_WHITE       (Color::from_hex(0xFFFFFF))
#define COLOR_SILVER      (Color::from_hex(0xC0C0C0))
#define COLOR_GRAY        (Color::from_hex(0x808080))
#define COLOR_BLACK       (Color::from_hex(0x000000))
#define COLOR_RED         (Color::from_hex(0xFF0000))
#define COLOR_MAROON      (Color::from_hex(0x800000))
#define COLOR_YELLOW      (Color::from_hex(0xFFFF00))
#define COLOR_OLIVE       (Color::from_hex(0x808000))
#define COLOR_LIME        (Color::from_hex(0x00FF00))
#define COLOR_GREEN       (Color::from_hex(0x008000))
#define COLOR_AQUA        (Color::from_hex(0x00FFFF))
#define COLOR_TEAL        (Color::from_hex(0x008080))
#define COLOR_BLUE        (Color::from_hex(0x0000FF))
#define COLOR_NAVY        (Color::from_hex(0x000080))
#define COLOR_FUCHSIA     (Color::from_hex(0xFF00FF))
#define COLOR_PURPLE      (Color::from_hex(0x800080))

#define STYLE_RESET       (1 << 0)
#define STYLE_BOLD        (1 << 1)
#define STYLE_LIGHT       (1 << 2)
#define STYLE_ITALIC      (1 << 3)
#define STYLE_UNDERLINE   (1 << 4)
#define STYLE_BLINK       (1 << 5)
#define STYLE_INVERSE     (1 << 6)
#define STYLE_INVISIBLE   (1 << 7)
#define STYLE_CROSSED_OUT (1 << 8)
#define STYLE_UNDERLINE2  (1 << 9)

#define STYLE_NO_FG       (1 << 10)
#define STYLE_NO_BG       (1 << 11)

    /**
     * Represent the style of a cell
     */
    struct Style {
        Color bg = Color::from_hex(0x000000);
        Color fg = Color::from_hex(0xffffff);
        uint16_t mode = STYLE_RESET;

        constexpr Style invert() const {
            return Style{.bg = bg.invert(), .fg = fg.invert(), .mode = mode};
        }

        constexpr bool operator==(const Style &other) const {
            return bg == other.bg && fg == other.fg && mode == other.mode;
        }

        constexpr bool operator!=(const Style &other) const {
            return !(*this == other);
        }
    };

    /**
     * Represents a character with style attributes
     */
    struct StyledChar {
        wchar_t value = '\0';
        Style style = {};

        ~StyledChar() = default;
    };

    namespace internal
    {
        std::vector<StyledChar> clr_fmt(const std::wstring &fmt);
    }

    /**
     * @brief This function performs color formatting and returns a vector of styled chars
     * which can be used to render text. 
     *
     * First, std::vformat is applied to fmt for any kind of custom formatting. Then the output
     * string from std::vformat is parsed according to the following syntax.
     *
     * %(#RRGGBB, #RRGGBB) -> Describes a style with bg (first element) and fg (second element)
     *                        where RRGGBB are hex codes of the color. Any text after this 
     *                        marker will be styled using these colors.
     * %end                -> Describes the end of the previous marker and sets the style to default.
     *                        It is not an error to pair the markers as this marker only resets the style
     *                        to default.
     *                        (default style is bg=COLOR_BLACK, fg=COLOR_WHITE)
     * %%                  -> Outputs a single %
     * 
     * Any text before the first marker is styled using default style (bg=BLACK, fg=WHITE). 
     * If the function finds any error in the color format, then it simply outputs those characters
     * instead of doing anything. Only std::format_error is thrown when std::vformat is called.
     * 
     * @throws std::format_error only thrown by std::vformat
     * @param [in] fmt the format string
     * @param [in] args the format arguments
     * @return std::vector<StyledChar>
     */
    template<class... Args>
    std::vector<StyledChar> color_fmt(std::format_string<Args...> fmt, Args... args) {
        return internal::clr_fmt(str_to_wstr(std::vformat(fmt.get(), std::make_format_args(args...))));
    }

    /**
     * @brief This function performs color formatting and returns a vector of styled chars
     * which can be used to render text. 
     *
     * First, std::vformat is applied to fmt for any kind of custom formatting. Then the output
     * string from std::vformat is parsed according to the following syntax.
     *
     * %(#RRGGBB, #RRGGBB) -> Describes a style with bg (first element) and fg (second element)
     *                        where RRGGBB are hex codes of the color. Any text after this 
     *                        marker will be styled using these colors.
     * %end                -> Describes the end of the previous marker and sets the style to default.
     *                        It is not an error to pair the markers as this marker only resets the style
     *                        to default.
     *                        (default style is bg=COLOR_BLACK, fg=COLOR_WHITE)
     * %%                  -> Outputs a single %
     * 
     * Any text before the first marker is styled using default style (bg=BLACK, fg=WHITE). 
     * If the function finds any error in the color format, then it simply outputs those characters
     * instead of doing anything. Only std::format_error is thrown when std::vformat is called.
     * 
     * @throws std::format_error only thrown by std::vformat
     * @param [in] fmt the format wstring
     * @param [in] args the format arguments
     * @return std::vector<StyledChar>
     */
    template<class... Args>
    std::vector<StyledChar> color_fmt(std::wformat_string<Args...> fmt, Args... args) {
        return internal::clr_fmt(std::vformat(fmt.get(), std::make_wformat_args(args...)));
    }

    /**
     * Represents a box border
     */
    struct BoxBorder {
        StyledChar top_left, top, top_right;
        StyledChar left, right;
        StyledChar bottom_left, bottom, bottom_right;
    };

    inline static const constexpr BoxBorder BOX_BORDER_DEFAULT = {
            {'+', {}},
            {'-', {}},
            {'+', {}},
            {'|', {}},
            {'|', {}},
            {'+', {}},
            {'-', {}},
            {'+', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_LIGHT = {
            {L'┌', {}},
            {L'─', {}},
            {L'┐', {}},
            {L'│', {}},
            {L'│', {}},
            {L'└', {}},
            {L'─', {}},
            {L'┘', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_HEAVY = {
            {L'┏', {}},
            {L'━', {}},
            {L'┓', {}},
            {L'┃', {}},
            {L'┃', {}},
            {L'┗', {}},
            {L'━', {}},
            {L'┛', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_LIGHT_DASHED2 = {
            {L'┌', {}},
            {L'╌', {}},
            {L'┐', {}},
            {L'╎', {}},
            {L'╎', {}},
            {L'└', {}},
            {L'╌', {}},
            {L'┘', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_LIGHT_DASHED3 = {
            {L'┌', {}},
            {L'┄', {}},
            {L'┐', {}},
            {L'┆', {}},
            {L'┆', {}},
            {L'└', {}},
            {L'┄', {}},
            {L'┘', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_LIGHT_DASHED4 = {
            {L'┌', {}},
            {L'┈', {}},
            {L'┐', {}},
            {L'┊', {}},
            {L'┊', {}},
            {L'└', {}},
            {L'┈', {}},
            {L'┘', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_HEAVY_DASHED2 = {
            {L'┏', {}},
            {L'╍', {}},
            {L'┓', {}},
            {L'╏', {}},
            {L'╏', {}},
            {L'┗', {}},
            {L'╍', {}},
            {L'┛', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_HEAVY_DASHED3 = {
            {L'┏', {}},
            {L'┅', {}},
            {L'┓', {}},
            {L'┇', {}},
            {L'┇', {}},
            {L'┗', {}},
            {L'┅', {}},
            {L'┛', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_HEAVY_DASHED4 = {
            {L'┏', {}},
            {L'┉', {}},
            {L'┓', {}},
            {L'┋', {}},
            {L'┋', {}},
            {L'┗', {}},
            {L'┉', {}},
            {L'┛', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_DOUBLE = {
            {L'╔', {}},
            {L'═', {}},
            {L'╗', {}},
            {L'║', {}},
            {L'║', {}},
            {L'╚', {}},
            {L'═', {}},
            {L'╝', {}},
    };

    inline static const constexpr BoxBorder BOX_BORDER_ROUNDED = {
            {L'╭', {}},
            {L'─', {}},
            {L'╮', {}},
            {L'│', {}},
            {L'│', {}},
            {L'╰', {}},
            {L'─', {}},
            {L'╯', {}},
    };

    /**
     * Represents a table border
     */
    struct TableBorder {
        StyledChar vertical, horizontal;
        StyledChar top_left, top_right;
        StyledChar bottom_left, bottom_right;
        StyledChar center_joint;
        StyledChar left_joint, right_joint, top_joint, bottom_joint;
    };

    inline static const constexpr TableBorder TABLE_BORDER_DEFAULT = {
            {'|', {}},
            {'-', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_LIGHT = {
            {L'│', {}},
            {L'─', {}},
            {L'┌', {}},
            {L'┐', {}},
            {L'└', {}},
            {L'┘', {}},
            {L'┼', {}},
            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_HEAVY = {
            {L'┃', {}},
            {L'━', {}},
            {L'┏', {}},
            {L'┓', {}},
            {L'┗', {}},
            {L'┛', {}},
            {L'╋', {}},
            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_LIGHT_DASHED2 = {
            {L'╎', {}},
            {L'╌', {}},
            {L'┌', {}},
            {L'┐', {}},
            {L'└', {}},
            {L'┘', {}},
            {L'┼', {}},
            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_LIGHT_DASHED3 = {
            {L'┆', {}},
            {L'┄', {}},
            {L'┌', {}},
            {L'┐', {}},
            {L'└', {}},
            {L'┘', {}},
            {L'┼', {}},
            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_LIGHT_DASHED4 = {
            {L'┊', {}},
            {L'┈', {}},
            {L'┌', {}},
            {L'┐', {}},
            {L'└', {}},
            {L'┘', {}},
            {L'┼', {}},
            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_HEAVY_DASHED2 = {
            {L'╏', {}},
            {L'╍', {}},
            {L'┏', {}},
            {L'┓', {}},
            {L'┗', {}},
            {L'┛', {}},
            {L'╋', {}},
            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_HEAVY_DASHED3 = {
            {L'┇', {}},
            {L'┅', {}},
            {L'┏', {}},
            {L'┓', {}},
            {L'┗', {}},
            {L'┛', {}},
            {L'╋', {}},
            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_HEAVY_DASHED4 = {
            {L'┋', {}},
            {L'┉', {}},
            {L'┏', {}},
            {L'┓', {}},
            {L'┗', {}},
            {L'┛', {}},
            {L'╋', {}},
            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_DOUBLE = {
            {L'║', {}},
            {L'═', {}},
            {L'╔', {}},
            {L'╗', {}},
            {L'╚', {}},
            {L'╝', {}},
            {L'╬', {}},
            {L'╠', {}},
            {L'╣', {}},
            {L'╦', {}},
            {L'╩', {}},
    };

    inline static const constexpr TableBorder TABLE_BORDER_ROUNDED = {
            {L'│', {}},
            {L'─', {}},
            {L'╭', {}},
            {L'╮', {}},
            {L'╰', {}},
            {L'╯', {}},
            {L'┼', {}},
            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    /**
     * Represents scroll bar style
     */
    struct ScrollBar {
        StyledChar home;
        StyledChar top, v_bar, v_node, bottom;
        StyledChar left, h_bar, h_node, right;
    };

    inline static const constexpr ScrollBar SCROLL_DEFAULT = {
            {'x', {}},
            {'+', {}},
            {'|', {}},
            {'*', {}},
            {'+', {}},
            {'+', {}},
            {'-', {}},
            {'*', {}},
            {'+', {}},
    };

    inline static const constexpr ScrollBar SCROLL_LIGHT = {
            {L'●', {}},
            {L'↑', {}},
            {L'│', {}},
            {L'░', {}},
            {L'↓', {}},
            {L'←', {}},
            {L'─', {}},
            {L'░', {}},
            {L'→', {}},
    };

    inline static const constexpr ScrollBar SCROLL_DASHED = {
            {L'●', {}},
            {L'⇡', {}},
            {L'╎', {}},
            {L'░', {}},
            {L'⇣', {}},
            {L'⇠', {}},
            {L'╌', {}},
            {L'░', {}},
            {L'⇢', {}},
    };

    inline static const constexpr ScrollBar SCROLL_DOUBLE = {
            {L'●', {}},
            {L'⇑', {}},
            {L'║', {}},
            {L'░', {}},
            {L'⇓', {}},
            {L'⇐', {}},
            {L'═', {}},
            {L'░', {}},
            {L'⇒', {}},
    };

    /**
     * Represents different alignment options
     */
    enum class Align {
        TOP_LEFT,
        TOP,
        TOP_RIGHT,
        LEFT,
        CENTER,
        RIGHT,
        BOTTOM_LEFT,
        BOTTOM,
        BOTTOM_RIGHT,
    };

    /**
     * Represents the position of an object
     */
    struct Position {
        union {
            size_t x;
            size_t col = 0;
        };

        union {
            size_t y;
            size_t row = 0;
        };

        Position operator+(const Position other) const {
            return {.col = col + other.col, .row = row + other.row};
        }

        Position operator-(const Position other) const {
            return {.col = col - other.col, .row = row - other.row};
        }

        Position &operator+=(const Position other) {
            col += other.col;
            row += other.row;
            return *this;
        }

        Position &operator-=(const Position other) {
            col -= other.col;
            row -= other.row;
            return *this;
        }

        constexpr bool operator==(const Position &other) const {
            return x == other.x && y == other.y;
        }

        constexpr bool operator!=(const Position &other) const {
            return !(*this == other);
        }
    };

    /**
     * Represents size of an object
     */
    struct Size {
        size_t width = 0;
        size_t height = 0;

        Size operator+(const Size size) const {
            return {.width = width + size.width, .height = height + size.height};
        }

        Size operator-(const Size size) const {
            return {.width = width > size.width ? width - size.width : 0, .height = height > size.height ? height - size.height : 0};
        }

        Size operator*(const size_t factor) const {
            return {.width = width * factor, .height = height * factor};
        }

        Size operator/(const size_t factor) const {
            return {.width = width / factor, .height = height / factor};
        }

        Size &operator+=(const Size size) {
            width += size.width;
            height += size.height;
            return *this;
        }

        Size &operator-=(const Size size) {
            width = width > size.width ? width - size.width : 0;
            height = height > size.height ? height - size.height : 0;
            return *this;
        }

        Size &operator*=(const size_t factor) {
            width *= factor;
            height *= factor;
            return *this;
        }

        Size &operator/=(const size_t factor) {
            width /= factor;
            height /= factor;
            return *this;
        }

        constexpr bool operator==(const Size &other) const {
            return width == other.width && height == other.height;
        }

        constexpr bool operator!=(const Size &other) const {
            return !(*this == other);
        }
    };
}    // namespace nite

// --------------------------------
//  Library internal definitions
// --------------------------------

namespace nite
{
    namespace internal
    {
        namespace console
        {
            bool is_tty();
            Result clear();
            Result size(size_t &width, size_t &height);
            Result print(const std::string &text = "");

            void set_style(const Style style);
            void gotoxy(const size_t col, const size_t row);
            void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style);

            Result init();
            Result restore();
        };    // namespace console

        /**
         * Represents the information of a screen cell
         */
        struct Cell {
            wchar_t value = ' ';
            Style style = {};

            constexpr bool operator==(const Cell &other) const {
                return value == other.value && style == other.style;
            }

            constexpr bool operator!=(const Cell &other) const {
                return !(*this == other);
            }
        };

        /**
         * Represents a two dimensional array of cells which is used as screen buffer
         */
        class CellBuffer {
            size_t width;
            size_t height;
            std::unique_ptr<Cell[]> cells;

          public:
            CellBuffer(size_t width, size_t height) : width(width), height(height) {
                cells = std::make_unique<Cell[]>(width * height);
            }

            CellBuffer(const Size size) : width(size.width), height(size.height) {
                cells = std::make_unique<Cell[]>(size.width * size.height);
            }

            CellBuffer(const CellBuffer &other) : width(other.width), height(other.height) {
                cells = std::make_unique<Cell[]>(width * height);
                for (size_t i = 0; i < width * height; i++)
                    cells[i] = other.cells[i];
            }

            CellBuffer(CellBuffer &&other) = default;

            CellBuffer &operator=(const CellBuffer &other) {
                if (this == &other)
                    return *this;

                width = other.width;
                height = other.height;

                cells = std::make_unique<Cell[]>(width * height);
                for (size_t i = 0; i < width * height; i++)
                    cells[i] = other.cells[i];
                return *this;
            }

            CellBuffer &operator=(CellBuffer &&other) = default;
            ~CellBuffer() = default;

            bool contains(size_t col, size_t row) const {
                return col < width && row < height;
            }

            const Cell &at(size_t col, size_t row) const {
                return cells[row * width + col];
            }

            const Cell &at(const Position pos) const {
                return cells[pos.row * width + pos.col];
            }

            Cell &at(size_t col, size_t row) {
                return cells[row * width + col];
            }

            Cell &at(const Position pos) {
                return cells[pos.row * width + pos.col];
            }

            size_t get_width() const {
                return width;
            }

            size_t get_height() const {
                return height;
            }

            Size size() const {
                return Size{.width = width, .height = height};
            }
        };
    }    // namespace internal
}    // namespace nite

// --------------------------------
//  Event definitions
// --------------------------------

namespace nite
{
    // Forward declaration
    struct State;

#define KEY_SHIFT ((uint8_t) (1 << 0))    // Shift key
#define KEY_CTRL  ((uint8_t) (1 << 1))    // Control on macOS, Ctrl on other platforms
#define KEY_ALT   ((uint8_t) (1 << 2))    // Option on macOS, Alt on other platforms
#define KEY_SUPER ((uint8_t) (1 << 3))    // Command on macOS, Win key on Windows, Super on other platforms
#define KEY_META  ((uint8_t) (1 << 4))    // Meta key

    /**
     * Represents different supported key codes
     */
    enum class KeyCode {
        // Alphabet keys
        K_A,
        K_B,
        K_C,
        K_D,
        K_E,
        K_F,
        K_G,
        K_H,
        K_I,
        K_J,
        K_K,
        K_L,
        K_M,
        K_N,
        K_O,
        K_P,
        K_Q,
        K_R,
        K_S,
        K_T,
        K_U,
        K_V,
        K_W,
        K_X,
        K_Y,
        K_Z,
        // Number keys
        K_0,
        K_1,
        K_2,
        K_3,
        K_4,
        K_5,
        K_6,
        K_7,
        K_8,
        K_9,
        // Symbol keys
        BANG,          // !
        AT,            // @
        HASH,          // #
        DOLLAR,        // $
        PERCENT,       // %
        CARET,         // ^
        AMPERSAND,     // &
        ASTERISK,      // *
        LPAREN,        // (
        RPAREN,        // )
        LBRACE,        // {
        RBRACE,        // }
        LBRACKET,      // [
        RBRACKET,      // ]
        TILDE,         // ~
        BQUOTE,        // `
        COLON,         // :
        SEMICOLON,     // ;
        DQUOTE,        // "
        SQUOTE,        // '
        LESS,          // <
        GREATER,       // >
        HOOK,          // ?
        SLASH,         // /
        COMMA,         // ,
        PERIOD,        // .
        BACKSLASH,     /* \ */
        PIPE,          // |
        UNDERSCORE,    // _
        MINUS,         // -
        PLUS,          // +
        EQUAL,         // =

        // Function keys
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,

        // Special keys
        BACKSPACE,
        ENTER,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        HOME,
        END,
        PAGE_UP,
        PAGE_DOWN,
        TAB,
        INSERT,
        DELETE,
        ESCAPE,
        SPACE,
    };

    // TODO: debug thing do not use this
    struct KeyCodeInfo {
        KeyCodeInfo() = delete;
        KeyCodeInfo(const KeyCodeInfo &) = delete;
        KeyCodeInfo(KeyCodeInfo &&) = delete;
        KeyCodeInfo &operator=(const KeyCodeInfo &) = delete;
        KeyCodeInfo &operator=(KeyCodeInfo &&) = delete;
        ~KeyCodeInfo() = delete;

        static inline constexpr bool IsPrint(KeyCode key_code) {
            return static_cast<int>(key_code) < static_cast<int>(KeyCode::F1);
        }

        static inline constexpr const char *DebugString(KeyCode key_code) {
            switch (key_code) {
            case KeyCode::K_A:
                return "K_A";
            case KeyCode::K_B:
                return "K_B";
            case KeyCode::K_C:
                return "K_C";
            case KeyCode::K_D:
                return "K_D";
            case KeyCode::K_E:
                return "K_E";
            case KeyCode::K_F:
                return "K_F";
            case KeyCode::K_G:
                return "K_G";
            case KeyCode::K_H:
                return "K_H";
            case KeyCode::K_I:
                return "K_I";
            case KeyCode::K_J:
                return "K_J";
            case KeyCode::K_K:
                return "K_K";
            case KeyCode::K_L:
                return "K_L";
            case KeyCode::K_M:
                return "K_M";
            case KeyCode::K_N:
                return "K_N";
            case KeyCode::K_O:
                return "K_O";
            case KeyCode::K_P:
                return "K_P";
            case KeyCode::K_Q:
                return "K_Q";
            case KeyCode::K_R:
                return "K_R";
            case KeyCode::K_S:
                return "K_S";
            case KeyCode::K_T:
                return "K_T";
            case KeyCode::K_U:
                return "K_U";
            case KeyCode::K_V:
                return "K_V";
            case KeyCode::K_W:
                return "K_W";
            case KeyCode::K_X:
                return "K_X";
            case KeyCode::K_Y:
                return "K_Y";
            case KeyCode::K_Z:
                return "K_Z";
            case KeyCode::K_0:
                return "K_0";
            case KeyCode::K_1:
                return "K_1";
            case KeyCode::K_2:
                return "K_2";
            case KeyCode::K_3:
                return "K_3";
            case KeyCode::K_4:
                return "K_4";
            case KeyCode::K_5:
                return "K_5";
            case KeyCode::K_6:
                return "K_6";
            case KeyCode::K_7:
                return "K_7";
            case KeyCode::K_8:
                return "K_8";
            case KeyCode::K_9:
                return "K_9";
            case KeyCode::BANG:
                return "BANG";
            case KeyCode::AT:
                return "AT";
            case KeyCode::HASH:
                return "HASH";
            case KeyCode::DOLLAR:
                return "DOLLAR";
            case KeyCode::PERCENT:
                return "PERCENT";
            case KeyCode::CARET:
                return "CARET";
            case KeyCode::AMPERSAND:
                return "AMPERSAND";
            case KeyCode::ASTERISK:
                return "ASTERISK";
            case KeyCode::LPAREN:
                return "LPAREN";
            case KeyCode::RPAREN:
                return "RPAREN";
            case KeyCode::LBRACE:
                return "LBRACE";
            case KeyCode::RBRACE:
                return "RBRACE";
            case KeyCode::LBRACKET:
                return "LBRACKET";
            case KeyCode::RBRACKET:
                return "RBRACKET";
            case KeyCode::TILDE:
                return "TILDE";
            case KeyCode::BQUOTE:
                return "BQUOTE";
            case KeyCode::COLON:
                return "COLON";
            case KeyCode::SEMICOLON:
                return "SEMICOLON";
            case KeyCode::DQUOTE:
                return "DQUOTE";
            case KeyCode::SQUOTE:
                return "SQUOTE";
            case KeyCode::LESS:
                return "LESS";
            case KeyCode::GREATER:
                return "GREATER";
            case KeyCode::HOOK:
                return "HOOK";
            case KeyCode::SLASH:
                return "SLASH";
            case KeyCode::COMMA:
                return "COMMA";
            case KeyCode::PERIOD:
                return "PERIOD";
            case KeyCode::BACKSLASH:
                return "BACKSLASH";
            case KeyCode::PIPE:
                return "PIPE";
            case KeyCode::UNDERSCORE:
                return "UNDERSCORE";
            case KeyCode::MINUS:
                return "MINUS";
            case KeyCode::PLUS:
                return "PLUS";
            case KeyCode::EQUAL:
                return "EQUAL";
            case KeyCode::F1:
                return "F1";
            case KeyCode::F2:
                return "F2";
            case KeyCode::F3:
                return "F3";
            case KeyCode::F4:
                return "F4";
            case KeyCode::F5:
                return "F5";
            case KeyCode::F6:
                return "F6";
            case KeyCode::F7:
                return "F7";
            case KeyCode::F8:
                return "F8";
            case KeyCode::F9:
                return "F9";
            case KeyCode::F10:
                return "F10";
            case KeyCode::F11:
                return "F11";
            case KeyCode::F12:
                return "F12";
            case KeyCode::F13:
                return "F13";
            case KeyCode::F14:
                return "F14";
            case KeyCode::F15:
                return "F15";
            case KeyCode::F16:
                return "F16";
            case KeyCode::F17:
                return "F17";
            case KeyCode::F18:
                return "F18";
            case KeyCode::F19:
                return "F19";
            case KeyCode::F20:
                return "F20";
            case KeyCode::F21:
                return "F21";
            case KeyCode::F22:
                return "F22";
            case KeyCode::F23:
                return "F23";
            case KeyCode::F24:
                return "F24";
            case KeyCode::BACKSPACE:
                return "BACKSPACE";
            case KeyCode::ENTER:
                return "ENTER";
            case KeyCode::LEFT:
                return "LEFT";
            case KeyCode::RIGHT:
                return "RIGHT";
            case KeyCode::UP:
                return "UP";
            case KeyCode::DOWN:
                return "DOWN";
            case KeyCode::HOME:
                return "HOME";
            case KeyCode::END:
                return "END";
            case KeyCode::PAGE_UP:
                return "PAGE_UP";
            case KeyCode::PAGE_DOWN:
                return "PAGE_DOWN";
            case KeyCode::TAB:
                return "TAB";
            case KeyCode::INSERT:
                return "INSERT";
            case KeyCode::DELETE:
                return "DELETE";
            case KeyCode::ESCAPE:
                return "ESCAPE";
            case KeyCode::SPACE:
                return "SPACE";
            }
            return "";
        }
    };

    struct KeyEvent {
        bool key_down;
        KeyCode key_code;
        char key_char;
        uint8_t modifiers = 0;
    };

    enum class MouseEventKind {
        CLICK,
        DOUBLE_CLICK,
        MOVED,
        SCROLL_DOWN,
        SCROLL_UP,
        SCROLL_LEFT,
        SCROLL_RIGHT,
    };

    enum class MouseButton {
        NONE,
        LEFT,
        MIDDLE,
        RIGHT,
    };

    struct MouseEvent {
        MouseEventKind kind;
        MouseButton button = MouseButton::NONE;
        Position pos;
        uint8_t modifiers = 0;
    };

    struct FocusEvent {
        bool focus_gained;
    };

    struct ResizeEvent {
        Size size;
    };

    struct DebugEvent {
        std::string text;
    };

    using Event = std::variant<KeyEvent, MouseEvent, FocusEvent, ResizeEvent, DebugEvent>;

    namespace internal
    {
        bool PollRawEvent(Event &event);

        template<typename Fn, typename Event>
        consteval bool is_handler_of_this_event() {
            return std::is_invocable_v<Fn, Event>;
        }

        template<typename Fn, std::size_t... I>
        consteval bool is_valid_handler_impl(std::index_sequence<I...>) {
            return (is_handler_of_this_event<Fn, std::variant_alternative_t<I, Event>>() || ...);
        }

        template<class Fn>
        consteval bool is_valid_handler() {
            return is_valid_handler_impl<Fn>(std::make_index_sequence<std::variant_size_v<Event>>{});
        }

        template<class Ev, class... Fn>
        consteval bool has_all_event_handlers_impl1() {
            return (is_handler_of_this_event<Fn, Ev>() || ...);
        }

        template<class... Fn, size_t... I>
        consteval bool has_all_event_handlers_impl0(std::index_sequence<I...>) {
            return (has_all_event_handlers_impl1<std::variant_alternative_t<I, Event>, Fn...>() && ...);
        }

        template<class... Fn>
        consteval bool has_all_event_handlers() {
            return has_all_event_handlers_impl0<Fn...>(std::make_index_sequence<std::variant_size_v<Event>>{});
        }
    }    // namespace internal

    /**
     * @brief Polls an event
     *
     * Checks whether there are any pending events.
     * If there is any event available, it is captured,
     * saved in \p event and returns true. Otherwise, it
     * does nothing to \p event and returns false.
     * 
     * \note This is a non-blocking call (not guaranteed). 
     * But it is guaranteed that this call will not block longer than 5 milliseconds
     * 
     * @param [inout] state the console state to work on
     * @param [out] event the captured event
     * @return true if event was available
     * @return false if there was no event available
     */
    bool PollEvent(State &state, Event &event);

    /**
     * This concept defines a valid event handler
     * @tparam Fn type of the event handler
     */
    template<class Fn>
    concept Handler = internal::is_valid_handler<Fn>();

    template<Handler... Handlers>
    struct HandlerMechanism : Handlers... {
        using Handlers::operator()...;
    };

    // Some compilers might require this explicit deduction guide
    template<class... Handlers>
    HandlerMechanism(Handlers...) -> HandlerMechanism<Handlers...>;

    /**
     * @brief Handles an event
     * 
     * This function handles \p event by using std::visit on the event.
     * The handlers are provided by \p handlers . It is not necessary to handle
     * every event, in that case a default fallback handler for those unhandled events will be called.
     * 
     * @param [in] event the event to be handled
     * @param [in] handlers the handlers provided
     */
    template<Handler... Handlers>
    inline void HandleEvent(Event event, Handlers &&...handlers) {
        if constexpr (internal::has_all_event_handlers<Handlers...>())
            std::visit(HandlerMechanism{handlers...}, event);
        else
            std::visit(HandlerMechanism{handlers..., [](const auto &) {}}, event);
    }

    // Keyboard
    bool IsKeyPressed(const State &state, KeyCode key_code);
    bool IsKeyDown(const State &state, KeyCode key_code);
    bool IsKeyUp(const State &state, KeyCode key_code);

    // Mouse
    Position GetMousePos(const State &state);
    Position GetMouseRelPos(const State &state);
}    // namespace nite

// --------------------------------
//  Library definitions
// --------------------------------

namespace nite
{
    /**
     * @brief Defines a miscellaneous handlers for XXXXInfo structs
     * @tparam T the type of the struct
     */
    template<typename T>
    using HandlerFn = std::function<void(T &)>;

    /**
     * Represents a library state
     */
    struct State {
        class StateImpl;
        std::unique_ptr<StateImpl> impl;

        State(std::unique_ptr<StateImpl> impl);

        State() = delete;
        State(const State &) = delete;
        State(State &&) = delete;
        State &operator==(const State &) = delete;
        State &operator==(State &&) = delete;
    };

    /**
     * Gets the size of the console window
     * @return Size
     */
    Size GetWindowSize();
    /**
     * Returns the console state
     * @return State& 
     */
    State &GetState();

    /**
     * Initializes the console and prepares all necessary components.
     * Caps the FPS of the console screen at 60.
     * @param [inout] state the console state to work on
     * @return Result
     */
    Result Initialize(State &state);
    /**
     * Cleanups the console and restores the terminal state
     * @param [inout] state the console state to work on
     * @return Result
     */
    Result Cleanup();

    /**
     * Returns the size of the console screen buffer for the current frame
     * @param [inout] state the console state to work on
     * @return Size 
     */
    Size GetBufferSize(const State &state);
    /**
     * Returns the top_left position of the current selected pane (the most recent pane)
     * @param [inout] state the console state to work on
     * @return Size 
     */
    Position GetPanePosition(const State &state);
    /**
     * Returns the size of the current selected pane (the most recent pane)
     * @param [inout] state the console state to work on
     * @return Size 
     */
    Size GetPaneSize(const State &state);
    /**
     * Returns the delta time of the previous frame in seconds
     * @param [inout] state the console state to work on
     * @return double 
     */
    double GetDeltaTime(const State &state);
    /**
     * Returns the current frames per second
     * @param [inout] state the console state to work on
     * @return double 
     */
    double GetFPS(const State &state);
    /**
     * Returns the target FPS
     * @param [inout] state the console state to work on
     * @return double 
     */
    double GetTargetFPS(const State &state);
    /**
     * Caps the FPS of the console screen at @p fps. 
     * Capping the FPS can also be termed as setting the
     * target FPS.
     * @param [inout] state the console state to work on
     */
    void SetTargetFPS(const State &state, double fps);
    /**
     * Returns whether the console window should be closed
     * @param [inout] state the console state to work on
     * @return true if window is closed
     * @return false if window is not closed
     */
    bool ShouldWindowClose(const State &state);

    /**
     * Creates and pushes a new screen buffer to the swapchain
     * @param [inout] state the console state to work on
     */
    void BeginDrawing(State &state);
    /**
     * Pops the latest frame from the swapchain and selectively renders 
     * the screen buffer on the console window
     * @param [inout] state the console state to work on
     */
    void EndDrawing(State &state);
    /**
     * Closes the console window
     * @param [inout] state the console state to work on
     */
    void CloseWindow(State &state);

    /**
     * Sets a specific cell on the console window
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] position the position of the cell
     * @param [in] style the style of the cell
     */
    void SetCell(State &state, wchar_t value, const Position position, const Style style = {});
    /**
     * Sets the style of a specific cell on the console window
     * @param [inout] state the console state to work on
     * @param [in] position the position of the cell
     * @param [in] style the style of the cell
     */
    void SetCellStyle(State &state, const Position position, const Style style);
    /**
     * Sets the bg color of a specific cell on the console window
     * @param [inout] state the console state to work on
     * @param [in] position the position of the cell
     * @param [in] style the bg color of the cell
     */
    void SetCellBG(State &state, const Position position, const Color color);
    /**
     * Sets the fg color of a specific cell on the console window
     * @param [inout] state the console state to work on
     * @param [in] position the position of the cell
     * @param [in] style the fg color of the cell
     */
    void SetCellFG(State &state, const Position position, const Color color);
    /**
     * Fills a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos1 the first position provided
     * @param [in] pos2 the second position provided
     * @param [in] style the style of the cells
     */
    void FillCells(State &state, wchar_t value, const Position pos1, const Position pos2, const Style style = {});
    /**
     * Fills a range of cells on the console window given the top_left position and size.
     * Filling starts from (col, row) inclusive to (col+width, row+height) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos the top left corner
     * @param [in] size the size of the range
     * @param [in] style the style of the cells
     */
    void FillCells(State &state, wchar_t value, const Position pos, const Size size, const Style style = {});
    /**
     * Fills the background of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos1 the first position provided
     * @param [in] pos2 the second position provided
     * @param [in] color the color of the cells
     */
    void FillBackground(State &state, const Position pos1, const Position pos2, const Color color);
    /**
     * Fill the background of all cells of the selected pane
     * @param [inout] state the console state to work on
     * @param [in] color the background color
     */
    void FillBackground(State &state, const Color color);
    /**
     * Fills the foreground of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos1 the first position provided
     * @param [in] pos2 the second position provided
     * @param [in] color the color of the cells
     */
    void FillBackground(State &state, const Position pos, const Size size, const Color color);
    /**
     * Fills the background of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos1 the first position provided
     * @param [in] pos2 the second position provided
     * @param [in] color the color of the cells
     */
    void FillForeground(State &state, const Position pos1, const Position pos2, const Color color);
    /**
     * Fills the foreground of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param [inout] state the console state to work on
     * @param [in] value the cell text
     * @param [in] pos1 the first position provided
     * @param [in] pos2 the second position provided
     * @param [in] color the color of the cells
     */
    void FillForeground(State &state, const Position pos, const Size size, const Color color);
    /**
     * Fill the foreground of all cells of the selected pane
     * @param [inout] state the console state to work on
     * @param [in] color the foreground color
     */
    void FillForeground(State &state, const Color color);
    /**
     * Draws a line on the console window where \p start is the starting point and 
     * \p end is the ending point. Line is always drawn starting from \p start to \p end (exclusive).
     * @param [inout] state the console state to work on
     * @param [in] start the starting point
     * @param [in] end the ending point
     * @param [in] fill the fill of the line
     * @param [in] style the style of the line
     */
    void DrawLine(State &state, const Position start, const Position end, wchar_t fill, const Style style = {});

    /**
     * Creates a pane on the screen with the specified position and size.
     *
     * \note Position values used before the corresponding EndPane call 
     * are always relative to the top_left position of this Pane
     * 
     * @param [inout] state the console state to work on
     * @param [in] top_left the position of the top left corner of the pane
     * @param [in] size the size of the pane
     */
    void BeginPane(State &state, const Position top_left, const Size size);

    struct ScrollPaneInfo {
        /// Position of the scroll pane (top left corner)
        Position pos = {};
        /// Minimum size (actual viewport size) of the scroll pane
        Size min_size = {};
        /// Maximum size (the maximum size of the content) of the scroll pane
        Size max_size = {};
        /// Style of the scroll bar
        ScrollBar scroll_bar = SCROLL_DEFAULT;
        /// Scroll factor of the scroll bar (the speed of scroll)
        float scroll_factor = 1.0f;
        /// Whether vertical scroll bar should be displayed
        bool show_vscroll_bar = true;
        /// Whether horizontal scroll bar should be displayed
        bool show_hscroll_bar = true;
        /// Whether scroll bar home (back to top button) should be displayed
        bool show_scroll_home = true;

        /// Handler triggered when vertical scroll occurs
        HandlerFn<ScrollPaneInfo> on_vscroll = {};
        /// Handler triggered when horizontal scroll occurs
        HandlerFn<ScrollPaneInfo> on_hscroll = {};
    };

    /**
     * Creates a scroll pane on the screen with the specified information
     * provided by the ScrollPaneInfo struct.
     * 
     * \note Position values used before the corresponding EndPane call 
     * are always relative to the top_left position of this Pane
     * 
     * @param [inout] state the console state to work on
     * @param [inout] state the scroll state of the scroll pane
     * @param [in] info the scroll pane info
     */
    void BeginScrollPane(State &state, Position &pivot, ScrollPaneInfo info);

    struct GridPaneInfo {
        /// Position of the grid pane (top left corner)
        Position pos = {};
        /// Size of the grid pane
        Size size = {};
        /// The proportions of column sizes (sum all items should be 100)
        std::vector<double> col_sizes = {100};
        /// The proportions of row sizes (sum all items should be 100)
        std::vector<double> row_sizes = {100};
    };

    /**
     * Creates a grid pane on the screen with the specified information
     * provided by the GridPaneInfo struct. The GridPane acts like a normal Pane
     * if used without invoking BeginGridCell function. The BeginGridCell function
     * is necessary to enable grid functionality of the GridPane.
     *
     * This function also calculates the top_left position and size of every grid cell
     * by using the data provided by the proportions of col_sizes and row_sizes.
     * 
     * \note Position values used before the corresponding EndPane call 
     * are always relative to the top_left position of this Pane
     * 
     * @param [inout] state the console state to work on
     * @param [in] info the grid pane info
     */
    void BeginGridPane(State &state, GridPaneInfo info);
    /**
     * Creates a normal Pane on the screen. Sets the top_left position and size of the Pane
     * of the \p col th and \p row th GridCell of the GridPane (as if the GridPane is divided into a table).
     * 
     * \note Position values used before the corresponding EndPane call 
     * are always relative to the top_left position of this Pane
     * 
     * @param [inout] state the console state to work on
     * @param [in] col the column index of the grid cell
     * @param [in] row the row index of the grid cell
     */
    void BeginGridCell(State &state, size_t col, size_t row);

    /**
     * Creates a NoPane on the screen. Any screen updates before the corresponding EndPane call
     * are never registered and thus they are never shown.
     *
     * \note Position values used before the corresponding EndPane call 
     * are always relative to the top_left position of this Pane
     * 
     * @param [inout] state the console state to work on
     */
    void BeginNoPane(State &state);

    /**
     * Marks the end of the most recent Pane creation. This function should always be paired with
     * BeginX calls that creates Pane. This can destroy any kind of Pane.
     * @param [inout] state the console state to work on
     */
    void EndPane(State &state);

    /**
     * Draws the border of the current pane. 
     * The border style is provided by \p border
     * @param [inout] state the console state to work on
     * @param [in] border the border style
     */
    void BeginBorder(State &state, const BoxBorder &border = BOX_BORDER_DEFAULT);
    /**
     * Marks the end of BeginBorder. This function increases 
     * the position.x and position.y of the current pane by 1 
     * and reduces size.width and size.height of the current pane by 2.
     * @param [inout] state the console state to work on
     */
    void EndBorder(State &state);
    /**
     * Draws the border of the current pane with some optional text. 
     * The border style is provided by \p border .
     * This function increases the position.x and position.y of the current pane by 1 
     * and reduces size.width and size.height of the current pane by 2.
     * @param [inout] state the console state to work on
     * @param [in] border the border style
     * @param [in] text the optional text
     */
    void DrawBorder(State &state, const BoxBorder &border = BOX_BORDER_DEFAULT, const std::string &text = "");

    Position GetAlignedPos(State &state, const Size size, const Align align);

    /**
     * Draws a horizontal divider at the specified row.
     * @param [inout] state the console state to work on
     * @param [in] row the specified row position
     * @param [in] fill the fill char of the divider
     * @param [in] style the style of the divider
     */
    void DrawHDivider(State &state, size_t row, wchar_t fill = L'─', Style style = {});
    /**
     * Draws a vertical divider at the specified col.
     * @param [inout] state the console state to work on
     * @param [in] col the specified col position
     * @param [in] fill the fill char of the divider
     * @param [in] style the style of the divider
     */
    void DrawVDivider(State &state, size_t col, wchar_t fill = L'│', Style style = {});

    class FocusTable {
        std::list<std::string> keys;
        std::unordered_set<std::string> table;
        std::list<std::string>::iterator focused = std::end(keys);

      public:
        FocusTable() = default;
        FocusTable(const FocusTable &) = default;
        FocusTable(FocusTable &&) = default;
        FocusTable &operator=(const FocusTable &) = default;
        FocusTable &operator=(FocusTable &&) = default;
        ~FocusTable() = default;

        FocusTable(std::initializer_list<std::string> list) {
            for (const std::string &name: list)
                set_focus(name, false);
        }

        /// Returns whether the table is empty
        bool empty() const {
            return table.empty();
        }

        /// Returns the size of the table
        size_t size() const {
            return table.size();
        }

        /// Clears the entire table and nothing is focused
        void clear() {
            keys.clear();
            table.clear();
            focused = std::end(keys);
        }

        /// Returns whether \p name exists in the table
        bool contains(const std::string &name) const {
            return table.find(name) != table.end();
        }

        /// Removes the element with \p name
        void erase(const std::string &name) {
            if (*focused == name)
                focused = std::end(keys);

            keys.remove(name);
            table.erase(name);
        }

        /**
         * Sets focus to the name
         * @param name the name of the element
         * @param focus the focus value
         */
        void set_focus(const std::string &name, bool focus) {
            if (table.find(name) == table.end())
                keys.push_back(name);
            table.insert(name);

            if (focus) {
                if (focused == std::end(keys) || *focused != name)
                    focused = std::find(std::begin(keys), std::end(keys), name);
            } else {
                if (focused != std::end(keys) && *focused == name)
                    focused = std::end(keys);
            }
        }

        /**
         * @param name the name of the element
         * @return true if \p name has focus
         * @return false if \p name does not have focus
         */
        bool has_focus(const std::string &name) const {
            return focused != std::end(keys) && *focused == name;
        }

        /**
         * Returns the name of the focused element as an std::optional
         * @return std::optional<std::string> 
         */
        std::optional<std::string> get_focus_name() const {
            if (focused == std::end(keys))
                return std::nullopt;
            return *focused;
        }

        /**
         * Sets the name of the optional to \p name if some element is focused
         * @param [out] name the name of the focused element
         * @return true if focused element is present
         * @return false if focused element is not present
         */
        bool get_focus_name(std::string &name) const {
            if (focused == std::end(keys))
                return false;
            name = *focused;
            return true;
        }

        /// Focuses the first element
        void focus_front() {
            if (table.empty()) {
                focused = std::end(keys);
                return;
            }
            focused = std::begin(keys);
        }

        /// Focuses the last element
        void focus_back() {
            if (table.empty()) {
                focused = std::end(keys);
                return;
            }
            focused = std::end(keys);
            focused--;
        }

        /// Focuses the next element
        void focus_next() {
            if (table.empty()) {
                focused = std::end(keys);
                return;
            }

            if (focused == std::end(keys))
                focused = std::begin(keys);
            else {
                focused++;
                if (focused == std::end(keys))
                    focused = std::begin(keys);
            }
        }

        /// Focuses the previous element
        void focus_prev() {
            if (table.empty()) {
                focused = std::end(keys);
                return;
            }

            if (focused == std::end(keys))
                focused = std::begin(keys);
            else {
                if (focused == std::begin(keys))
                    focused = std::end(keys);
                focused--;
            }
        }
    };

    struct TextInfo {
        /// The text to display
        std::string text = {};
        /// Position of the text
        Position pos = {};
        /// Style of the text
        Style style = {};
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<TextInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<TextInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<TextInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<TextInfo> on_menu = {};
    };

    /**
     * Displays text on the console window and returns the width of the text.
     * This does not support multi-line text.
     * @param [inout] state the console state to work on
     * @param [in] info the text information provided
     * @return size_t 
     */
    size_t Text(State &state, TextInfo info);

    struct RichTextInfo {
        /// The text to display (with style)
        std::vector<StyledChar> text = {};
        /// Position of the text
        Position pos = {};
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<RichTextInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<RichTextInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<RichTextInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<RichTextInfo> on_menu = {};
    };

    /**
     * Displays rich text on the console window and returns the width of the text.
     * This does not support multi-line text.
     * @param [inout] state the console state to work on
     * @param [in] info the text information provided
     * @return size_t 
     */
    size_t RichText(State &state, RichTextInfo info);

    struct TextBoxInfo {
        /// The text to display
        std::string text = {};
        /// Position of the text box
        Position pos = {};
        /// Size of the text box
        Size size = {};
        /// Style of the text box
        Style style = {};
        /// Whether text should be wrapped
        bool wrap = true;
        /// Alignment of the text inside the text box
        Align align = Align::TOP_LEFT;
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<TextBoxInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<TextBoxInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<TextBoxInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<TextBoxInfo> on_menu = {};
    };

    /**
     * Draws a text box with the specified info provided by \p info .
     * This supports multi-line text.
     * @param [inout] state the console state to work on
     * @param [in] info the text box info
     */
    void TextBox(State &state, TextBoxInfo info);

    struct RichTextBoxInfo {
        /// The text to display (with style)
        std::vector<StyledChar> text = {};
        /// Position of the rich text box
        Position pos = {};
        /// Size of the rich text box
        Size size = {};
        /// Default style of the rich text box
        Style style = {};
        /// Whether text should be wrapped
        bool wrap = true;
        /// Alignment of the text inside the text box
        Align align = Align::TOP_LEFT;
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<RichTextBoxInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<RichTextBoxInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<RichTextBoxInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<RichTextBoxInfo> on_menu = {};
    };

    /**
     * Draws a rich text box the specified info provided by \p info .
     * This supports multi-line text.
     * @param [inout] state the console state to work on
     * @param [in] info the rich text box info
     */
    void RichTextBox(State &state, RichTextBoxInfo info);

    // TODO: find a better way to describe progress bar motion

    inline static constexpr std::array DEFAULT_MOTION = {
            StyledChar{L'█', {}},
    };

    inline static constexpr std::array SLEEK_MOTION = {
            StyledChar{L'▏', {}},
            StyledChar{L'▎', {}},
            StyledChar{L'▍', {}},
            StyledChar{L'▌', {}},
            StyledChar{L'▋', {}},
            StyledChar{L'▊', {}},
            StyledChar{L'▉', {}},
            StyledChar{L'█', {}},
    };

    // template<size_t N>
    // struct PBMotion {
    //     std::array<StyledChar, N> top_down;
    //     std::array<StyledChar, N> down_top;
    //     std::array<StyledChar, N> left_right;
    //     std::array<StyledChar, N> right_left;
    // };

    // enum class PBDirection {
    //     TOP_DOWN,
    //     DOWN_TOP,
    //     LEFT_RIGHT,
    //     RIGHT_LEFT,
    // };

    // TODO: implement all other progress bar motions

    struct ProgressBarInfo {
        /// value is from [0, 1]
        double value = 0.0f;
        Position pos = {};
        size_t length = 0;
        std::vector<StyledChar> motion = {DEFAULT_MOTION.begin(), DEFAULT_MOTION.end()};
        Style style = {};
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<ProgressBarInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<ProgressBarInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<ProgressBarInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<ProgressBarInfo> on_menu = {};
    };

    void ProgressBar(State &state, ProgressBarInfo info);

    struct SimpleTableInfo {
        /// Data of the table
        std::vector<std::string> data = {};
        /// Whether to treat the top row as table header row
        bool include_header_row = true;
        /// Number columns in the table
        size_t num_cols = 0;
        /// Number rows in the table
        size_t num_rows = 0;

        /// Position of the table
        Position pos = {};
        /// Header row style of the table
        Style header_style = {.mode = STYLE_BOLD};
        /// Style of other cells of the table (not header row)
        Style table_style = {};

        /// Whether to show borders
        bool show_border = false;
        /// Border style of the table
        TableBorder border = TABLE_BORDER_DEFAULT;

        /// Whether the thing is focused
        bool focus = false;
    };

    /**
     * Draws a simple table on the screen and returns the size of the table
     * 
     * @param [inout] state the console state to work on
     * @param [in] info the info describing the table
     * @return Size 
     */
    Size SimpleTable(State &state, SimpleTableInfo info);

    class TextInputState {
        bool focus = true;
        bool insert_mode = false;

        std::string data = "";
        size_t cursor = 0;

        bool selection_mode = false;
        size_t selection_pivot = 0;

      public:
        TextInputState() = default;
        TextInputState(const TextInputState &) = default;
        TextInputState(TextInputState &&) = default;
        TextInputState &operator=(const TextInputState &) = default;
        TextInputState &operator=(TextInputState &&) = default;
        ~TextInputState() = default;

        void insert_char(char c) {
            if (insert_mode)
                data.replace(cursor, 1, 1, c);
            else
                data.insert(data.begin() + cursor, c);
            move_right();
        }

        void on_key_backspace() {
            if (1 <= cursor && cursor <= data.size())
                data.erase(cursor - 1, 1);
            move_left();
        }

        void on_key_delete() {
            if (cursor <= data.size() - 1)
                data.erase(cursor, 1);
        }

        void move_left(size_t delta = 1) {
            if (cursor > delta)
                cursor -= delta;
            else
                cursor = 0;
        }

        void move_right(size_t delta = 1) {
            if (cursor + delta >= data.size())
                cursor = data.size();
            else
                cursor += delta;
        }

        void go_home() {
            const auto idx = data.substr(0, cursor).find_last_of('\n');
            cursor = cursor == std::string::npos ? 0 : idx + 1;
        }

        void go_end() {
            const auto idx = data.substr(cursor).find_first_of('\n');
            cursor = idx == std::string::npos ? data.size() : idx + cursor;
        }

        void toggle_insert_mode() {
            insert_mode = !insert_mode;
        }

        void start_selection() {
            if (selection_mode)
                return;
            selection_mode = true;
            selection_pivot = cursor;
        }

        void erase_selection() {
            if (!selection_mode)
                return;
            const auto [selection_start, selection_end] = get_selection_range();
            data.erase(selection_start, selection_end - selection_start);
            cursor = selection_pivot = selection_start;
        }

        void end_selection() {
            selection_mode = false;
        }

        std::pair<size_t, size_t> get_selection_range() {
            if (!selection_mode)
                return {0, 0};
            const size_t selection_start = cursor >= selection_pivot ? selection_pivot : cursor;
            const size_t selection_end = cursor >= selection_pivot ? cursor : selection_pivot;
            return {selection_start, selection_end};
        }

        std::string get_selected_text() const {
            if (!selection_mode)
                return "";
            if (cursor >= selection_pivot)
                return data.substr(selection_pivot, cursor - selection_pivot);
            else
                return data.substr(cursor, selection_pivot - cursor);
        }

        std::vector<StyledChar>
        process(const Style text_style, const Style selection_style, const Style cursor_style, const Style cursor_style_ins,
                const Style cursor_style_sel);

        std::string delete_line() {
            end_selection();
            go_home();
            start_selection();
            go_end();
            const auto result = get_selected_text();
            erase_selection();
            end_selection();
            return result;
        }

        std::string delete_all() {
            const auto result = data;
            data = "";
            cursor = 0;
            selection_mode = false;
            selection_pivot = 0;
            return result;
        }

        bool has_focus() const {
            return focus;
        }

        void set_focus(bool focus) {
            this->focus = focus;
        }

        bool is_insert_mode() const {
            return insert_mode;
        }

        bool is_selected() const {
            return selection_mode;
        }

        const std::string &get_text() const {
            return data;
        }

        size_t get_cursor() const {
            return cursor;
        }

        void set_cursor(size_t index) {
            if (index >= data.size())
                cursor = data.size();
            cursor = index;
        }
    };

    struct TextInputInfo {
        /// Position of the text input
        Position pos = {};
        /// Size of the text input box
        Size size = {};
        /// Whether text should be wrapped
        bool wrap = true;
        /// Whether to handle enter as an event
        bool handle_enter_as_event = false;
        /// Alignment of the text in the text input box
        Align align = Align::TOP_LEFT;

        /// Text style in normal mode
        Style text_style = {.bg = COLOR_BLACK, .fg = COLOR_WHITE};
        /// Text style of selected text
        Style selection_style = {.bg = Color::from_hex(0x3737ac), .fg = COLOR_WHITE};
        /// Cursor style in normal mode
        Style cursor_style = {.bg = COLOR_WHITE, .fg = COLOR_BLACK};
        /// Cursor style in insert mode
        Style cursor_style_ins = {.bg = Color::from_hex(0xe63f32), .fg = COLOR_WHITE};
        /// Cursor style in selection mode
        Style cursor_style_sel = {.bg = Color::from_hex(0x24acf2), .fg = COLOR_WHITE};

        /// Handler triggered when enter is pressed (only when handle_enter_as_event is true)
        HandlerFn<TextInputInfo> on_enter = {};
    };

    void TextInput(State &state, TextInputState &text_state, TextInputInfo info);

    struct TextFieldInfo {
        /// Position of the text field
        Position pos = {};
        /// Width of the text field
        size_t width = 0;
        /// Alignment of the text in the text field
        Align align = Align::TOP_LEFT;

        /// Text style in normal mode
        Style text_style = {.bg = COLOR_BLACK, .fg = COLOR_WHITE};
        /// Text style of selected text
        Style selection_style = {.bg = Color::from_hex(0x3737ac), .fg = COLOR_WHITE};
        /// Cursor style in normal mode
        Style cursor_style = {.bg = COLOR_WHITE, .fg = COLOR_BLACK};
        /// Cursor style in insert mode
        Style cursor_style_ins = {.bg = Color::from_hex(0xe63f32), .fg = COLOR_WHITE};
        /// Cursor style in selection mode
        Style cursor_style_sel = {.bg = Color::from_hex(0x24acf2), .fg = COLOR_WHITE};

        /// Handler triggered when enter is pressed (only when handle_enter_as_event is true)
        HandlerFn<TextFieldInfo> on_enter = {};
    };

    void TextField(State &state, TextInputState &text_state, TextFieldInfo info);

    enum class CheckBoxValue {
        UNCHECKED,
        CHECKED,
        INDETERMINATE,
    };

    struct CheckBoxStyle {
        StyledChar unchecked;
        StyledChar checked;
        StyledChar indeterm;
    };

    inline static constexpr const CheckBoxStyle CHECKBOX_DEFAULT = {
            .unchecked = {L'☐', {}},
            .checked = {L'☑', {}},
            .indeterm = {L'☒', {}},
    };

    struct CheckBoxInfo {
        /// Text of the checkbox
        std::string text = "";
        /// Position of the checkbox
        Position pos = {};
        /// Style of the check box
        CheckBoxStyle check_box = CHECKBOX_DEFAULT;
        /// Whether to allow indeterminate values
        bool allow_indeterm = false;
        /// Style of the checkbox text
        Style style = {};
        /// Whether the thing is focused
        bool focus = false;

        /// Handler triggered when mouse is hovered
        HandlerFn<CheckBoxInfo> on_hover = {};
        /// Handler triggered when mouse clicks on this
        HandlerFn<CheckBoxInfo> on_click = {};
        /// Handler triggered when mouse double clicks on this
        HandlerFn<CheckBoxInfo> on_click2 = {};
        /// Handler triggered when mouse right clicks on this
        HandlerFn<CheckBoxInfo> on_menu = {};
    };

    void CheckBox(State &state, CheckBoxValue &value, CheckBoxInfo info);
}    // namespace nite

template<>
struct std::formatter<nite::Position> {
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.end();
    }

    auto format(const nite::Position &p, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "({}, {})", p.col, p.row);
    }
};

template<>
struct std::formatter<nite::Size> {
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.end();
    }

    auto format(const nite::Size &s, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "({}, {})", s.width, s.height);
    }
};

template<>
struct std::formatter<nite::Color> {
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.end();
    }

    auto format(const nite::Color &c, std::format_context &ctx) const {
        return std::format_to(ctx.out(), "#{:02x}{:02x}{:02x}", c.r, c.g, c.b);
    }
};
