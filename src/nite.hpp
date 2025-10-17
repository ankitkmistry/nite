#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <memory>
#include <variant>

// --------------------------------
//  Error definitions
// --------------------------------

namespace nite
{
    /**
     * The base error class
     */
    class NiteError : public std::runtime_error {
      public:
        explicit NiteError(const std::string &msg) : runtime_error(msg) {}
    };

    /**
     * Indicates out of range error
     */
    class RangeError : public NiteError {
      public:
        explicit RangeError() : NiteError("out of range") {}
    };

    /**
     * Indicates out of memory
     */
    class OutOfMemoryError : public NiteError {
      public:
        explicit OutOfMemoryError() : NiteError("out of memory") {}
    };

    /**
     * Indicates some type of console error
     */
    class ConsoleError : public NiteError {
      public:
        explicit ConsoleError(const std::string &msg) : NiteError(msg) {}
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

        constexpr Color inverse() const {
            return Color{.r = static_cast<uint8_t>(255 - r), .g = static_cast<uint8_t>(255 - g), .b = static_cast<uint8_t>(255 - b)};
        }

        constexpr bool operator==(const Color &other) const {
            return r == other.r && g == other.g && b == other.b;
        }

        constexpr bool operator!=(const Color &other) const {
            return !(*this == other);
        }
    };

    static const uint8_t STYLE_RESET = 0b0001;
    static const uint8_t STYLE_BOLD = 0b0010;
    static const uint8_t STYLE_UNDERLINE = 0b0100;
    static const uint8_t STYLE_INVERSE = 0b1000;

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

        constexpr Style inverse() const {
            return Style{.bg = bg.inverse(), .fg = fg.inverse(), .mode = mode};
        }

        constexpr bool operator==(const Style &other) const {
            return bg == other.bg && fg == other.fg && mode == other.mode;
        }

        constexpr bool operator!=(const Style &other) const {
            return !(*this == other);
        }
    };

    /**
     * Represents the position of an object
     */
    struct Position {
        union {
            struct {
                size_t x;
                size_t y;
            };

            struct {
                size_t col = 0;
                size_t row = 0;
            };
        };

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
            bool clear();
            bool size(size_t &width, size_t &height);
            bool print(const std::string &text);
            void set_style(const Style style);
            void gotoxy(const size_t col, const size_t row);
            void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style);

            bool init();
            bool restore();
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

            CellBuffer(const Size &size) : width(size.width), height(size.height) {
                cells = new Cell[size.width * size.height];
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

                const size_t new_length = width * height;
                delete[] cells;
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

            const Cell &at(const Position &pos) const {
                return cells[pos.row * width + pos.col];
            }

            Cell &at(size_t col, size_t row) {
                return cells[row * width + col];
            }

            Cell &at(const Position &pos) {
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
     * @return true if operation succeeded
     * @return false if operation failed
     */
    bool Initialize(State &state);
    /**
     * Cleanups the console and restores the terminal state
     * @param state the console state to work on
     * @return true if operation succeeded
     * @return false if operation failed
     */
    bool Cleanup();

    /**
     * Returns the size of the console screen buffer for the current frame
     * @param state the console state to work on
     * @return Size 
     */
    Size GetBufferSize(State &state);
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
     * Fills a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param style the style of the cell
     */
    void FillCells(State &state, wchar_t value, const Position pos1, const Position pos2, const Style style = {});
    /**
     * Fills a range of cells on the console window given the top_left position and size.
     * Filling starts from (col, row) inclusive to (col+width, row+height) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos the top left corner
     * @param size the size of the range
     * @param style the style of the cell
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
     * @param style the style of the cell
     */
    void FillBackground(State &state, const Position pos1, const Position pos2, const Color color);
    /**
     * Fills the foreground of a range of cells on the console window 
     * where \p pos1 and \p pos2 are diagonally opposite.
     * Filling starts from (col_min, row_min) inclusive to (col_min, row_min) exclusive.
     * @param state the console state to work on
     * @param value the cell text
     * @param pos1 the first position provided
     * @param pos2 the second position provided
     * @param style the style of the cell
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
     * @param style the style of the cell
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
     * @param style the style of the cell
     */
    void FillForeground(State &state, const Position pos, const Size size, const Color color);

    struct TextInfo {
        std::string text = "";
        Position pos = {};
        Size size = {};
        Style style = {};
    };

    /**
     * Displays text on the console window
     * @param state 
     * @param info 
     */
    void Text(State &state, TextInfo info = {});
}    // namespace nite

// --------------------------------
//  Event definitions
// --------------------------------

namespace nite
{
    constexpr const uint8_t KEY_SHIFT = 1 << 0;    // Shift key
    constexpr const uint8_t KEY_CTRL = 1 << 1;     // Control on macOS, Ctrl on other platforms
    constexpr const uint8_t KEY_ALT = 1 << 2;      // Option on macOS, Alt on other platforms
    constexpr const uint8_t KEY_SUPER = 1 << 3;    // Command on macOS, Win key on Windows, Super on other platforms
    constexpr const uint8_t KEY_META = 1 << 4;     // Meta key

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
        }
    };

    struct KeyEvent {
        bool key_down;
        KeyCode key_code;
        char key_char;
        uint8_t modifiers;
    };

    enum class MouseEventKind {
        DOWN,
        UP,
        MOVED,
        SCROLL_DOWN,
        SCROLL_UP,
        SCROLL_LEFT,
        SCROLL_RIGHT,
    };

    enum class MouseButton {
        NONE,
        LEFT,
        RIGHT,
    };

    struct MouseEvent {
        MouseEventKind kind;
        MouseButton button;
        Position pos;
        uint8_t modifiers;
    };

    struct FocusEvent {
        bool focus_gained;
    };

    struct ResizeEvent {
        Size size;
    };

    using Event = std::variant<KeyEvent, MouseEvent, FocusEvent, ResizeEvent>;

    bool PollEvent(Event &event);
}    // namespace nite