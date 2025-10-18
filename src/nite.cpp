#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cwchar>
#include <format>
#include <optional>
#include <queue>

#include "nite.hpp"

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#    define OS_WINDOWS
#endif

#define null (nullptr)

namespace nite
{
    struct State::StateImpl {
        bool closed = false;
        std::queue<internal::CellBuffer> swapchain;

        StateImpl() = default;

        bool set_cell(const size_t col, const size_t row, wchar_t value, const Style style);
    };

    Size GetWindowSize() {
        Size size;
        if (!internal::console::size(size.width, size.height))
            return Size();
        return size;
    }

    State &GetState() {
        static State state{std::make_unique<State::StateImpl>()};
        return state;
    }

    Size GetBufferSize(State &state) {
        return state.impl->swapchain.back().size();
    }

    bool ShouldWindowClose(State &state) {
        return state.impl->closed;
    }

    bool Initialize(State &state) {
        if (!internal::console::is_tty())
            return false;
        if (!internal::console::init())
            return false;

        state.impl->closed = false;
        return true;
    }

    bool Cleanup() {
        return internal::console::restore();
    }

    void BeginDrawing(State &state) {
        internal::CellBuffer buf(GetWindowSize());
        state.impl->swapchain.push(buf);
    }

    void EndDrawing(State &state) {
        switch (state.impl->swapchain.size()) {
        case 0:
            break;
        case 1: {
            const auto &cur_buf = state.impl->swapchain.front();
            const auto cur_size = cur_buf.size();

            for (size_t row = 0; row < cur_size.height; row++) {
                for (size_t col = 0; col < cur_size.width; col++) {
                    if (row == 16) {
                        (void) cur_size.width;
                    }
                    const auto &cell = cur_buf.at(col, row);
                    internal::console::set_cell(col, row, cell.value, cell.style);
                }
            }
            break;
        }
        default: {
            const auto prev_buf = std::move(state.impl->swapchain.front());
            const auto prev_size = prev_buf.size();
            state.impl->swapchain.pop();

            const auto &cur_buf = state.impl->swapchain.front();
            const auto cur_size = cur_buf.size();

            if (cur_size == prev_size) {
                for (size_t row = 0; row < cur_size.height; row++) {
                    for (size_t col = 0; col < cur_size.width; col++) {
                        const auto &cell = cur_buf.at(col, row);
                        const auto &prev_cell = prev_buf.at(col, row);
                        if (cell != prev_cell)
                            internal::console::set_cell(col, row, cell.value, cell.style);
                    }
                }
            } else {
                internal::console::clear();
                for (size_t row = 0; row < cur_size.height; row++) {
                    for (size_t col = 0; col < cur_size.width; col++) {
                        const auto &cell = cur_buf.at(col, row);
                        internal::console::set_cell(col, row, cell.value, cell.style);
                    }
                }
            }
            break;
        }
        }
    }

    void CloseWindow(State &state) {
        state.impl->closed = true;
    }

    void SetCell(State &state, wchar_t value, const Position position, const Style style) {
        state.impl->set_cell(position.col, position.row, value, style);
    }

    void FillCells(State &state, wchar_t value, const Position pos1, const Position pos2, const Style style) {
        const size_t col_start = std::min(pos1.col, pos2.col);
        const size_t col_end = std::max(pos1.col, pos2.col);
        const size_t row_start = std::min(pos1.row, pos2.row);
        const size_t row_end = std::max(pos1.row, pos2.row);

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                state.impl->set_cell(col, row, value, style);
    }

    void FillCells(State &state, wchar_t value, const Position pos, const Size size, const Style style) {
        const size_t col_start = pos.col;
        const size_t col_end = pos.col + size.width;
        const size_t row_start = pos.row;
        const size_t row_end = pos.col + size.height;

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                state.impl->set_cell(col, row, value, style);
    }

