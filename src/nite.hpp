#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <functional>
#include <string>
#include <memory>
#include <variant>

// --------------------------------
//  Error definitions
// --------------------------------

namespace nite
{
    class Result {
        bool success;
        std::string message;

      public:
        Result(const std::string &message) : success(false), message(message) {}

        Result(bool success) : success(success), message() {}

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
    };

#define COLOR_WHITE     (Color::from_hex(0xFFFFFF))
#define COLOR_SILVER    (Color::from_hex(0xC0C0C0))
#define COLOR_GRAY      (Color::from_hex(0x808080))
#define COLOR_BLACK     (Color::from_hex(0x000000))
#define COLOR_RED       (Color::from_hex(0xFF0000))
#define COLOR_MAROON    (Color::from_hex(0x800000))
#define COLOR_YELLOW    (Color::from_hex(0xFFFF00))
#define COLOR_OLIVE     (Color::from_hex(0x808000))
#define COLOR_LIME      (Color::from_hex(0x00FF00))
#define COLOR_GREEN     (Color::from_hex(0x008000))
#define COLOR_AQUA      (Color::from_hex(0x00FFFF))
#define COLOR_TEAL      (Color::from_hex(0x008080))
#define COLOR_BLUE      (Color::from_hex(0x0000FF))
#define COLOR_NAVY      (Color::from_hex(0x000080))
#define COLOR_FUCHSIA   (Color::from_hex(0xFF00FF))
#define COLOR_PURPLE    (Color::from_hex(0x800080))

#define STYLE_RESET     (1 << 0)
#define STYLE_BOLD      (1 << 1)
#define STYLE_UNDERLINE (1 << 2)
#define STYLE_INVERSE   (1 << 3)
#define STYLE_NO_FG     (1 << 4)
#define STYLE_NO_BG     (1 << 5)

    /**
     * Represent the style of a cell
     */
    struct Style {
        Color bg = Color::from_hex(0x000000);
        Color fg = Color::from_hex(0xffffff);
        uint8_t mode = STYLE_RESET;

        constexpr Style reverse() const {
            return Style{.bg = fg, .fg = bg, .mode = mode};
        }

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

    struct StyledChar {
        wchar_t value = '\0';
        Style style = {};
    };

    /**
     * Represents the border information.
     *     
     * Border is represented in the following way:
     * 
     * +----------------+---------+-----------------+
     * | top_left       | top     | top_right       |
     * +----------------+---------+-----------------+
     * | left           | center  | right           |
     * +----------------+---------+-----------------+
     * | bottom_left    | bottom  | bottom_right    |
     * +----------------+---------+-----------------+
     *
     * For example:
     *  + - +
     *  | + |
     *  + - +
     */
    struct Border {
        StyledChar top_left, top, top_right;
        StyledChar left, center, right;
        StyledChar bottom_left, bottom, bottom_right;
        StyledChar left_joint, right_joint, top_joint, bottom_joint;
    };

    inline static const constexpr Border BORDER_DEFAULT = {
            {'+', {}},
            {'-', {}},
            {'+', {}},
            {'|', {}},
            {'+', {}},
            {'|', {}},
            {'+', {}},
            {'-', {}},
            {'+', {}},

            {'+', {}},
            {'+', {}},
            {'+', {}},
            {'+', {}},
    };