    void FillBackground(State &state, const Position pos1, const Position pos2, const Color color) {
        const size_t col_start = std::min(pos1.col, pos2.col);
        const size_t col_end = std::max(pos1.col, pos2.col);
        const size_t row_start = std::min(pos1.row, pos2.row);
        const size_t row_end = std::max(pos1.row, pos2.row);

        internal::CellBuffer &buffer = state.impl->swapchain.back();
        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                if (buffer.contains(col, row))
                    buffer.at(col, row).style.bg = color;
    }

    void FillBackground(State &state, const Position pos, const Size size, const Color color) {
        const size_t col_start = pos.col;
        const size_t col_end = pos.col + size.width;
        const size_t row_start = pos.row;
        const size_t row_end = pos.col + size.height;

        internal::CellBuffer &buffer = state.impl->swapchain.back();
        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                if (buffer.contains(col, row))
                    buffer.at(col, row).style.bg = color;
    }

    void FillForeground(State &state, const Position pos1, const Position pos2, const Color color) {
        const size_t col_start = std::min(pos1.col, pos2.col);
        const size_t col_end = std::max(pos1.col, pos2.col);
        const size_t row_start = std::min(pos1.row, pos2.row);
        const size_t row_end = std::max(pos1.row, pos2.row);

        internal::CellBuffer &buffer = state.impl->swapchain.back();
        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                if (buffer.contains(col, row))
                    buffer.at(col, row).style.fg = color;
    }

    void FillForeground(State &state, const Position pos, const Size size, const Color color) {
        const size_t col_start = pos.col;
        const size_t col_end = pos.col + size.width;
        const size_t row_start = pos.row;
        const size_t row_end = pos.col + size.height;

        internal::CellBuffer &buffer = state.impl->swapchain.back();
        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++)
                if (buffer.contains(col, row))
                    buffer.at(col, row).style.fg = color;
    }

    void DrawLine(State &state, const Position start, const Position end, wchar_t fill, const Style style) {
        const size_t col_start = start.col;
        const size_t col_end = end.col;
        const size_t row_start = start.row;
        const size_t row_end = end.row;

        if (col_start == col_end)
            for (size_t row = row_start; row < row_end; row++)
                state.impl->set_cell(col_start, row, fill, style);
        else {
            for (size_t col = col_start; col < col_end; col++) {
                const double row = std::lerp(row_start, row_end, (col - col_start) / std::abs(1. * col_end - col_start));
                state.impl->set_cell(col, row, fill, style);
            }
        }
    }

    void Text(State &state, TextInfo info) {
        for (size_t i = 0; char c: info.text) {
            state.impl->set_cell(info.pos.col + i, info.pos.row, c, info.style);
            i++;
        }
    }
}    // namespace nite

namespace nite
{
    State::State(std::unique_ptr<StateImpl> impl) : impl(std::move(impl)) {}

    bool State::StateImpl::set_cell(const size_t col, const size_t row, wchar_t value, const Style style) {
        internal::CellBuffer &buffer = swapchain.back();
        if (buffer.contains(col, row)) {
            internal::Cell &cell = buffer.at(col, row);
            cell.value = value;
            cell.style = style;
            return true;
        }
        return false;
    }
}    // namespace nite

namespace nite::internal::console
{
    void set_style(const Style style) {
        // Refer to https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
        if (style.mode & STYLE_RESET)
            print("\x1b[0m");
        if (style.mode & STYLE_BOLD)
            print("\x1b[1m");
        if (style.mode & STYLE_UNDERLINE)
            print("\x1b[4m");
        if (style.mode & STYLE_INVERSE)
            print("\x1b[7m");

        // Set foreground color
        auto r = std::to_string(style.fg.r);
        auto g = std::to_string(style.fg.g);
        auto b = std::to_string(style.fg.b);
        print("\x1b[38;2;" + r + ";" + g + ";" + b + "m");

        // Set background color
        r = std::to_string(style.bg.r);
        g = std::to_string(style.bg.g);
        b = std::to_string(style.bg.b);
        print("\x1b[48;2;" + r + ";" + g + ";" + b + "m");
    }

    void gotoxy(const size_t col, const size_t row) {
        print("\x1b[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H");
    }

    void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style) {
        gotoxy(col, row);
        set_style(style);
        std::fputwc(value, stdout);
    }
}    // namespace nite::internal::console

#ifdef OS_WINDOWS
#    include <windows.h>

namespace nite::internal::console
{
    static ConsoleError get_last_error() {
        LPVOID err_msg_buf;
        FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER              // Allocates a buffer for the message
                        | FORMAT_MESSAGE_FROM_SYSTEM        // Searches the system message table
                        | FORMAT_MESSAGE_IGNORE_INSERTS,    // Ignores insert sequences in the message definition.
                null,                                       // Handle to the module containing the message table
                GetLastError(),                             // Error code to format
                0,                                          // Default language
                reinterpret_cast<LPSTR>(&err_msg_buf),      // Output buffer for the formatted message
                0,                                          // Minimum size of the output buffer
                null
        );    // No arguments for insert sequences
        size_t size = std::strlen(static_cast<const char *>(err_msg_buf));
        // Build the string
        std::string msg_str{static_cast<char *>(err_msg_buf), size};
        // Free the old buffer
        LocalFree(err_msg_buf);
        return ConsoleError(msg_str);
    }

    bool print(const std::string &text) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!WriteConsole(h_con, text.c_str(), text.size(), null, null))
            return false;
        return true;
    }

    bool is_tty() {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        return GetConsoleMode(h_con, &mode);
    }

    bool size(size_t &width, size_t &height) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(h_con, &info))
            return false;

        const int columns = info.srWindow.Right - info.srWindow.Left + 1;
        const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        width = columns > 0 ? columns : 0;
        height = rows > 0 ? rows : 0;
        return true;
    }

    bool clear() {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);

        constexpr const COORD coord_screen = {0, 0};    // Top-left corner
        DWORD chars_written;
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        if (!GetConsoleScreenBufferInfo(h_con, &csbi))
            return false;
        const DWORD con_size = csbi.dwSize.X * csbi.dwSize.Y;
        // Fill the entire screen with blanks.
        if (!FillConsoleOutputCharacter(h_con, ' ', con_size, coord_screen, &chars_written))
            return false;
        // Get the current text attribute.
        if (!GetConsoleScreenBufferInfo(h_con, &csbi))
            return false;
        // Set the buffer attributes.
        if (!FillConsoleOutputAttribute(h_con, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, con_size, coord_screen, &chars_written))
            return false;
        // Put the cursor in the top left corner.
        if (!SetConsoleCursorPosition(h_con, coord_screen))
            return false;
        return true;
    }

    static DWORD old_in_mode = 0, old_out_mode = 0;
    static UINT old_console_cp = 0;

    bool init() {
        static const HANDLE h_conin = GetStdHandle(STD_INPUT_HANDLE);
        static const HANDLE h_conout = GetStdHandle(STD_OUTPUT_HANDLE);

        if (!GetConsoleMode(h_conin, &old_in_mode))
            return false;
        if (!GetConsoleMode(h_conout, &old_out_mode))
            return false;
        if ((old_console_cp = GetConsoleOutputCP()) == 0)
            return false;

        DWORD out_mode = 0;
        out_mode |= ENABLE_PROCESSED_OUTPUT;
        out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        out_mode |= DISABLE_NEWLINE_AUTO_RETURN;
        if (!SetConsoleMode(h_conout, out_mode))
            return false;

        DWORD in_mode = 0;
        in_mode |= ENABLE_EXTENDED_FLAGS;
        in_mode |= ENABLE_MOUSE_INPUT;
        in_mode |= ENABLE_WINDOW_INPUT;
        in_mode &= ~ENABLE_QUICK_EDIT_MODE;
        if (!SetConsoleMode(h_conin, in_mode)) {
            print(std::format("Invalid handle: {}\n", (h_conin == INVALID_HANDLE_VALUE)));
            print(get_last_error().what());
            return false;
        }

        if (!SetConsoleOutputCP(CP_UTF8))
            return false;

        print("\x1b[?1049h");    // Enter alternate buffer
        clear();
        return true;
    }

    bool restore() {
        if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), old_in_mode))
            return false;
        if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), old_out_mode))
            return false;
        if (!SetConsoleOutputCP(old_console_cp))
            return false;

        clear();
        print("\x1b[?1049l");    // Exit alternate buffer
        return true;
    }
}    // namespace nite::internal::console