    inline static const constexpr Border BORDER_LIGHT = {
            {L'┌', {}},
            {L'─', {}},
            {L'┐', {}},
            {L'│', {}},
            {L'┼', {}},
            {L'│', {}},
            {L'└', {}},
            {L'─', {}},
            {L'┘', {}},

            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr Border BORDER_HEAVY = {
            {L'┏', {}},
            {L'━', {}},
            {L'┓', {}},
            {L'┃', {}},
            {L'╋', {}},
            {L'┃', {}},
            {L'┗', {}},
            {L'━', {}},
            {L'┛', {}},

            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr Border BORDER_LIGHT_DASHED2 = {
            {L'┌', {}},
            {L'╌', {}},
            {L'┐', {}},
            {L'╎', {}},
            {L'┼', {}},
            {L'╎', {}},
            {L'└', {}},
            {L'╌', {}},
            {L'┘', {}},

            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr Border BORDER_LIGHT_DASHED3 = {
            {L'┌', {}},
            {L'┄', {}},
            {L'┐', {}},
            {L'┆', {}},
            {L'┼', {}},
            {L'┆', {}},
            {L'└', {}},
            {L'┄', {}},
            {L'┘', {}},

            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr Border BORDER_LIGHT_DASHED4 = {
            {L'┌', {}},
            {L'┈', {}},
            {L'┐', {}},
            {L'┊', {}},
            {L'┼', {}},
            {L'┊', {}},
            {L'└', {}},
            {L'┈', {}},
            {L'┘', {}},

            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

    inline static const constexpr Border BORDER_HEAVY_DASHED2 = {
            {L'┏', {}},
            {L'╍', {}},
            {L'┓', {}},
            {L'╏', {}},
            {L'╋', {}},
            {L'╏', {}},
            {L'┗', {}},
            {L'╍', {}},
            {L'┛', {}},

            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr Border BORDER_HEAVY_DASHED3 = {
            {L'┏', {}},
            {L'┅', {}},
            {L'┓', {}},
            {L'┇', {}},
            {L'╋', {}},
            {L'┇', {}},
            {L'┗', {}},
            {L'┅', {}},
            {L'┛', {}},

            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr Border BORDER_HEAVY_DASHED4 = {
            {L'┏', {}},
            {L'┉', {}},
            {L'┓', {}},
            {L'┋', {}},
            {L'╋', {}},
            {L'┋', {}},
            {L'┗', {}},
            {L'┉', {}},
            {L'┛', {}},

            {L'┣', {}},
            {L'┫', {}},
            {L'┳', {}},
            {L'┻', {}},
    };

    inline static const constexpr Border BORDER_DOUBLE = {
            {L'╔', {}},
            {L'═', {}},
            {L'╗', {}},
            {L'║', {}},
            {L'┼', {}},
            {L'║', {}},
            {L'╚', {}},
            {L'═', {}},
            {L'╝', {}},

            {L'╠', {}},
            {L'╣', {}},
            {L'╦', {}},
            {L'╩', {}},
    };

    inline static const constexpr Border BORDER_ROUNDED = {
            {L'╭', {}},
            {L'─', {}},
            {L'╮', {}},
            {L'│', {}},
            {L'┼', {}},
            {L'│', {}},
            {L'╰', {}},
            {L'─', {}},
            {L'╯', {}},

            {L'├', {}},
            {L'┤', {}},
            {L'┬', {}},
            {L'┴', {}},
    };

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

        Size operator*(const size_t factor) const {
            return {.width = width * factor, .height = height * factor};
        }

        Size operator/(const size_t factor) const {
            return {.width = width / factor, .height = height / factor};
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

    template<typename T>
    using Handler = std::function<void(T &)>;
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
            Result print(const std::string &text);

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
            Cell *cells;

          public:
            CellBuffer(size_t width, size_t height) : width(width), height(height) {
                cells = new Cell[width * height]{};
            }

            CellBuffer(const Size size) : width(size.width), height(size.height) {
                cells = new Cell[size.width * size.height]{};
            }

            CellBuffer(const CellBuffer &other) : width(other.width), height(other.height) {
                const size_t new_length = width * height;
                cells = new Cell[new_length];
                for (size_t i = 0; i < new_length; i++) {
                    cells[i] = other.cells[i];
                }
            }

            CellBuffer(CellBuffer &&other) : width(other.width), height(other.height), cells(other.cells) {
                other.cells = nullptr;
            }

            CellBuffer &operator=(const CellBuffer &other) {
                width = other.width;
                height = other.height;

                delete[] cells;

                const size_t new_length = width * height;
                cells = new Cell[new_length];
                for (size_t i = 0; i < new_length; i++) {
                    cells[i] = other.cells[i];
                }
                return *this;
            }

            CellBuffer &operator=(CellBuffer &&other) {
                width = other.width;
                height = other.height;
                cells = other.cells;

                other.cells = nullptr;
                return *this;
            }

            ~CellBuffer() {
                delete[] cells;
            }

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
//  Library definitions
// --------------------------------

namespace nite
{
    struct State {
        struct StateImpl;
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
     * Initializes the console and prepares all necessary components
     * @param state the console state to work on
     * @return Result
     */
    Result Initialize(State &state);
    /**
     * Cleanups the console and restores the terminal state
     * @param state the console state to work on
     * @return Result
     */
    Result Cleanup();

    /**
     * Returns the size of the console screen buffer for the current frame
     * @param state the console state to work on
     * @return Size 
     */
    Size GetBufferSize(State &state);
    /**
     * Returns the size of the current selected pane
     * @param state the console state to work on
     * @return Size 
     */
    Size GetPaneSize(State &state);
    /**
     * Returns the delta time in seconds
     * @param state the console state to work on
     * @return double 
     */
    double GetDeltaTime(State &state);
    /**
     * Returns whether the console window should be closed
     * @param state the console state to work on
     * @return true if window is closed
     * @return false if window is not closed
     */
    bool ShouldWindowClose(State &state);

    /**
     * Creates and pushes a new screen buffer to the swapchain
     * @param state the console state to work on
     */
    void BeginDrawing(State &state);
    /**
     * Pops the latest frame from the swapchain and selectively renders 
     * the screen buffer on the console window
     * @param state the console state to work on
     */
    void EndDrawing(State &state);
    /**
     * Closes the console window
     * @param state the console state to work on
     */
    void CloseWindow(State &state);

    /**
     * Sets a specific cell on the console window
     * @param state the console state to work on
     * @param value the cell text
     * @param position the position of the cell
     * @param style the style of the cell
     */
    void SetCell(State &state, wchar_t value, const Position position, const Style style = {});
    /**
     * Sets the style of a specific cell on the console window
     * @param state the console state to work on
     * @param position the position of the cell
     * @param style the style of the cell
     */
    void SetCellStyle(State &state, const Position position, const Style style);
    /**
     * Sets the bg color of a specific cell on the console window
     * @param state the console state to work on
     * @param position the position of the cell
     * @param style the bg color of the cell
     */
    void SetCellBG(State &state, const Position position, const Color color);
    /**
     * Sets the fg color of a specific cell on the console window
     * @param state the console state to work on
     * @param position the position of the cell
     * @param style the fg color of the cell
     */
    void SetCellFG(State &state, const Position position, const Color color);
    /**
     * Fills a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param style the style of the cells
     */
    void FillCells(State &state, wchar_t value, const Position pos1, const Position pos2, const Style style = {});
    /**
     * Fills a range of cells on the console window given the top_left position and size.
     * Filling starts from (col, row) inclusive to (col+width, row+height) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos the top left corner
     * @param size the size of the range
     * @param style the style of the cells
     */
    void FillCells(State &state, wchar_t value, const Position pos, const Size size, const Style style = {});
    /**
     * Fills the background of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param color the color of the cells
     */
    void FillBackground(State &state, const Position pos1, const Position pos2, const Color color);
    /**
     * Fill the background of all cells of the selected pane
     * @param state the console state to work on
     * @param color the background color
     */
    void FillBackground(State &state, const Color color);
    /**
     * Fills the foreground of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param color the color of the cells
     */
    void FillBackground(State &state, const Position pos, const Size size, const Color color);
    /**
     * Fills the background of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param color the color of the cells
     */
    void FillForeground(State &state, const Position pos1, const Position pos2, const Color color);
    /**
     * Fills the foreground of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param color the color of the cells
     */
    void FillForeground(State &state, const Position pos, const Size size, const Color color);
    /**
     * Fill the foreground of all cells of the selected pane
     * @param state the console state to work on
     * @param color the foreground color
     */
    void FillForeground(State &state, const Color color);
    /**
     * Draws a line on the console window where \p start is the starting point and 
     * \p end is the ending point. Line is always drawn starting from \p start to \p end (exclusive).
     * @param state the console state to work on
     * @param start the starting point
     * @param end the ending point
     * @param fill the fill of the line
     * @param style the style of the line
     */
    void DrawLine(State &state, const Position start, const Position end, wchar_t fill, const Style style = {});

    void BeginPane(State &state, const Position top_left, const Size size);

    struct ScrollPaneInfo {
        Position pos = {};
        Size min_size = {};
        Size max_size = {};
        ScrollBar scroll_bar = SCROLL_DEFAULT;
        float scroll_factor = 1.0f;
        bool show_vscroll_bar = true;
        bool show_hscroll_bar = true;
        bool show_scroll_home = true;

        Handler<ScrollPaneInfo> on_vscroll = {};
        Handler<ScrollPaneInfo> on_hscroll = {};
    };

    void BeginScrollPane(State &state, Position &pivot, ScrollPaneInfo info);

    struct GridPaneInfo {
        Position pos = {};
        Size size = {};
        std::vector<double> col_sizes = {100};
        std::vector<double> row_sizes = {100};
    };

    void BeginGridPane(State &state, GridPaneInfo info);
    void BeginGridCell(State &state, size_t col, size_t row);

    void BeginNoPane(State &state);

    void EndPane(State &state);

    void DrawBorder(State &state, const Border &border = BORDER_DEFAULT);
    void DrawHDivider(State &state, size_t row, wchar_t fill = L'─', Style style = {});
    void DrawVDivider(State &state, size_t col, wchar_t fill = L'│', Style style = {});

    struct TextInfo {
        std::string text = {};
        Position pos = {};
        Style style = {};
        Handler<TextInfo> on_hover = {};
        Handler<TextInfo> on_click = {};
        Handler<TextInfo> on_click2 = {};
        Handler<TextInfo> on_menu = {};
    };

    /**
     * Displays text on the console window
     * @param state 
     * @param info 
     */
    void Text(State &state, TextInfo info);

    struct TextBoxInfo {
        std::string text = {};
        Position pos = {};
        Size size = {};
        Style style = {};
        bool wrap = true;
        Align align = Align::TOP_LEFT;

        Handler<TextBoxInfo> on_hover = {};
        Handler<TextBoxInfo> on_click = {};
        Handler<TextBoxInfo> on_click2 = {};
        Handler<TextBoxInfo> on_menu = {};
    };

    void TextBox(State &state, TextBoxInfo info);

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

        Handler<ProgressBarInfo> on_hover = {};
        Handler<ProgressBarInfo> on_click = {};
        Handler<ProgressBarInfo> on_click2 = {};
        Handler<ProgressBarInfo> on_menu = {};
    };

    void ProgressBar(State &state, ProgressBarInfo info);

    struct SimpleTableInfo {
        std::vector<std::string> data;
        bool include_header_row = true;
        size_t num_cols = 0;
        size_t num_rows = 0;

        Position pos;
        Style header_style = {.bg = COLOR_TEAL, .fg = COLOR_WHITE, .mode = STYLE_BOLD};
        Style table_style = {.bg = COLOR_NAVY, .fg = COLOR_SILVER};

        bool show_border = false;
        Border border = BORDER_DEFAULT;
    };

    void SimpleTable(State &state, SimpleTableInfo info);
}    // namespace nite

// --------------------------------
//  Event definitions
// --------------------------------

namespace nite
{
#define KEY_SHIFT ((uint8_t) (1 << 0))    // Shift key
#define KEY_CTRL  ((uint8_t) (1 << 1))    // Control on macOS, Ctrl on other platforms
#define KEY_ALT   ((uint8_t) (1 << 2))    // Option on macOS, Alt on other platforms
#define KEY_SUPER ((uint8_t) (1 << 3))    // Command on macOS, Win key on Windows, Super on other platforms
#define KEY_META  ((uint8_t) (1 << 4))    // Meta key

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
        MIDDLE,    // TODO: add middle button support for windows
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
    }

    bool PollEvent(State &state, Event &event);

    // Keyboard
    bool IsKeyPressed(const State &state, KeyCode key_code);
    bool IsKeyDown(const State &state, KeyCode key_code);
    bool IsKeyUp(const State &state, KeyCode key_code);

    // Mouse
    bool IsMouseClicked(const State &state, const MouseButton button);
    bool IsMouseDoubleClicked(const State &state, const MouseButton button);
    size_t GetMouseClickCount(const State &state, const MouseButton button);
    size_t GetMouseClick2Count(const State &state, const MouseButton button);
    Position GetMousePosition(const State &state);
    intmax_t GetMouseScrollV(const State &state);
    intmax_t GetMouseScrollH(const State &state);
}    // namespace nite