#endif

#ifdef OS_WINDOWS
namespace nite
{
    static bool get_key_mod(WORD virtual_key_code, uint8_t &key_mod);
    static bool get_key_code(WORD virtual_key_code, char key_char, KeyCode &key_code);

    bool PollEvent(Event &event) {
        // Console input handle
        static const HANDLE h_conin = GetStdHandle(STD_INPUT_HANDLE);

        // Current active key modifiers
        static uint8_t cur_key_mod = 0;

        // Manage pending events
        static std::queue<Event> pending_events;
        if (!pending_events.empty()) {
            event = pending_events.front();
            pending_events.pop();
            return true;
        }

        // Get the number of available console input events
        DWORD event_count;
        if (!GetNumberOfConsoleInputEvents(h_conin, &event_count))
            return false;    // Call failed
        if (event_count == 0)
            return false;    // No event available

        // Read one console event
        INPUT_RECORD record;
        DWORD num_read;
        if (!ReadConsoleInput(h_conin, &record, 1, &num_read))
            return false;    // Call failed
        if (num_read != 1)
            return false;    // Failed to fetch one event

        switch (record.EventType) {
        case KEY_EVENT: {
            // Refer to: https://learn.microsoft.com/en-us/windows/console/key-event-record-str
            KEY_EVENT_RECORD info = record.Event.KeyEvent;
            if (KeyCode key_code; get_key_code(info.wVirtualKeyCode, info.uChar.AsciiChar, key_code)) {
                // Handle key modifiers
                uint8_t modifiers = 0;
                if (info.dwControlKeyState & SHIFT_PRESSED)
                    modifiers |= KEY_SHIFT;
                if (info.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                    modifiers |= KEY_CTRL;
                if (info.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
                    modifiers |= KEY_ALT;
                // Also append existing modifiers
                modifiers |= cur_key_mod;

                // Return the event
                event = KeyEvent{
                        .key_down = info.bKeyDown == TRUE,
                        .key_code = key_code,
                        .key_char = info.uChar.AsciiChar,
                        .modifiers = modifiers,
                };
                // Put the repeating ones in pending
                if (info.wRepeatCount > 1)
                    for (size_t i = 0; i < info.wRepeatCount - 1; i++) {
                        pending_events.push(event);
                    }
                return true;
            } else if (uint8_t key_mod; get_key_mod(info.wVirtualKeyCode, key_mod)) {
                if (info.bKeyDown)
                    // Set mod key state
                    cur_key_mod |= key_mod;
                else
                    // Unset mod key state
                    cur_key_mod &= ~key_mod;
                return false;    // Do not return mod key events
            } else
                return false;
        }
        case MOUSE_EVENT: {
            // Refer to: https://learn.microsoft.com/en-us/windows/console/mouse-event-record-str
            MOUSE_EVENT_RECORD info = record.Event.MouseEvent;

            static std::optional<Position> old_pos;
            Position pos{.x = static_cast<size_t>(info.dwMousePosition.X), .y = static_cast<size_t>(info.dwMousePosition.Y)};

            // Handle key modifiers
            uint8_t modifiers = 0;
            if (info.dwControlKeyState & SHIFT_PRESSED)
                modifiers |= KEY_SHIFT;
            if (info.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                modifiers |= KEY_CTRL;
            if (info.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
                modifiers |= KEY_ALT;
            // Also append existing modifiers
            modifiers |= cur_key_mod;

            // Find mouse event kind and mouse button
            MouseEventKind kind;
            MouseButton button = MouseButton::NONE;
            switch (info.dwEventFlags) {
            case 0:
            case DOUBLE_CLICK:
                kind = MouseEventKind::DOWN;
                if (info.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
                    button = MouseButton::LEFT;
                else if (info.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
                    button = MouseButton::RIGHT;
                break;
            case MOUSE_MOVED:
                kind = MouseEventKind::MOVED;
                if (!old_pos)
                    old_pos = pos;
                else if (*old_pos == pos)
                    return false;
                break;
            case MOUSE_WHEELED:
                if ((int16_t) (info.dwButtonState >> 8 * sizeof(WORD)) < 0)
                    kind = MouseEventKind::SCROLL_DOWN;
                else
                    kind = MouseEventKind::SCROLL_UP;
                break;
            case MOUSE_HWHEELED:
                if ((int16_t) (info.dwButtonState >> 8 * sizeof(WORD)) < 0)
                    kind = MouseEventKind::SCROLL_LEFT;
                else
                    kind = MouseEventKind::SCROLL_RIGHT;
                break;
            }

            MouseEvent mouse_event{
                    .kind = kind,
                    .button = button,
                    .pos = pos,
                    .modifiers = modifiers,
            };

            if (mouse_event.kind == MouseEventKind::DOWN) {
                MouseEvent new_event = mouse_event;
                new_event.kind = MouseEventKind::UP;
                pending_events.push(new_event);
            }
            event = mouse_event;
            return true;
        }
        case FOCUS_EVENT: {
            // Refer to: https://learn.microsoft.com/en-us/windows/console/focus-event-record-str
            FOCUS_EVENT_RECORD info = record.Event.FocusEvent;
            event = FocusEvent{
                    .focus_gained = info.bSetFocus == TRUE,
            };
            return true;
        }
        case WINDOW_BUFFER_SIZE_EVENT: {
            // Refer to: https://learn.microsoft.com/en-us/windows/console/window-buffer-size-record-str
            WINDOW_BUFFER_SIZE_RECORD info = record.Event.WindowBufferSizeEvent;
            event = ResizeEvent{
                    .size = Size{.width = static_cast<size_t>(info.dwSize.X), .height = static_cast<size_t>(info.dwSize.Y)}
            };
            return true;
        }
        default:
            return false;
        }
    }

    static bool get_key_mod(WORD virtual_key_code, uint8_t &key_mod) {
        switch (virtual_key_code) {
        case VK_LSHIFT:
        case VK_RSHIFT:
            key_mod = KEY_SHIFT;
            return true;
        case VK_LCONTROL:
        case VK_RCONTROL:
            key_mod = KEY_CTRL;
            return true;
        case VK_LMENU:
        case VK_RMENU:
            key_mod = KEY_ALT;
            return true;
        case VK_LWIN:
        case VK_RWIN:
            key_mod = KEY_SUPER;
            return true;
        default:
            return false;
        }
    }

    static inline constexpr bool is_print(char c) {
        return 32 <= c && c <= 126;
    }

    static bool get_key_code(WORD virtual_key_code, char key_char, KeyCode &key_code) {
        if (is_print(key_char)) {
            switch (key_char) {
            /* Alphabet keys */
            case 'a':
            case 'A':
                key_code = KeyCode::K_A;
                return true;
            case 'b':
            case 'B':
                key_code = KeyCode::K_B;
                return true;
            case 'c':
            case 'C':
                key_code = KeyCode::K_C;
                return true;
            case 'd':
            case 'D':
                key_code = KeyCode::K_D;
                return true;
            case 'e':
            case 'E':
                key_code = KeyCode::K_E;
                return true;
            case 'f':
            case 'F':
                key_code = KeyCode::K_F;
                return true;
            case 'g':
            case 'G':
                key_code = KeyCode::K_G;
                return true;
            case 'h':
            case 'H':
                key_code = KeyCode::K_H;
                return true;
            case 'i':
            case 'I':
                key_code = KeyCode::K_I;
                return true;
            case 'j':
            case 'J':
                key_code = KeyCode::K_J;
                return true;
            case 'k':
            case 'K':
                key_code = KeyCode::K_K;
                return true;
            case 'l':
            case 'L':
                key_code = KeyCode::K_L;
                return true;
            case 'm':
            case 'M':
                key_code = KeyCode::K_M;
                return true;
            case 'n':
            case 'N':
                key_code = KeyCode::K_N;
                return true;
            case 'o':
            case 'O':
                key_code = KeyCode::K_O;
                return true;
            case 'p':
            case 'P':
                key_code = KeyCode::K_P;
                return true;
            case 'q':
            case 'Q':
                key_code = KeyCode::K_Q;
                return true;
            case 'r':
            case 'R':
                key_code = KeyCode::K_R;
                return true;
            case 's':
            case 'S':
                key_code = KeyCode::K_S;
                return true;
            case 't':
            case 'T':
                key_code = KeyCode::K_T;
                return true;
            case 'u':
            case 'U':
                key_code = KeyCode::K_U;
                return true;
            case 'v':
            case 'V':
                key_code = KeyCode::K_V;
                return true;
            case 'w':
            case 'W':
                key_code = KeyCode::K_W;
                return true;
            case 'x':
            case 'X':
                key_code = KeyCode::K_X;
                return true;
            case 'y':
            case 'Y':
                key_code = KeyCode::K_Y;
                return true;
            case 'z':
            case 'Z':
                key_code = KeyCode::K_Z;
                return true;
            /* Number keys */
            case '0':
                key_code = KeyCode::K_0;
                return true;
            case '1':
                key_code = KeyCode::K_1;
                return true;
            case '2':
                key_code = KeyCode::K_2;
                return true;
            case '3':
                key_code = KeyCode::K_3;
                return true;
            case '4':
                key_code = KeyCode::K_4;
                return true;
            case '5':
                key_code = KeyCode::K_5;
                return true;
            case '6':
                key_code = KeyCode::K_6;
                return true;
            case '7':
                key_code = KeyCode::K_7;
                return true;
            case '8':
                key_code = KeyCode::K_8;
                return true;
            case '9':
                key_code = KeyCode::K_9;
                return true;
            /* Symbol keys */
            case '!':
                key_code = KeyCode::BANG;
                return true;
            case '@':
                key_code = KeyCode::AT;
                return true;
            case '#':
                key_code = KeyCode::HASH;
                return true;
            case '$':
                key_code = KeyCode::DOLLAR;
                return true;
            case '%':
                key_code = KeyCode::PERCENT;
                return true;
            case '^':
                key_code = KeyCode::CARET;
                return true;
            case '&':
                key_code = KeyCode::AMPERSAND;
                return true;
            case '*':
                key_code = KeyCode::ASTERISK;
                return true;
            case '(':
                key_code = KeyCode::LPAREN;
                return true;
            case ')':
                key_code = KeyCode::RPAREN;
                return true;
            case '{':
                key_code = KeyCode::LBRACE;
                return true;
            case '}':
                key_code = KeyCode::RBRACE;
                return true;
            case '[':
                key_code = KeyCode::LBRACKET;
                return true;
            case ']':
                key_code = KeyCode::RBRACKET;
                return true;
            case '~':
                key_code = KeyCode::TILDE;
                return true;
            case '`':
                key_code = KeyCode::BQUOTE;
                return true;
            case ':':
                key_code = KeyCode::COLON;
                return true;
            case ';':
                key_code = KeyCode::SEMICOLON;
                return true;
            case '"':
                key_code = KeyCode::DQUOTE;
                return true;
            case '\'':
                key_code = KeyCode::SQUOTE;
                return true;
            case '<':
                key_code = KeyCode::LESS;
                return true;
            case '>':
                key_code = KeyCode::GREATER;
                return true;
            case '?':
                key_code = KeyCode::HOOK;
                return true;
            case '/':
                key_code = KeyCode::SLASH;
                return true;
            case ',':
                key_code = KeyCode::COMMA;
                return true;
            case '.':
                key_code = KeyCode::PERIOD;
                return true;
            case '\\':
                key_code = KeyCode::BACKSLASH;
                return true;
            case '|':
                key_code = KeyCode::PIPE;
                return true;
            case '_':
                key_code = KeyCode::UNDERSCORE;
                return true;
            case '-':
                key_code = KeyCode::MINUS;
                return true;
            case '+':
                key_code = KeyCode::PLUS;
                return true;
            case '=':
                key_code = KeyCode::EQUAL;
                return true;
            case ' ':
                key_code = KeyCode::SPACE;
                return true;
            default:
                return false;
            }
        } else {
            switch (virtual_key_code) {
            case VK_NUMPAD0:
                key_code = KeyCode::K_0;
                return true;
            case VK_NUMPAD1:
                key_code = KeyCode::K_1;
                return true;
            case VK_NUMPAD2:
                key_code = KeyCode::K_2;
                return true;
            case VK_NUMPAD3:
                key_code = KeyCode::K_3;
                return true;
            case VK_NUMPAD4:
                key_code = KeyCode::K_4;
                return true;
            case VK_NUMPAD5:
                key_code = KeyCode::K_5;
                return true;
            case VK_NUMPAD6:
                key_code = KeyCode::K_6;
                return true;
            case VK_NUMPAD7:
                key_code = KeyCode::K_7;
                return true;
            case VK_NUMPAD8:
                key_code = KeyCode::K_8;
                return true;
            case VK_NUMPAD9:
                key_code = KeyCode::K_9;
                return true;
            case VK_F1:
                key_code = KeyCode::F1;
                return true;
            case VK_F2:
                key_code = KeyCode::F2;
                return true;
            case VK_F3:
                key_code = KeyCode::F3;
                return true;
            case VK_F4:
                key_code = KeyCode::F4;
                return true;
            case VK_F5:
                key_code = KeyCode::F5;
                return true;
            case VK_F6:
                key_code = KeyCode::F6;
                return true;
            case VK_F7:
                key_code = KeyCode::F7;
                return true;
            case VK_F8:
                key_code = KeyCode::F8;
                return true;
            case VK_F9:
                key_code = KeyCode::F9;
                return true;
            case VK_F10:
                key_code = KeyCode::F10;
                return true;
            case VK_F11:
                key_code = KeyCode::F11;
                return true;
            case VK_F12:
                key_code = KeyCode::F12;
                return true;
            case VK_F13:
                key_code = KeyCode::F13;
                return true;
            case VK_F14:
                key_code = KeyCode::F14;
                return true;
            case VK_F15:
                key_code = KeyCode::F15;
                return true;
            case VK_F16:
                key_code = KeyCode::F16;
                return true;
            case VK_F17:
                key_code = KeyCode::F17;
                return true;
            case VK_F18:
                key_code = KeyCode::F18;
                return true;
            case VK_F19:
                key_code = KeyCode::F19;
                return true;
            case VK_F20:
                key_code = KeyCode::F20;
                return true;
            case VK_F21:
                key_code = KeyCode::F21;
                return true;
            case VK_F22:
                key_code = KeyCode::F22;
                return true;
            case VK_F23:
                key_code = KeyCode::F23;
                return true;
            case VK_F24:
                key_code = KeyCode::F24;
                return true;
            case VK_BACK:
                key_code = KeyCode::BACKSPACE;
                return true;
            case VK_RETURN:
                key_code = KeyCode::ENTER;
                return true;
            case VK_LEFT:
                key_code = KeyCode::LEFT;
                return true;
            case VK_RIGHT:
                key_code = KeyCode::RIGHT;
                return true;
            case VK_UP:
                key_code = KeyCode::UP;
                return true;
            case VK_DOWN:
                key_code = KeyCode::DOWN;
                return true;
            case VK_HOME:
                key_code = KeyCode::HOME;
                return true;
            case VK_END:
                key_code = KeyCode::END;
                return true;
            case VK_PRIOR:
                key_code = KeyCode::PAGE_UP;
                return true;
            case VK_NEXT:
                key_code = KeyCode::PAGE_DOWN;
                return true;
            case VK_TAB:
                key_code = KeyCode::TAB;
                return true;
            case VK_INSERT:
                key_code = KeyCode::INSERT;
                return true;
            case VK_DELETE:
#    undef DELETE
                key_code = KeyCode::DELETE;
                return true;
            case VK_ESCAPE:
                key_code = KeyCode::ESCAPE;
                return true;
            default:
                return false;
            }
        }
    }
}    // namespace nite
#endif