#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cwchar>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cwchar>

#include "nite.hpp"

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#    define OS_WINDOWS
#elif defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu_linux__)
#    define OS_LINUX
#else
#    error "Unsupported platform"
#endif

#ifdef OS_LINUX
#    include <cerrno>
#    include <charconv>
#    include <clocale>
#    include <concepts>
#    include <csignal>
#    include <cstring>
#endif

#define ESC                 "\033"
#define CSI                 ESC "["

#define NITE_DEFAULT_LOCALE "en_US.UTF-8"

#define $(expr)                                                                                                                                      \
    if (const auto result = (expr); !result)                                                                                                         \
    return result

namespace nite
{
    std::string wc_to_str(const wchar_t wc) {
        // Create conversion state
        mbstate_t state = {};
        // Create buffer
        std::vector<char> buf(MB_CUR_MAX);

#ifdef OS_WINDOWS
        // Convert
        size_t len;
        wcrtomb_s(&len, &buf[0], sizeof(buf), wc, &state);
        if (len == static_cast<size_t>(-1))
            return "";
        return std::string(&buf[0], len);
#else
        // Convert
        const size_t len = wcrtomb(&buf[0], wc, &state);
        if (len == static_cast<size_t>(-1))
            return "";
        return std::string(&buf[0], len);
#endif
    }

    // Convert a multibyte (narrow) string to a single wide character.
    // Returns L'\0' on empty input or on conversion error.
    wchar_t str_to_wc(const std::string &str) {
        if (str.empty())
            return L'\0';

        mbstate_t state = {};
        wchar_t wc;
        // mbrtowc converts the multibyte sequence to a wide char.
        // Provide str.size() + 1 so the terminating null can be considered.
        const size_t ret = mbrtowc(&wc, str.c_str(), str.size() + 1, &state);
        if (ret == static_cast<size_t>(-1) || ret == static_cast<size_t>(-2))
            return L'\0';
        return wc;
    }

    // Convert a single narrow char to a wide char using str_to_wc helper.
    wchar_t c_to_wc(const char c) {
        return str_to_wc(std::string(1, c));
    }

    // Convert a multibyte std::string to a std::wstring.
    // Uses the "secure" variants on Windows (mbsrtowcs_s) and the standard
    // mbsrtowcs on POSIX. Returns empty string on failure.
    std::wstring str_to_wstr(const std::string &str) {
        if (str.empty())
            return L"";
        mbstate_t state = {};
        const char *src = str.c_str();

#ifdef OS_WINDOWS
        // mbsrtowcs_s reports required number of wide chars (including null).
        size_t required = 0;
        const char *p = src;
        if (mbsrtowcs_s(&required, NULL, 0, &p, 0, &state) != 0 || required == 0)
            return L"";

        std::vector<wchar_t> buf(required);    // includes space for terminating null
        p = src;
        size_t converted = 0;
        if (mbsrtowcs_s(&converted, buf.data(), buf.size(), &p, buf.size(), &state) != 0)
            return L"";

        // converted includes terminating null, so subtract one for length
        const size_t len_without_null = converted > 0 ? converted - 1 : 0;
        return std::wstring(buf.data(), len_without_null);
#else
        // POSIX: first get required length (excluding terminating null)
        const size_t len = mbsrtowcs(NULL, &src, 0, &state);
        if (len == static_cast<size_t>(-1))
            return L"";

        std::vector<wchar_t> buf(len + 1);    // +1 for terminating null
        src = str.c_str();
        const size_t ret = mbsrtowcs(buf.data(), &src, buf.size(), &state);
        if (ret == static_cast<size_t>(-1))
            return L"";

        return std::wstring(buf.data(), ret);
#endif
    }

    // Convert a std::wstring to a multibyte std::string.
    // Uses wcsrtombs_s on Windows and wcsrtombs on POSIX. Returns empty string on failure.
    std::string wstr_to_str(const std::wstring &str) {
        if (str.empty())
            return "";
        mbstate_t state = {};
        const wchar_t *src = str.c_str();

#ifdef OS_WINDOWS
        // wcsrtombs_s reports required number of bytes (including null).
        size_t required = 0;
        const wchar_t *p = src;
        if (wcsrtombs_s(&required, NULL, 0, &p, 0, &state) != 0 || required == 0)
            return "";

        std::vector<char> buf(required);    // includes space for terminating null
        p = src;
        size_t converted = 0;
        if (wcsrtombs_s(&converted, buf.data(), buf.size(), &p, buf.size(), &state) != 0)
            return "";

        // converted includes terminating null, so subtract one for length
        const size_t len_without_null = converted > 0 ? converted - 1 : 0;
        return std::string(buf.data(), len_without_null);
#else
        // POSIX: first get required length (excluding terminating null)
        const size_t len = wcsrtombs(NULL, &src, 0, &state);
        if (len == static_cast<size_t>(-1))
            return "";

        std::vector<char> buf(len + 1);    // +1 for terminating null
        src = str.c_str();
        const size_t ret = wcsrtombs(buf.data(), &src, buf.size(), &state);
        if (ret == static_cast<size_t>(-1))
            return "";

        return std::string(buf.data(), ret);
#endif
    }
}    // namespace nite

static inline constexpr bool is_print(char c) {
    return 32 <= c && c <= 126;
}

// static std::string get_printable_str(const std::string &str) {
//     std::string result;
//     for (const char c: str) {
//         if (c == *ESC)
//             result += "ESC";
//         else
//             result.push_back(c);
//     }
//     return result;
// }

template<std::unsigned_integral Integer>
static inline constexpr Integer saturated_sub(Integer a, Integer b) {
    if (a >= b)
        return a - b;
    return 0;
}

template<std::unsigned_integral Integer>
static inline constexpr Integer saturated_add(Integer a, Integer b) {
    if (a <= std::numeric_limits<Integer>::max() - b)
        return a + b;
    return std::numeric_limits<Integer>::max();
}

namespace nite
{
    namespace internal
    {
        class Box {
          protected:
            Box() = default;

          public:
            virtual ~Box() = default;

            virtual void set_pos(const Position &p) = 0;
            virtual Position get_pos() const = 0;
            virtual void set_size(const Size &p) = 0;
            virtual Size get_size() const = 0;
            virtual bool contains(const size_t col, const size_t row) const = 0;
            virtual bool contains(const Position pos) const = 0;
            virtual bool transform(size_t &col, size_t &row) const = 0;
        };

        class NoBox : public Box {
          public:
            NoBox() = default;
            ~NoBox() = default;

            void set_pos(const Position &) {}

            Position get_pos() const {
                return {};
            }

            void set_size(const Size &) {}

            Size get_size() const {
                return {};
            }

            bool contains(const size_t, const size_t) const {
                return false;
            }

            bool contains(const Position) const {
                return false;
            }

            bool transform(size_t &, size_t &) const {
                return false;
            }
        };

        class StaticBox : public Box {
            Position pos;
            Size size;

          public:
            StaticBox(const Position &p, const Size &s) : pos(p), size(s) {}

            StaticBox() = default;
            ~StaticBox() = default;

            void set_pos(const Position &p) override {
                pos = p;
            }

            Position get_pos() const override {
                return pos;
            }

            void set_size(const Size &s) override {
                size = s;
            }

            Size get_size() const override {
                return size;
            }

            bool contains(const size_t col, const size_t row) const override {
                return pos.col <= col && col < pos.col + size.width && pos.row <= row && row < pos.row + size.height;
            }

            bool contains(const Position pos) const override {
                return contains(pos.col, pos.row);
            }

            bool transform(size_t &col, size_t &row) const override {
                col += pos.col;
                row += pos.row;
                return true;
            }
        };

        class ScrollBox : public Box {
            bool scroll_home;
            bool hscroll_bar;
            bool vscroll_bar;
            ScrollBar scroll_style;
            Position pos;
            Position pivot;
            Size min_size;
            Size max_size;

          public:
            ScrollBox(
                    bool show_scroll_home, bool show_hscroll_bar, bool show_vscroll_bar, const ScrollBar &scroll_style, const Position pos,
                    const Position pivot, const Size min_size, const Size max_size
            )
                : scroll_home(show_scroll_home),
                  hscroll_bar(show_hscroll_bar),
                  vscroll_bar(show_vscroll_bar),
                  scroll_style(scroll_style),
                  pos(pos),
                  pivot(pivot),
                  min_size(min_size),
                  max_size(max_size) {}

            ScrollBox() = default;
            ~ScrollBox() = default;

            bool show_scroll_home() const {
                return scroll_home;
            }

            bool show_hscroll_bar() const {
                return hscroll_bar;
            }

            bool show_vscroll_bar() const {
                return vscroll_bar;
            }

            const ScrollBar &get_scroll_style() const {
                return scroll_style;
            }

            Position get_pivot() const {
                return pivot;
            }

            Size get_min_size() const {
                return min_size;
            }

            Size get_max_size() const {
                return max_size;
            }

            void set_pos(const Position &p) override {
                pos = p;
            }

            Position get_pos() const override {
                return pos;
            }

            void set_size(const Size &s) override {
                min_size = s;
            }

            Size get_size() const override {
                return min_size;
            }

            bool contains(const size_t col, const size_t row) const override {
                return pos.col <= col && col < pos.col + min_size.width && pos.row <= row && row < pos.row + min_size.height;
            }

            bool contains(const Position pos) const override {
                return contains(pos.col, pos.row);
            }

            bool transform(size_t &col, size_t &row) const override {
                if (pivot.col <= col && col < pivot.col + min_size.width && pivot.row <= row && row < pivot.row + min_size.height) {
                    col = col + pos.col - pivot.col;
                    row = row + pos.row - pivot.row;
                    return true;
                }
                return false;
            }
        };

        class GridBox : public Box {
            Position pos;
            Size size;

            size_t num_cols;
            size_t num_rows;
            std::vector<StaticBox> grid;

          public:
            GridBox(const Position &p, const Size &s, size_t num_cols, size_t num_rows, const std::vector<StaticBox> &grid)
                : pos(p), size(s), num_cols(num_cols), num_rows(num_rows), grid(grid) {
                for (auto &box: this->grid) {
                    box.set_pos(p + box.get_pos());
                }
            }

            GridBox() = default;
            ~GridBox() = default;

            std::unique_ptr<internal::Box> get_grid_cell(size_t col, size_t row) {
                if (col >= num_cols || row >= num_rows)
                    return std::make_unique<internal::NoBox>();
                return std::make_unique<internal::StaticBox>(grid[row * num_cols + col]);
            }

            void set_pos(const Position &p) override {
                pos = p;
            }

            Position get_pos() const override {
                return pos;
            }

            void set_size(const Size &s) override {
                size = s;
            }

            Size get_size() const override {
                return size;
            }

            bool contains(const size_t col, const size_t row) const override {
                return pos.col <= col && col < pos.col + size.width && pos.row <= row && row < pos.row + size.height;
            }

            bool contains(const Position pos) const override {
                return contains(pos.col, pos.row);
            }

            bool transform(size_t &col, size_t &row) const override {
                col += pos.col;
                row += pos.row;
                return true;
            }
        };
    }    // namespace internal
}    // namespace nite

namespace nite
{
    struct KeyState {
        bool printable = false;
        bool down = false;
    };

    struct BtnState {
        size_t click1_count = 0;
        size_t click2_count = 0;
    };

    class State::StateImpl {
        bool closed = false;

        // Render mechanism
        std::chrono::duration<double> delta_time;
        std::queue<internal::CellBuffer> swapchain;
        std::vector<std::unique_ptr<internal::Box>> box_stack;

      public:
        // Events mechanism
        std::unordered_map<KeyCode, KeyState> key_states;

        std::array<BtnState, 4> btn_states;
        static_assert(4 == static_cast<int>(MouseButton::RIGHT) + 1, "Size of the array must match");

        Position mouse_pos;
        intmax_t mouse_scroll_v = 0;
        intmax_t mouse_scroll_h = 0;

      public:
        StateImpl() = default;

        bool is_closed() const {
            return closed;
        }

        void set_closed(bool b) {
            closed = b;
        }

        void push_buffer(const Size size) {
            swapchain.emplace(GetWindowSize());
            box_stack.clear();
            emplace_box<internal::StaticBox>(Position{}, size);
        }

        size_t get_swapchain_count() const {
            return swapchain.size();
        }

        internal::CellBuffer pop_current_buffer() {
            assert(!swapchain.empty() && "Swapchain cannot be empty");
            const internal::CellBuffer buf = swapchain.front();
            swapchain.pop();
            return buf;
        }

        internal::CellBuffer &get_current_buffer() {
            assert(!swapchain.empty() && "Swapchain cannot be empty");
            return swapchain.front();
        }

        const internal::CellBuffer &get_current_buffer() const {
            assert(!swapchain.empty() && "Swapchain cannot be empty");
            return swapchain.front();
        }

        template<typename BoxType, typename... BoxArgs>
            requires std::is_constructible_v<BoxType, BoxArgs...>
        void emplace_box(BoxArgs... args) {
            box_stack.push_back(std::make_unique<BoxType>(std::forward<BoxArgs>(args)...));
        }

        void push_box(std::unique_ptr<internal::Box> box) {
            if (box)
                box_stack.push_back(std::move(box));
            else
                emplace_box<internal::NoBox>();
        }

        void pop_box() {
            assert(!box_stack.empty() && "Box stack cannot be empty");
            box_stack.pop_back();
        }

        internal::Box &get_current_box() {
            assert(!box_stack.empty() && "Box stack cannot be empty");
            return *box_stack.back();
        }

        internal::Box &get_parent_box() {
            assert(box_stack.size() >= 2 && "Box stack size must be >= 2");
            return *box_stack[box_stack.size() - 2];
        }

        size_t box_stack_count() const {
            return box_stack.size();
        }

        std::chrono::duration<double> get_delta_time() const {
            return delta_time;
        }

        void set_delta_time(std::chrono::duration<double> time) {
            delta_time = time;
        }

        bool set_cell(size_t col, size_t row, wchar_t value, const Style style) {
            internal::Box &box = get_current_box();
            if (!box.transform(col, row))
                return false;
            if (!box.contains(col, row))
                return false;

            internal::CellBuffer &buffer = swapchain.back();
            if (!buffer.contains(col, row))
                return false;

            internal::Cell &cell = buffer.at(col, row);
            cell.value = value;
            if ((style.mode & STYLE_NO_FG) == 0)
                cell.style.fg = style.fg;
            if ((style.mode & STYLE_NO_BG) == 0)
                cell.style.bg = style.bg;
            // cell.style.mode = cell.style.mode;
            cell.style.mode = style.mode & ~(STYLE_NO_FG | STYLE_NO_BG);
            return true;
        }

        bool set_cell(size_t col, size_t row, const StyledChar st_char) {
            return set_cell(col, row, st_char.value, st_char.style);
        }

        internal::Cell &get_cell(size_t col, size_t row) {
            static internal::Cell sentinel;

            internal::Box &selected = get_current_box();
            col += selected.get_pos().col;
            row += selected.get_pos().row;

            internal::CellBuffer &buffer = swapchain.back();
            if (buffer.contains(col, row) && selected.contains(col, row))
                return buffer.at(col, row);
            return sentinel;
        }
    };

    State::State(std::unique_ptr<StateImpl> impl) : impl(std::move(impl)) {}

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

    Size GetBufferSize(const State &state) {
        return state.impl->get_current_buffer().size();
    }

    Position GetPanePosition(const State &state) {
        return state.impl->get_current_box().get_pos();
    }

    Size GetPaneSize(const State &state) {
        return state.impl->get_current_box().get_size();
    }

    double GetDeltaTime(const State &state) {
        return state.impl->get_delta_time().count();
    }

    bool ShouldWindowClose(const State &state) {
        return state.impl->is_closed();
    }

    Result Initialize(State &state) {
        if (!internal::console::is_tty())
            return Result::Error("cannot initialize in a non-terminal environment");
        if (const auto result = internal::console::init(); !result)
            return result;

        state.impl->set_closed(false);
        return Result::Ok;
    }

    Result Cleanup() {
        return internal::console::restore();
    }

    void BeginDrawing(State &state) {
        state.impl->push_buffer(GetWindowSize());
    }

    void EndDrawing(State &state) {
        state.impl->mouse_scroll_v = 0;
        state.impl->mouse_scroll_h = 0;
        for (BtnState &btn_state: state.impl->btn_states)
            btn_state = {};

        state.impl->pop_box();

        switch (state.impl->get_swapchain_count()) {
        case 0:
            break;
        case 1: {
            const auto start = std::chrono::high_resolution_clock::now();

            const auto &cur_buf = state.impl->get_current_buffer();
            const auto cur_size = cur_buf.size();

            for (size_t row = 0; row < cur_size.height; row++) {
                for (size_t col = 0; col < cur_size.width; col++) {
                    const auto &cell = cur_buf.at(col, row);
                    internal::console::set_cell(col, row, cell.value, cell.style);
                }
            }

            const auto end = std::chrono::high_resolution_clock::now();
            state.impl->set_delta_time(end - start);
            break;
        }
        default: {
            const auto start = std::chrono::high_resolution_clock::now();

            const auto prev_buf = state.impl->pop_current_buffer();
            const auto prev_size = prev_buf.size();

            const auto &cur_buf = state.impl->get_current_buffer();
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

            const auto end = std::chrono::high_resolution_clock::now();
            state.impl->set_delta_time(end - start);
            break;
        }
        }
    }

    void CloseWindow(State &state) {
        state.impl->set_closed(true);
    }

    void SetCell(State &state, wchar_t value, const Position position, const Style style) {
        state.impl->set_cell(position.col, position.row, value, style);
    }

    void SetCellStyle(State &state, const Position position, const Style style) {
        state.impl->get_cell(position.col, position.row).style = style;
    }

    void SetCellBG(State &state, const Position position, const Color color) {
        state.impl->get_cell(position.col, position.row).style.bg = color;
    }

    void SetCellFG(State &state, const Position position, const Color color) {
        state.impl->get_cell(position.col, position.row).style.fg = color;
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

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.bg = color;
            }
    }

    void FillBackground(State &state, const Color color) {
        internal::Box &selected = state.impl->get_current_box();

        for (size_t row = 0; row < selected.get_size().height; row++)
            for (size_t col = 0; col < selected.get_size().width; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.bg = color;
            }
    }

    void FillBackground(State &state, const Position pos, const Size size, const Color color) {
        const size_t col_start = pos.col;
        const size_t col_end = pos.col + size.width;
        const size_t row_start = pos.row;
        const size_t row_end = pos.col + size.height;

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.bg = color;
            }
    }

    void FillForeground(State &state, const Position pos1, const Position pos2, const Color color) {
        const size_t col_start = std::min(pos1.col, pos2.col);
        const size_t col_end = std::max(pos1.col, pos2.col);
        const size_t row_start = std::min(pos1.row, pos2.row);
        const size_t row_end = std::max(pos1.row, pos2.row);

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.fg = color;
            }
    }

    void FillForeground(State &state, const Position pos, const Size size, const Color color) {
        const size_t col_start = pos.col;
        const size_t col_end = pos.col + size.width;
        const size_t row_start = pos.row;
        const size_t row_end = pos.col + size.height;

        for (size_t row = row_start; row < row_end; row++)
            for (size_t col = col_start; col < col_end; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.fg = color;
            }
    }

    void FillForeground(State &state, const Color color) {
        internal::Box &selected = state.impl->get_current_box();

        for (size_t row = 0; row < selected.get_size().height; row++)
            for (size_t col = 0; col < selected.get_size().width; col++) {
                internal::Cell &cell = state.impl->get_cell(col, row);
                cell.style.fg = color;
            }
    }

    void DrawLine(State &state, const Position start, const Position end, wchar_t fill, const Style style) {
        const size_t col_start = start.col;
        const size_t col_end = end.col;
        const size_t row_start = start.row;
        const size_t row_end = end.row;

        if (col_start == col_end)
            for (size_t row = row_start; row < row_end; row++)
                state.impl->set_cell(col_start, row, fill, style);
        else if (row_start == row_end)
            for (size_t col = col_start; col < col_end; col++)
                state.impl->set_cell(col, row_start, fill, style);
        else
            for (size_t col = col_start; col < col_end; col++) {
                const double row = std::lerp(row_start, row_end, (col - col_start) / std::abs(1. * col_end - col_start));
                state.impl->set_cell(col, row, fill, style);
            }
    }

    void BeginPane(State &state, const Position top_left, const Size size) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_current_box()); no_box) {
            state.impl->emplace_box<internal::NoBox>(*no_box);
            return;
        }

        state.impl->emplace_box<internal::StaticBox>(GetPanePosition(state) + top_left, size);
    }

    static void scroll_vertical(Position &pivot, ScrollPaneInfo &info, int64_t value) {
        if (!info.show_vscroll_bar)
            return;

        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_vscroll) {
            const intmax_t abs_value = std::abs(value);
            for (intmax_t i = 0; i < abs_value; i++)
                info.on_vscroll(std::ref(info));
        }
        if (value > 0) {
            pivot.row += value;
        } else if (value < 0) {
            if (static_cast<size_t>(-value) <= pivot.row)
                pivot.row += value;
            else
                pivot.row = 0;
        }
        if (pivot.row >= info.max_size.height - info.min_size.height)
            pivot.row = info.max_size.height - info.min_size.height - 1;
    }

    static void scroll_horizontal(Position &pivot, ScrollPaneInfo &info, int64_t value) {
        if (!info.show_hscroll_bar)
            return;

        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_hscroll) {
            const intmax_t abs_value = std::abs(value);
            for (intmax_t i = 0; i < abs_value; i++)
                info.on_hscroll(std::ref(info));
        }
        if (value > 0) {
            pivot.col += value;
        } else if (value < 0) {
            if (static_cast<size_t>(-value) <= pivot.col)
                pivot.col += value;
            else
                pivot.col = 0;
        }
        if (pivot.col >= info.max_size.width - info.min_size.width)
            pivot.col = info.max_size.width - info.min_size.width - 1;
    }

    void BeginScrollPane(State &state, ScrollState &scroll_state, ScrollPaneInfo info) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_current_box()); no_box) {
            state.impl->emplace_box<internal::NoBox>(*no_box);
            return;
        }

        const auto left_scroll_btn = Position{.col = 0, .row = info.min_size.height - 1} + info.pos;
        const auto right_scroll_btn = Position{.col = info.min_size.width - 2, .row = info.min_size.height - 1} + info.pos;
        const auto top_scroll_btn = Position{.col = info.min_size.width - 1, .row = 0} + info.pos;
        const auto bottom_scroll_btn = Position{.col = info.min_size.width - 1, .row = info.min_size.height - 2} + info.pos;
        const auto home_cell_btn = Position{.col = info.min_size.width - 1, .row = info.min_size.height - 1} + info.pos;

        int64_t vscroll_count = 0;
        int64_t hscroll_count = 0;

        for (const Event &event: scroll_state.get_captured_events()) {
            HandleEvent(event, [&](const MouseEvent &ev) {
                switch (ev.kind) {
                case MouseEventKind::CLICK:
                case MouseEventKind::DOUBLE_CLICK:
                    if (ev.button != MouseButton::LEFT)
                        break;
                    if (ev.pos - GetPanePosition(state) == left_scroll_btn)
                        hscroll_count--;
                    if (ev.pos - GetPanePosition(state) == right_scroll_btn)
                        hscroll_count++;
                    if (ev.pos - GetPanePosition(state) == top_scroll_btn)
                        vscroll_count--;
                    if (ev.pos - GetPanePosition(state) == bottom_scroll_btn)
                        vscroll_count++;
                    if (ev.pos - GetPanePosition(state) == home_cell_btn)
                        scroll_state.set_pivot({});
                    break;
                case MouseEventKind::SCROLL_DOWN:
                    if (internal::StaticBox(GetPanePosition(state) + info.pos, info.min_size).contains(ev.pos))
                        vscroll_count++;
                    break;
                case MouseEventKind::SCROLL_UP:
                    if (internal::StaticBox(GetPanePosition(state) + info.pos, info.min_size).contains(ev.pos))
                        vscroll_count--;
                    break;
                case MouseEventKind::SCROLL_LEFT:
                    if (internal::StaticBox(GetPanePosition(state) + info.pos, info.min_size).contains(ev.pos))
                        hscroll_count--;
                    break;
                case MouseEventKind::SCROLL_RIGHT:
                    if (internal::StaticBox(GetPanePosition(state) + info.pos, info.min_size).contains(ev.pos))
                        hscroll_count++;
                    break;
                case MouseEventKind::MOVED:
                    break;
                }
            });
        }

        scroll_horizontal(scroll_state.get_pivot(), info, hscroll_count);
        scroll_vertical(scroll_state.get_pivot(), info, vscroll_count);

        state.impl->emplace_box<internal::ScrollBox>(
                info.show_scroll_home, info.show_hscroll_bar, info.show_vscroll_bar, info.scroll_bar, GetPanePosition(state) + info.pos,
                scroll_state.get_pivot(), info.min_size, info.max_size
        );

        scroll_state.clear_captured_events();
    }

    void BeginGridPane(State &state, GridPaneInfo info) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_current_box()); no_box) {
            state.impl->emplace_box<internal::NoBox>(*no_box);
            return;
        }

        std::vector<internal::StaticBox> grid;
        const size_t num_rows = info.row_sizes.size();
        const size_t num_cols = info.col_sizes.size();

        double row_progress = 0.0;
        for (size_t row = 0; row < num_rows; row++) {
            double col_progress = 0.0;
            for (size_t col = 0; col < num_cols; col++) {
                Size s = {
                        .width = static_cast<size_t>(info.col_sizes[col] / 100.0 * info.size.width),
                        .height = static_cast<size_t>(info.row_sizes[row] / 100.0 * info.size.height),
                };
                Position p = {
                        .col = static_cast<size_t>(col_progress * info.size.width),
                        .row = static_cast<size_t>(row_progress * info.size.height),
                };
                grid.emplace_back(p, s);
                col_progress += info.col_sizes[col] / 100.0;
            }
            row_progress += info.row_sizes[row] / 100.0;
        }

        state.impl->emplace_box<internal::GridBox>(GetPanePosition(state) + info.pos, info.size, num_cols, num_rows, grid);
    }

    void BeginGridCell(State &state, size_t col, size_t row) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_current_box()); no_box) {
            state.impl->emplace_box<internal::NoBox>(*no_box);
            return;
        }
        if (const auto grid_box = dynamic_cast<internal::GridBox *>(&state.impl->get_current_box()); grid_box)
            state.impl->push_box(grid_box->get_grid_cell(col, row));
        else
            state.impl->emplace_box<internal::NoBox>();
    }

    void BeginNoPane(State &state) {
        state.impl->emplace_box<internal::NoBox>();
    }

    void EndPane(State &state) {
        const internal::Box &box = state.impl->get_current_box();
        if (const auto scroll_box = dynamic_cast<const internal::ScrollBox *>(&box); scroll_box) {
            const auto &scroll = scroll_box->get_scroll_style();
            const auto max_size = scroll_box->get_max_size();
            const auto min_size = scroll_box->get_min_size();
            const auto pivot = scroll_box->get_pivot();

            // Scroll home button
            if (scroll_box->show_scroll_home()) {
                const auto home_cell = Position{.col = min_size.width - 1, .row = min_size.height - 1} + pivot;
                SetCell(state, scroll.home.value, home_cell, scroll.home.style);
            }
            // Vertical scroll
            if (scroll_box->show_vscroll_bar() && min_size.height < max_size.height) {
                const auto vscroll_start = Position{.col = min_size.width - 1, .row = 1} + pivot;
                const auto vscroll_end = Position{.col = min_size.width - 1, .row = min_size.height - 2} + pivot;
                DrawLine(state, vscroll_start, vscroll_end, scroll.v_bar.value, scroll.v_bar.style);

                const auto top_scroll_btn = Position{.col = min_size.width - 1, .row = 0} + pivot;
                const auto bottom_scroll_btn = Position{.col = min_size.width - 1, .row = min_size.height - 2} + pivot;
                SetCell(state, scroll.top.value, top_scroll_btn, scroll.top.style);
                SetCell(state, scroll.bottom.value, bottom_scroll_btn, scroll.bottom.style);

                const size_t row = static_cast<double>(pivot.row) / max_size.height * (vscroll_end.row - vscroll_start.row);
                const auto node_start = Position{.col = min_size.width - 1, .row = row + 1} + pivot;
                const size_t max_row = (double) (max_size.height - min_size.height - 1) / max_size.height * (vscroll_end.row - vscroll_start.row);
                const size_t node_height = (vscroll_end.row - vscroll_start.row) - max_row;
                DrawLine(state, node_start, {.col = node_start.col, .row = node_start.row + node_height}, scroll.v_node.value, scroll.v_node.style);
            }
            // Horizontal scroll
            if (scroll_box->show_hscroll_bar() && min_size.width < max_size.width) {
                const auto hscroll_start = Position{.col = 1, .row = min_size.height - 1} + pivot;
                const auto hscroll_end = Position{.col = min_size.width - 2, .row = min_size.height - 1} + pivot;
                DrawLine(state, hscroll_start, hscroll_end, scroll.h_bar.value, scroll.h_bar.style);

                const auto left_scroll_btn = Position{.col = 0, .row = min_size.height - 1} + pivot;
                const auto right_scroll_btn = Position{.col = min_size.width - 2, .row = min_size.height - 1} + pivot;
                SetCell(state, scroll.left.value, left_scroll_btn, scroll.left.style);
                SetCell(state, scroll.right.value, right_scroll_btn, scroll.right.style);

                const size_t col = static_cast<double>(pivot.col) / max_size.width * (hscroll_end.col - hscroll_start.col);
                const auto node_start = Position{.col = col + 1, .row = min_size.height - 1} + pivot;
                const size_t max_col = (double) (max_size.width - min_size.width - 1) / max_size.width * (hscroll_end.col - hscroll_start.col);
                const size_t node_width = (hscroll_end.col - hscroll_start.col) - max_col;
                DrawLine(state, node_start, {.col = node_start.col + node_width, .row = node_start.row}, scroll.h_node.value, scroll.h_node.style);
            }
        }
        state.impl->pop_box();
    }

    void BeginBorder(State &state, const BoxBorder &border) {
        const internal::Box &selected = state.impl->get_current_box();

        if (border.top_left.value != '\0')
            SetCell(state, border.top_left.value, {.col = 0, .row = 0}, border.top_left.style);
        if (border.top_right.value != '\0')
            SetCell(state, border.top_right.value, {.col = selected.get_size().width - 1, .row = 0}, border.top_right.style);
        if (border.bottom_left.value != '\0')
            SetCell(state, border.bottom_left.value, {.col = 0, .row = selected.get_size().height - 1}, border.bottom_left.style);
        if (border.bottom_right.value != '\0')
            SetCell(state, border.bottom_right.value, {.col = selected.get_size().width - 1, .row = selected.get_size().height - 1},
                    border.bottom_right.style);

        for (size_t row = 1; row < selected.get_size().height - 1; row++) {
            if (border.left.value != '\0')
                SetCell(state, border.left.value, {.col = 0, .row = row}, border.left.style);
            if (border.right.value != '\0')
                SetCell(state, border.right.value, {.col = selected.get_size().width - 1, .row = row}, border.right.style);
        }

        for (size_t col = 1; col < selected.get_size().width - 1; col++) {
            if (border.top.value != '\0')
                SetCell(state, border.top.value, {.col = col, .row = 0}, border.top.style);
            if (border.bottom.value != '\0')
                SetCell(state, border.bottom.value, {.col = col, .row = selected.get_size().height - 1}, border.bottom.style);
        }
    }

    void EndBorder(State &state) {
        internal::Box &selected = state.impl->get_current_box();

        // Change this for convenience
        const size_t x = selected.get_pos().x;
        const size_t y = selected.get_pos().y;
        const size_t width = selected.get_size().width;
        const size_t height = selected.get_size().height;

        selected.set_pos({.x = x + 1, .y = y + 1});
        selected.set_size({.width = width - 2, .height = height - 2});
    }

    void DrawBorder(State &state, const BoxBorder &border, const std::string &text) {
        BeginBorder(state, border);
        if (!text.empty()) {
            // clang-format off
            Text(state, {
                .text = text,
                .pos = {.col = 1, .row = 0},
                .style = border.top.style,
            });
            // clang-format on
        }
        EndBorder(state);
    }

    Position GetAlignedPos(State &state, const Size size, const Align align) {
        auto &current = state.impl->get_current_box();

        size_t col_start = 0;
        size_t row_start = 0;

        switch (align) {
        case Align::TOP_LEFT:
            col_start = 0;
            row_start = 0;
            break;
        case Align::TOP:
            col_start = (current.get_size().width - size.width) / 2;
            row_start = 0;
            break;
        case Align::TOP_RIGHT:
            col_start = current.get_size().width - size.width;
            row_start = 0;
            break;
        case Align::LEFT:
            col_start = 0;
            row_start = (current.get_size().height - size.height) / 2;
            break;
        case Align::CENTER:
            col_start = (current.get_size().width - size.width) / 2;
            row_start = (current.get_size().height - size.height) / 2;
            break;
        case Align::RIGHT:
            col_start = current.get_size().width - size.width;
            row_start = (current.get_size().height - size.height) / 2;
            break;
        case Align::BOTTOM_LEFT:
            col_start = 0;
            row_start = current.get_size().height - size.height;
            break;
        case Align::BOTTOM:
            col_start = (current.get_size().width - size.width) / 2;
            row_start = current.get_size().height - size.height;
            break;
        case Align::BOTTOM_RIGHT:
            col_start = current.get_size().width - size.width;
            row_start = current.get_size().height - size.height;
            break;
        }
        return current.get_pos() + Position{.col = col_start, .row = row_start};
    }

    void DrawHDivider(State &state, size_t row, wchar_t value, Style style) {
        DrawLine(state, {.col = 0, .row = row}, {.col = state.impl->get_current_box().get_size().width, .row = row}, value, style);
    }

    void DrawVDivider(State &state, size_t col, wchar_t value, Style style) {
        DrawLine(state, {.col = col, .row = 0}, {.col = col, .row = state.impl->get_current_box().get_size().height}, value, style);
    }

    void Text(State &state, TextInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.text.size(), .height = 1}).contains(GetMouseRelPos(state))) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::ref(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::ref(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::ref(info));
            } else if (info.on_hover)
                info.on_hover(std::ref(info));
        }

        for (size_t i = 0; char c: info.text) {
            state.impl->set_cell(info.pos.col + i, info.pos.row, c, info.style);
            i++;
        }
    }

    void RichText(State &state, RichTextInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.text.size(), .height = 1}).contains(GetMouseRelPos(state))) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::ref(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::ref(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::ref(info));
            } else if (info.on_hover)
                info.on_hover(std::ref(info));
        }

        for (size_t i = 0; StyledChar st_char: info.text) {
            state.impl->set_cell(info.pos.col + i, info.pos.row, st_char.value, st_char.style);
            i++;
        }
    }

    void TextBox(State &state, TextBoxInfo info) {
        if (internal::StaticBox(info.pos, info.size).contains(GetMouseRelPos(state))) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::ref(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::ref(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::ref(info));
            } else if (info.on_hover)
                info.on_hover(std::ref(info));
        }

        std::vector<std::string> lines;
        // Split lines
        for (size_t start = 0, i = 0; i <= info.text.size(); i++) {
            if (i == info.text.size() || info.text[i] == '\n') {
                auto line = info.text.substr(start, i - start);
                if (info.wrap)
                    for (size_t i = 0; i < line.size(); i += info.size.width)
                        lines.push_back(line.substr(i, info.size.width));
                else
                    lines.push_back(std::move(line));
                start = i + 1;
            }
        }

        BeginPane(state, info.pos, info.size);

        switch (info.align) {
        case Align::TOP_LEFT:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::TOP:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::TOP_RIGHT:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::LEFT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::CENTER:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::RIGHT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM_LEFT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM_RIGHT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start], info.style);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        }

        EndPane(state);
    }

    void RichTextBox(State &state, RichTextBoxInfo info) {
        if (internal::StaticBox(info.pos, info.size).contains(GetMouseRelPos(state))) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::ref(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::ref(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::ref(info));
            } else if (info.on_hover)
                info.on_hover(std::ref(info));
        }

        std::vector<std::vector<StyledChar>> lines;
        for (size_t start = 0, i = 0; i <= info.text.size(); i++) {
            if (i == info.text.size() || info.text[i].value == '\n') {
                if (start == i) {
                    if (i != info.text.size())
                        lines.emplace_back();
                } else {
                    std::vector<StyledChar> line(info.text.begin() + start, info.text.begin() + i);
                    if (info.wrap) {
                        for (size_t i = 0; i < line.size(); i += info.size.width)
                            lines.emplace_back(line.begin() + i, line.begin() + std::min(line.size(), i + info.size.width));
                    } else
                        lines.push_back(std::move(line));
                }
                start = i + 1;
            }
        }

        BeginPane(state, info.pos, info.size);

        switch (info.align) {
        case Align::TOP_LEFT:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::TOP:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::TOP_RIGHT:
            for (size_t row = 0; row < info.size.height; row++)
                if (row < lines.size()) {
                    const auto &line = lines[row];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            break;
        case Align::LEFT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::CENTER:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::RIGHT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = (info.size.height - lines.size()) / 2;
                const auto row_end = row_start + lines.size();
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM_LEFT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col < line.size())
                            state.impl->set_cell(col, row, line[col]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = (info.size.width - line.size()) / 2;
                    const auto col_end = col_start + line.size();
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        case Align::BOTTOM_RIGHT:
            for (size_t row = 0; row < info.size.height; row++) {
                const auto row_start = info.size.height - lines.size();
                const auto row_end = info.size.height;
                if (row_start <= row && row < row_end) {
                    const auto &line = lines[row - row_start];
                    const auto col_start = info.size.width - line.size();
                    const auto col_end = info.size.width;
                    for (size_t col = 0; col < info.size.width; col++)
                        if (col_start <= col && col < col_end)
                            state.impl->set_cell(col, row, line[col - col_start]);
                        else
                            state.impl->set_cell(col, row, ' ', info.style);
                } else
                    for (size_t col = 0; col < info.size.width; col++)
                        state.impl->set_cell(col, row, ' ', info.style);
            }
            break;
        }

        EndPane(state);
    }

    void ProgressBar(State &state, ProgressBarInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.length, .height = 1}).contains(GetMouseRelPos(state))) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::ref(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::ref(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::ref(info));
            } else if (info.on_hover)
                info.on_hover(std::ref(info));
        }

        double value = info.value;
        if (value < 0)
            value = 0;
        if (value > 1)
            value = 1;

        size_t req = value * (info.length * info.motion.size());
        size_t col = 0;
        while (req > 0) {
            if (req >= info.motion.size()) {
                state.impl->set_cell(info.pos.col + col, info.pos.row, info.motion.back().value, info.motion.back().style);
                req -= info.motion.size();
                col++;
            } else {
                state.impl->set_cell(info.pos.col + col, info.pos.row, info.motion[req - 1].value, info.motion[req - 1].style);
                req = 0;
                col++;
            }
        }

        for (; col < info.length; col++)
            state.impl->set_cell(info.pos.col + col, info.pos.row, ' ', info.style);
    }

    Size SimpleTable(State &state, SimpleTableInfo info) {
        Style header_style1 = info.header_style;
        Style header_style2;
        header_style2.bg = Color::from_hex(saturated_add(header_style1.bg.get_hex(), 0x151515u));
        header_style2.fg = header_style1.fg;
        header_style2.mode = header_style1.mode;

        Style table_style1 = info.table_style;
        Style table_style2;
        table_style2.bg = Color::from_hex(saturated_add(table_style1.bg.get_hex(), 0x151515u));
        table_style2.fg = table_style1.fg;
        table_style2.mode = table_style1.mode;

        size_t total_width = 0;
        std::vector<size_t> max_col_widths(info.num_cols);
        for (size_t col = 0; col < info.num_cols; col++) {
            size_t max = 0;
            for (size_t row = 0; row < info.num_rows; row++) {
                const auto &cell_text = (row * info.num_cols + col) >= info.data.size() ? "" : info.data[row * info.num_cols + col];
                if (cell_text.size() + 1 >= max)
                    max = cell_text.size() + 1;
            }
            total_width += max_col_widths[col] = max;
        }

        if (info.show_border) {
            // Data:
            //  | a   | b    | c     |
            //  | a   | b    | c     |
            //  | a   | b    | c     |
            //  | a   | b    | c     |
            //
            //  Output:
            //
            //  +-----+------+-------+
            //  | a   | b    | c     |
            //  +-----+------+-------+
            //  | a   | b    | c     |
            //  | a   | b    | c     |
            //  | a   | b    | c     |
            //  +-----+------+-------+
            //
            Size size;
            if (info.include_header_row)
                size = {.width = total_width + max_col_widths.size() + 1, .height = info.num_rows + 3};
            else
                size = {.width = total_width + max_col_widths.size() + 1, .height = info.num_rows + 2};

            BeginPane(state, info.pos, size);
            {
                const auto last_row = info.include_header_row ? info.num_rows + 2 : info.num_rows + 1;

                state.impl->set_cell(0, 0, info.border.top_left.value, info.border.top_left.style);
                if (info.include_header_row)
                    state.impl->set_cell(0, 2, info.border.left_joint.value, info.border.left_joint.style);
                state.impl->set_cell(0, last_row, info.border.bottom_left.value, info.border.bottom_left.style);

                size_t col = 1;
                for (const size_t col_width: max_col_widths) {
                    DrawLine(
                            state, {.col = col, .row = 0}, {.col = col + col_width, .row = 0}, info.border.horizontal.value,
                            info.border.horizontal.style
                    );
                    state.impl->set_cell(col + col_width, 0, info.border.top_joint.value, info.border.top_joint.style);

                    if (info.include_header_row) {
                        DrawLine(
                                state, {.col = col, .row = 2}, {.col = col + col_width, .row = 2}, info.border.horizontal.value,
                                info.border.horizontal.style
                        );
                        state.impl->set_cell(col + col_width, 2, info.border.center_joint.value, info.border.center_joint.style);
                    }

                    DrawLine(
                            state, {.col = col, .row = last_row}, {.col = col + col_width, .row = last_row}, info.border.horizontal.value,
                            info.border.horizontal.style
                    );
                    state.impl->set_cell(col + col_width, last_row, info.border.bottom_joint.value, info.border.bottom_joint.style);

                    col += col_width + 1;
                }
                col--;
                state.impl->set_cell(col, 0, info.border.top_right.value, info.border.top_right.style);
                if (info.include_header_row)
                    state.impl->set_cell(col, 2, info.border.right_joint.value, info.border.right_joint.style);
                state.impl->set_cell(col, last_row, info.border.bottom_right.value, info.border.bottom_right.style);
            }

            size_t y = 1;
            for (size_t row = 0; row < info.num_rows; row++) {
                state.impl->set_cell(0, y, info.border.vertical.value, info.border.vertical.style);
                size_t x = 1;
                for (size_t col = 0; col < info.num_cols; col++) {
                    const auto &cell_text = (row * info.num_cols + col) >= info.data.size() ? "" : info.data[row * info.num_cols + col];
                    Style style;
                    if (info.include_header_row) {
                        if (row == 0)
                            style = col % 2 == 0 ? header_style2 : header_style1;
                        else
                            style = (row + col) % 2 == 0 ? table_style2 : table_style1;
                    } else
                        style = (row + col) % 2 == 0 ? table_style2 : table_style1;
                    // clang-format off
                    TextBox(state, {
                        .text = cell_text,
                        .pos = {.x = x, .y = y},
                        .size = {.width = max_col_widths[col], .height = 1},
                        .style = style,
                    });
                    // clang-format on
                    x += max_col_widths[col];
                    // Place the separaator
                    state.impl->set_cell(x, y, info.border.vertical.value, info.border.vertical.style);
                    x++;
                }
                // Make space for header row
                if (y == 1 && info.include_header_row)
                    y = 3;
                else
                    y++;
            }
            EndPane(state);
            return size;
        } else {
            Size size = {.width = total_width, .height = info.num_rows};
            BeginPane(state, info.pos, size);
            size_t y = 0;
            for (size_t row = 0; row < info.num_rows; row++) {
                size_t x = 0;
                for (size_t col = 0; col < info.num_cols; col++) {
                    const auto &cell_text = (row * info.num_cols + col) >= info.data.size() ? "" : info.data[row * info.num_cols + col];
                    Style style;
                    if (info.include_header_row) {
                        if (row == 0)
                            style = col % 2 == 0 ? header_style2 : header_style1;
                        else
                            style = (row + col) % 2 == 0 ? table_style2 : table_style1;
                    } else
                        style = (row + col) % 2 == 0 ? table_style2 : table_style1;
                    // clang-format off
                    TextBox(state, {
                        .text = cell_text,
                        .pos = {.x = x, .y = y},
                        .size = {.width = max_col_widths[col], .height = 1},
                        .style = style,
                    });
                    // clang-format on
                    x += max_col_widths[col];
                }
                y++;
            }
            EndPane(state);
            return size;
        }
    }

    static void format_styled_text(std::vector<StyledChar> &list, const char c, const Style style) {
        std::string str;
        switch (c) {
        case '\x00':
            str = "<NUL>";
            break;
        case '\x01':
            str = "<SOH>";
            break;
        case '\x02':
            str = "<STX>";
            break;
        case '\x03':
            str = "<ETX>";
            break;
        case '\x04':
            str = "<EOT>";
            break;
        case '\x05':
            str = "<ENQ>";
            break;
        case '\x06':
            str = "<ACK>";
            break;
        case '\x07':
            str = "<BEL>";
            break;
        case '\x08':
            str = "<BS>";
            break;
        case '\x09':    // horizontal tab
            str = "    ";
            break;
        case '\x0B':
            str = "<VT>";
            break;
        case '\x0C':
            str = "<FF>";
            break;
        case '\x0D':
            str = "<CR>";
            break;
        case '\x0E':
            str = "<SO>";
            break;
        case '\x0F':
            str = "<SI>";
            break;
        case '\x10':
            str = "<DLE>";
            break;
        case '\x11':
            str = "<DC1>";
            break;
        case '\x12':
            str = "<DC2>";
            break;
        case '\x13':
            str = "<DC3>";
            break;
        case '\x14':
            str = "<DC4>";
            break;
        case '\x15':
            str = "<NAK>";
            break;
        case '\x16':
            str = "<SYN>";
            break;
        case '\x17':
            str = "<ETB>";
            break;
        case '\x18':
            str = "<CAN>";
            break;
        case '\x19':
            str = "<EM>";
            break;
        case '\x1A':
            str = "<SUB>";
            break;
        case '\x1B':
            str = "<ESC>";
            break;
        case '\x1C':
            str = "<FS>";
            break;
        case '\x1D':
            str = "<GS>";
            break;
        case '\x1E':
            str = "<RS>";
            break;
        case '\x1F':
            str = "<US>";
            break;
        case '\x7F':
            str = "<DEL>";
            break;
        default:
            str = c;
            break;
        }
        for (char c: str)
            list.push_back(StyledChar{.value = c_to_wc(c), .style = style});
    }

    std::vector<StyledChar> TextInputState::process(
            const Style text_style, const Style selection_style, const Style cursor_style, const Style cursor_style_ins, const Style cursor_style_sel
    ) {
        std::vector<StyledChar> result;

        const auto [selection_start, selection_end] = get_selection_range();
        const Style cur_style = selection_mode ? cursor_style_sel : insert_mode ? cursor_style_ins : cursor_style;

        for (size_t i = 0; i <= data.size(); i++) {
            if (i == cursor) {
                if (i == data.size() || data[i] == '\n')
                    result.push_back(StyledChar{.value = ' ', .style = cur_style});
                if (i < data.size())
                    format_styled_text(result, data[i], cur_style);
            } else if (i < data.size()) {
                if (selection_mode && selection_start <= i && i < selection_end)
                    format_styled_text(result, data[i], selection_style);
                else
                    format_styled_text(result, data[i], text_style);
            }
        }

        return result;
    }

    void TextInput(State &state, TextInputState &text_state, TextInputInfo info) {
        for (const Event &event: text_state.get_captured_events()) {
            HandleEvent(event, [&](const KeyEvent &ev) {
                if (!ev.key_down)
                    return;
                switch (ev.key_code) {
                case KeyCode::ESCAPE:
                    text_state.end_selection();
                    break;
                case KeyCode::BACKSPACE:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected()) {
                            text_state.erase_selection();
                            text_state.end_selection();
                        } else
                            text_state.on_key_backspace();
                    }
                    break;
                case KeyCode::DELETE:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected()) {
                            text_state.erase_selection();
                            text_state.end_selection();
                        } else
                            text_state.on_key_delete();
                    }
                    break;
                case KeyCode::LEFT:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected()) {
                            const auto selection_start = text_state.get_selection_range().first;
                            text_state.end_selection();
                            text_state.set_cursor(selection_start);
                        } else
                            text_state.move_left();
                    }
                    if (ev.modifiers & KEY_SHIFT) {
                        text_state.start_selection();
                        text_state.move_left();
                    }
                    break;
                case KeyCode::RIGHT:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected()) {
                            const auto selection_end = text_state.get_selection_range().second;
                            text_state.end_selection();
                            text_state.set_cursor(selection_end);
                        } else
                            text_state.move_right();
                    }
                    if (ev.modifiers & KEY_SHIFT) {
                        text_state.start_selection();
                        text_state.move_right();
                    }
                    break;
                case KeyCode::HOME:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected())
                            text_state.end_selection();
                        text_state.go_home();
                    }
                    if (ev.modifiers & KEY_SHIFT) {
                        text_state.start_selection();
                        text_state.go_home();
                    }
                    break;
                case KeyCode::END:
                    if (ev.modifiers == 0) {
                        if (text_state.is_selected())
                            text_state.end_selection();
                        text_state.go_end();
                    }
                    if (ev.modifiers & KEY_SHIFT) {
                        text_state.start_selection();
                        text_state.go_end();
                    }
                    break;
                case KeyCode::INSERT:
                    if (ev.modifiers == 0)
                        text_state.toggle_insert_mode();
                    break;
                case KeyCode::ENTER:
                    if (info.handle_enter_as_event) {
                        if (info.on_enter)
                            info.on_enter(std::ref(info));
                    } else if (ev.modifiers == 0) {
                        if (text_state.is_selected()) {
                            text_state.erase_selection();
                            text_state.end_selection();
                        }
                        text_state.insert_char('\n');
                    }
                    break;
                default:
                    if ((ev.modifiers == 0 || ev.modifiers & KEY_SHIFT) && std::isprint(ev.key_char)) {
                        if (text_state.is_selected()) {
                            text_state.erase_selection();
                            text_state.end_selection();
                        }
                        text_state.insert_char(ev.key_char);
                    }
                    break;
                }
            });
        }

        // clang-format off
        RichTextBox(state, {
            .text = text_state.process(info.text_style, info.selection_style, info.cursor_style, info.cursor_style_ins, info.cursor_style_sel),
            .pos = info.pos,
            .size = info.size,
            .style = info.text_style,
            .wrap = info.wrap,
            .align = info.align,
        });
        // clang-format on

        text_state.clear_captured_events();
    }

    std::vector<StyledChar> compute_check_box(CheckBoxValue &value, const CheckBoxInfo &info) {
        std::vector<StyledChar> result;
        switch (value) {
        case CheckBoxValue::UNCHECKED:
            result.push_back(info.check_box.unchecked);
            result.push_back({' ', info.check_box.unchecked.style});
            // result.push_back({' ', info.check_box.unchecked.style});
            break;
        case CheckBoxValue::CHECKED:
            result.push_back(info.check_box.checked);
            result.push_back({' ', info.check_box.checked.style});
            // result.push_back({' ', info.check_box.checked.style});
            break;
        case CheckBoxValue::INDETERMINATE:
            result.push_back(info.check_box.indeterm);
            result.push_back({' ', info.check_box.indeterm.style});
            // result.push_back({' ', info.check_box.indeterm.style});
            break;
        }
        for (char c: info.text)
            result.push_back(StyledChar{c_to_wc(c), info.style});
        return result;
    }

    void CheckBox(State &state, CheckBoxValue &value, CheckBoxInfo info) {
        if (!info.allow_indeterm && value == CheckBoxValue::INDETERMINATE)
            value = CheckBoxValue::UNCHECKED;

        const auto click_action = [&]() {
            switch (value) {
            case CheckBoxValue::UNCHECKED:
                value = CheckBoxValue::CHECKED;
                break;
            case CheckBoxValue::CHECKED:
                value = info.allow_indeterm ? CheckBoxValue::INDETERMINATE : CheckBoxValue::UNCHECKED;
                break;
            case CheckBoxValue::INDETERMINATE:
                value = CheckBoxValue::UNCHECKED;
                break;
            }
        };

        // clang-format off
        RichText(state, {
            .text = compute_check_box(value, info),
            .pos = info.pos,
            .on_hover =
                [&](RichTextInfo &text_info) {
                    if (info.on_hover)
                        info.on_hover(std::ref(info));
                    text_info.text = compute_check_box(value, info);
                },
            .on_click =
                [&](RichTextInfo &text_info) {
                    click_action();
                    if (info.on_click)
                        info.on_click(std::ref(info));
                    text_info.text = compute_check_box(value, info);
                },
            .on_click2 =
                [&](RichTextInfo &text_info) {
                    click_action();
                    if (info.on_click2)
                        info.on_click2(std::ref(info));
                    text_info.text = compute_check_box(value, info);
                },
            .on_menu =
                [&](RichTextInfo &text_info) {
                    if (info.on_menu)
                        info.on_menu(std::ref(info));
                    text_info.text = compute_check_box(value, info);
                }
        });
        // clang-format on
    }

    bool PollEvent(State &state, Event &event) {
        if (internal::PollRawEvent(event)) {
            // clang-format off
            HandleEvent(
                event,
                [&](const KeyEvent &ev) {
                    state.impl->key_states[ev.key_code] = KeyState{.printable = is_print(ev.key_char), .down = ev.key_down};
                },
                [&](const MouseEvent &ev) {
                    switch (ev.kind) {
                    case MouseEventKind::CLICK:
                        state.impl->btn_states[static_cast<size_t>(ev.button)].click1_count++;
                        break;
                    case MouseEventKind::DOUBLE_CLICK:
                        state.impl->btn_states[static_cast<size_t>(ev.button)].click2_count++;
                        break;
                    case MouseEventKind::MOVED:
                        state.impl->mouse_pos = ev.pos;
                        break;
                    case MouseEventKind::SCROLL_DOWN:
                        state.impl->mouse_scroll_v++;
                        break;
                    case MouseEventKind::SCROLL_UP:
                        state.impl->mouse_scroll_v--;
                        break;
                    case MouseEventKind::SCROLL_LEFT:
                        state.impl->mouse_scroll_h--;
                        break;
                    case MouseEventKind::SCROLL_RIGHT:
                        state.impl->mouse_scroll_h++;
                        break;
                    }
                }
            );
            // clang-format on
            return true;
        }
        return false;
    }

    bool IsKeyPressed(const State &state, KeyCode key_code) {
        if (const auto it = state.impl->key_states.find(key_code); it != state.impl->key_states.end())
            return it->second.down && it->second.printable;
        return false;
    }

    bool IsKeyDown(const State &state, KeyCode key_code) {
        if (const auto it = state.impl->key_states.find(key_code); it != state.impl->key_states.end())
            return it->second.down;
        return false;
    }

    bool IsKeyUp(const State &state, KeyCode key_code) {
        return !IsKeyDown(state, key_code);
    }

    bool IsMouseClicked(const State &state, const MouseButton button) {
        return state.impl->btn_states[static_cast<size_t>(button)].click1_count > 0;
    }

    bool IsMouseDoubleClicked(const State &state, const MouseButton button) {
        return state.impl->btn_states[static_cast<size_t>(button)].click2_count > 0;
    }

    size_t GetMouseClickCount(const State &state, const MouseButton button) {
        return state.impl->btn_states[static_cast<size_t>(button)].click1_count;
    }

    size_t GetMouseClick2Count(const State &state, const MouseButton button) {
        return state.impl->btn_states[static_cast<size_t>(button)].click2_count;
    }

    Position GetMousePos(const State &state) {
        return state.impl->mouse_pos;
    }

    Position GetMouseRelPos(const State &state) {
        return GetMousePos(state) - GetPanePosition(state);
    }

    intmax_t GetMouseScrollV(const State &state) {
        return state.impl->mouse_scroll_v;
    }

    intmax_t GetMouseScrollH(const State &state) {
        return state.impl->mouse_scroll_h;
    }
}    // namespace nite

namespace nite::internal::console
{
    void set_style(const Style style) {
        // Refer to: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
        // Refer to: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h4-Functions-using-CSI-_-ordered-by-the-final-character-lparen-s-rparen:CSI-Pm-m.1CA7
        if (style.mode & STYLE_RESET)
            print(CSI "0m");
        if (style.mode & STYLE_BOLD)
            print(CSI "1m");
        if (style.mode & STYLE_LIGHT)
            print(CSI "2m");
        if (style.mode & STYLE_ITALIC)
            print(CSI "3m");
        if (style.mode & STYLE_UNDERLINE)
            print(CSI "4m");
        if (style.mode & STYLE_BLINK)
            print(CSI "5m");
        if (style.mode & STYLE_INVERSE)
            print(CSI "7m");
        if (style.mode & STYLE_INVISIBLE)
            print(CSI "8m");
        if (style.mode & STYLE_CROSSED_OUT)
            print(CSI "9m");
        if (style.mode & STYLE_UNDERLINE2)
            print(CSI "21m");

        if ((style.mode & STYLE_NO_FG) == 0)
            // Set foreground color
            print(CSI "38;2;" + std::to_string(style.fg.r) + ";" + std::to_string(style.fg.g) + ";" + std::to_string(style.fg.b) + "m");

        if ((style.mode & STYLE_NO_BG) == 0)
            // Set background color
            print(CSI "48;2;" + std::to_string(style.bg.r) + ";" + std::to_string(style.bg.g) + ";" + std::to_string(style.bg.b) + "m");
    }

    void gotoxy(const size_t col, const size_t row) {
        print(CSI "" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H");
    }

    static size_t prev_col = 0;
    static size_t prev_row = 0;
    static std::optional<Style> prev_style = std::nullopt;

    void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style) {
        std::string out;

        if (prev_col + 1 == col && prev_row == row)
            // no change, go with the flow
            ;
        else
            // goto to the specified coords
            out += (CSI + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H");

        // update these
        prev_col = col;
        prev_row = row;

        if (!prev_style || *prev_style != style) {
            // set the console style
            if (style.mode & STYLE_RESET)
                out += (CSI "0m");
            if (style.mode & STYLE_BOLD)
                out += (CSI "1m");
            if (style.mode & STYLE_LIGHT)
                out += (CSI "2m");
            if (style.mode & STYLE_ITALIC)
                out += (CSI "3m");
            if (style.mode & STYLE_UNDERLINE)
                out += (CSI "4m");
            if (style.mode & STYLE_BLINK)
                out += (CSI "5m");
            if (style.mode & STYLE_INVERSE)
                out += (CSI "7m");
            if (style.mode & STYLE_INVISIBLE)
                out += (CSI "8m");
            if (style.mode & STYLE_CROSSED_OUT)
                out += (CSI "9m");
            if (style.mode & STYLE_UNDERLINE2)
                out += (CSI "21m");

            if ((style.mode & STYLE_NO_FG) == 0)
                // Set foreground color
                out += (CSI "38;2;" + std::to_string(style.fg.r) + ";" + std::to_string(style.fg.g) + ";" + std::to_string(style.fg.b) + "m");

            if ((style.mode & STYLE_NO_BG) == 0)
                // Set background color
                out += (CSI "48;2;" + std::to_string(style.bg.r) + ";" + std::to_string(style.bg.g) + ";" + std::to_string(style.bg.b) + "m");

            prev_style = style;
        }
        // Now the main thing
        out += wc_to_str(value);
        print(out);
    }

    // void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style) {
    //     gotoxy(col, row);
    //     set_style(style);
    //     print(wc_to_str(value));
    // }
}    // namespace nite::internal::console

#ifdef OS_WINDOWS
#    include <windows.h>

namespace nite::internal::console
{
    static std::string get_last_error() {
        char err_msg_buf[4096];
        DWORD size = FormatMessage(
                FORMAT_MESSAGE_FROM_SYSTEM                  // Searches the system message table
                        | FORMAT_MESSAGE_IGNORE_INSERTS,    // Ignores insert sequences in the message definition.
                NULL,                                       // Handle to the module containing the message table
                GetLastError(),                             // Error code to format
                0,                                          // Default language
                err_msg_buf,                                // Output buffer for the formatted message
                4096,                                       // Minimum size of the output buffer
                NULL                                        // No arguments for insert sequences
        );

        if (size == 0)
            return get_last_error();
        return std::string{err_msg_buf, size};
    }

    bool is_tty() {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        return GetConsoleMode(h_con, &mode);
    }

    Result clear() {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);

        constexpr const COORD coord_screen = {0, 0};    // Top-left corner
        DWORD chars_written;
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        // Fill the entire screen with blanks.
        if (!GetConsoleScreenBufferInfo(h_con, &csbi))
            return Result::Error("error getting console info: {}", get_last_error());
        if (!FillConsoleOutputCharacter(h_con, ' ', csbi.dwSize.X * csbi.dwSize.Y, coord_screen, &chars_written))
            return Result::Error("error writing to console: {}", get_last_error());

        // Set the buffer attributes.
        if (!GetConsoleScreenBufferInfo(h_con, &csbi))
            return Result::Error("error getting console info: {}", get_last_error());
        if (!FillConsoleOutputAttribute(
                    h_con, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, csbi.dwSize.X * csbi.dwSize.Y, coord_screen, &chars_written
            ))
            return Result::Error("error setting console color: {}", get_last_error());

        // Put the cursor in the top left corner.
        if (!SetConsoleCursorPosition(h_con, coord_screen))
            return Result::Error("error setting console position: {}", get_last_error());
        return Result::Ok;
    }

    Result size(size_t &width, size_t &height) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(h_con, &info))
            return Result::Error("error getting console size: {}", get_last_error());

        const int columns = info.srWindow.Right - info.srWindow.Left + 1;
        const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        width = columns > 0 ? columns : 0;
        height = rows > 0 ? rows : 0;
        return Result::Ok;
    }

    Result print(const std::string &text) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!WriteConsole(h_con, text.c_str(), text.size(), NULL, NULL))
            return Result::Error("error printing to console: {}", get_last_error());
        return Result::Ok;
    }

    static DWORD old_in_mode = 0, old_out_mode = 0;
    static UINT old_console_cp = 0;
    static std::string old_locale;

    Result init() {
        static const HANDLE h_conin = GetStdHandle(STD_INPUT_HANDLE);
        static const HANDLE h_conout = GetStdHandle(STD_OUTPUT_HANDLE);

        if (!GetConsoleMode(h_conout, &old_out_mode))
            return Result::Error("error getting console out mode: {}", get_last_error());
        if (!GetConsoleMode(h_conin, &old_in_mode))
            return Result::Error("error getting console in mode: {}", get_last_error());
        if ((old_console_cp = GetConsoleOutputCP()) == 0)
            return Result::Error("error getting console code page: {}", get_last_error());

        DWORD out_mode = 0;
        out_mode |= ENABLE_PROCESSED_OUTPUT;
        out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        out_mode |= DISABLE_NEWLINE_AUTO_RETURN;
        if (!SetConsoleMode(h_conout, out_mode))
            return Result::Error("error setting console in mode: {}", get_last_error());

        DWORD in_mode = 0;
        in_mode |= ENABLE_EXTENDED_FLAGS;
        in_mode |= ENABLE_MOUSE_INPUT;
        in_mode |= ENABLE_WINDOW_INPUT;
        in_mode &= ~ENABLE_QUICK_EDIT_MODE;
        if (!SetConsoleMode(h_conin, in_mode))
            return Result::Error("error setting console out mode: {}", get_last_error());

        if (!SetConsoleOutputCP(CP_UTF8))
            return Result::Error("error setting console code page: {}", get_last_error());

        // Set locale to utf-8
        old_locale = std::setlocale(LC_CTYPE, NULL);
        if (std::setlocale(LC_CTYPE, NITE_DEFAULT_LOCALE) == NULL)
            return Result::Error("error setting locale to '{}'", NITE_DEFAULT_LOCALE);

        $(print(CSI "?1049h"));    // Enter alternate buffer
        $(print(CSI "?25l"));      // Hide console cursor
        $(print(CSI "?30l"));      // Do not show scroll bar

        $(clear());
        return Result::Ok;
    }

    Result restore() {
        $(clear());

        $(print(CSI "?30h"));      // Show scroll bar
        $(print(CSI "?25h"));      // Show console cursor
        $(print(CSI "?1049l"));    // Exit alternate buffer

        // Restore locale
        if (std::setlocale(LC_CTYPE, old_locale.c_str()) == NULL)
            return Result::Error("error restoring locale to '{}'", old_locale);

        if (!SetConsoleOutputCP(old_console_cp))
            return Result::Error("error setting console code page: {}", get_last_error());
        if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), old_in_mode))
            return Result::Error("error setting console in mode: {}", get_last_error());
        if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), old_out_mode))
            return Result::Error("error setting console out mode: {}", get_last_error());
        return Result::Ok;
    }
}    // namespace nite::internal::console

#endif

#ifdef OS_WINDOWS
namespace nite::internal
{
    static bool get_key_mod(WORD virtual_key_code, uint8_t &key_mod);
    static bool get_key_code(WORD virtual_key_code, char key_char, KeyCode &key_code);

    bool PollRawEvent(Event &event) {
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
                kind = MouseEventKind::CLICK;
                if (info.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
                    button = MouseButton::LEFT;
                else if (info.dwButtonState & (FROM_LEFT_2ND_BUTTON_PRESSED | FROM_LEFT_3RD_BUTTON_PRESSED | FROM_LEFT_4TH_BUTTON_PRESSED))
                    button = MouseButton::MIDDLE;
                else if (info.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
                    button = MouseButton::RIGHT;
                else
                    return false;
                break;
            case DOUBLE_CLICK:
#    undef DOUBLE_CLICK
                kind = MouseEventKind::DOUBLE_CLICK;
                if (info.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
                    button = MouseButton::LEFT;
                else if (info.dwButtonState & (FROM_LEFT_2ND_BUTTON_PRESSED | FROM_LEFT_3RD_BUTTON_PRESSED | FROM_LEFT_4TH_BUTTON_PRESSED))
                    button = MouseButton::MIDDLE;
                else if (info.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
                    button = MouseButton::RIGHT;
                else
                    return false;
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

            event = MouseEvent{
                    .kind = kind,
                    .button = button,
                    .pos = pos,
                    .modifiers = modifiers,
            };
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
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            key_mod = KEY_SHIFT;
            return true;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            key_mod = KEY_CTRL;
            return true;
        case VK_MENU:
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
            if ('0' <= virtual_key_code && virtual_key_code <= '9') {
                key_code = static_cast<KeyCode>(virtual_key_code - '0' + static_cast<int>(KeyCode::K_0));
                return true;
            }
            if ('A' <= virtual_key_code && virtual_key_code <= 'Z') {
                key_code = static_cast<KeyCode>(virtual_key_code - 'A' + static_cast<int>(KeyCode::K_A));
                return true;
            }
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
}    // namespace nite::internal
#endif

#ifdef OS_LINUX
// #    define NITE_USE_NCURSES
#    ifdef NITE_USE_NCURSES

// The `ncurses` backend has many caveats:
// ----------------------------------------------------------------------------------------------------
// 1. It does not have any  support for Escape key
// 2. It does not have good support for modifier key: Shift+Key
// 3. It does not have any  support for modifier keys: Ctrl+Key, Ctrl+Shift+Key, Ctrl+Shift+Alt+Key,
//                                                     Shift+Alt+Key, Ctrl+Alt+Key, Alt+Key
// 4. It does not have any  support for mouse scroll: up, down, left, right
// 5. There is no way to figure out what kind of error occured if any operation fails
// ----------------------------------------------------------------------------------------------------

#        include <ncurses.h>

namespace nite::internal::console
{
    inline static std::string get_last_error() {
        return std::strerror(errno);
    }

    bool is_tty() {
        return isatty(STDOUT_FILENO);
    }

    Result clear() {
        if (erase() == ERR)
            return Result::Error();
        return Result::Ok;
    }

    Result size(size_t &width, size_t &height) {
        width = COLS;
        height = LINES;
        return Result::Ok;
    }

    Result print(const std::string &text) {
        if (write(STDOUT_FILENO, text.data(), text.size()) == -1)
            return Result::Error("error writing to the console: {}", get_last_error());
        return Result::Ok;
    }

    static std::string old_locale;
    static mmask_t old_mmask;

    Result init() {
        // Set locale to utf-8
        old_locale = std::setlocale(LC_CTYPE, NULL);
        if (std::setlocale(LC_CTYPE, NITE_DEFAULT_LOCALE) == NULL)
            return Result::Error("error setting locale to '{}'", NITE_DEFAULT_LOCALE);

        initscr();               // Start curses mode
        raw();                   // Make the terminal raw
        cbreak();                // Disable line buffering, character processing
        noecho();                // Disable echoing of user input
        keypad(stdscr, true);    // Now we get LEFT, RIGHT, F1, F2, ... etc
        meta(stdscr, true);      // Enable `termios->c_cflag |= CS8;`
        timeout(2);              // getch() has a timeout for 2ms
        curs_set(0);             // Make cursor invisible

        // Enable mouse events
        mmask_t mmask = BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED | BUTTON2_CLICKED | BUTTON2_DOUBLE_CLICKED | BUTTON3_CLICKED |
                        BUTTON3_DOUBLE_CLICKED | BUTTON4_CLICKED | BUTTON4_DOUBLE_CLICKED | BUTTON5_CLICKED | BUTTON5_DOUBLE_CLICKED | BUTTON_SHIFT |
                        BUTTON_CTRL | BUTTON_ALT | REPORT_MOUSE_POSITION;
        mousemask(mmask, &old_mmask);

        refresh();
        return Result::Ok;
    }

    Result restore() {
        endwin();    // End curses mode

        // Restore locale
        if (std::setlocale(LC_CTYPE, old_locale.c_str()) == NULL)
            return Result::Error("error restoring locale to '{}'", old_locale);
        return Result::Ok;
    }
}    // namespace nite::internal::console

namespace nite::internal
{
    Result get_key_code(char c, KeyCode &key_code);

    bool PollRawEvent(Event &event) {
        static std::vector<Event> pending_events;

        if (!pending_events.empty()) {
            event = pending_events.back();
            pending_events.pop_back();
            return true;
        }

        int c = getch();
        if (c == ERR)
            return false;

        switch (c) {
        case KEY_BACKSPACE:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::BACKSPACE,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_ENTER:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::ENTER,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_LEFT:
        case KEY_SLEFT:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::LEFT,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SLEFT ? KEY_SHIFT : 0),
            };
            break;
        case KEY_RIGHT:
        case KEY_SRIGHT:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::RIGHT,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SRIGHT ? KEY_SHIFT : 0),
            };
            break;
        case KEY_UP:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::UP,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_DOWN:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::DOWN,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_HOME:
        case KEY_SHOME:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::HOME,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SHOME ? KEY_SHIFT : 0),
            };
            break;
        case KEY_END:
        case KEY_SEND:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::END,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SEND ? KEY_SHIFT : 0),
            };
            break;
        case KEY_NPAGE:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::PAGE_UP,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_PPAGE:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::PAGE_DOWN,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_BTAB:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::TAB,
                    .key_char = 0,
                    .modifiers = 0,
            };
            break;
        case KEY_IC:
        case KEY_SIC:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::INSERT,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SIC ? KEY_SHIFT : 0),
            };
            break;
        case KEY_DC:
        case KEY_SDC:
            event = KeyEvent{
                    .key_down = true,
                    .key_code = KeyCode::DELETE,
                    .key_char = 0,
                    .modifiers = static_cast<uint8_t>(c == KEY_SDC ? KEY_SHIFT : 0),
            };
            break;
        case KEY_RESIZE:
            event = ResizeEvent{
                    .size = {.width = static_cast<size_t>(COLS), .height = static_cast<size_t>(LINES)}
            };
            break;
        case KEY_MOUSE: {
            MEVENT ev;
            if (getmouse(&ev) == ERR)
                return false;
            MouseEventKind kind = MouseEventKind::MOVED;
            MouseButton button = MouseButton::NONE;
            Position pos{
                    .x = static_cast<size_t>(ev.x),
                    .y = static_cast<size_t>(ev.y),
            };
            uint8_t modifiers = 0;

            if (ev.bstate & BUTTON1_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::LEFT;
            }
            if (ev.bstate & BUTTON1_DOUBLE_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::LEFT;
            }

            if (ev.bstate & BUTTON2_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }
            if (ev.bstate & BUTTON2_DOUBLE_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }

            if (ev.bstate & BUTTON3_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }
            if (ev.bstate & BUTTON3_DOUBLE_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }

            if (ev.bstate & BUTTON4_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }
            if (ev.bstate & BUTTON4_DOUBLE_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::MIDDLE;
            }

            if (ev.bstate & BUTTON5_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::RIGHT;
            }
            if (ev.bstate & BUTTON5_DOUBLE_CLICKED) {
                kind = MouseEventKind::CLICK;
                button = MouseButton::RIGHT;
            }

            if (ev.bstate & BUTTON_SHIFT)
                modifiers |= KEY_SHIFT;
            if (ev.bstate & BUTTON_CTRL)
                modifiers |= KEY_CTRL;
            if (ev.bstate & BUTTON_ALT)
                modifiers |= KEY_ALT;

            if (ev.bstate & REPORT_MOUSE_POSITION) {
                kind = MouseEventKind::MOVED;
                button = MouseButton::NONE;
            }

            event = MouseEvent{
                    .kind = kind,
                    .button = button,
                    .pos = pos,
                    .modifiers = modifiers,
            };
            break;
        }
        default:
            if (KEY_F(1) <= c && c <= KEY_F(24)) {
                const KeyCode key_code = static_cast<KeyCode>(static_cast<int>(KeyCode::F1) + c - KEY_F(1));
                event = KeyEvent{
                        .key_down = true,
                        .key_code = key_code,
                        .key_char = 0,
                        .modifiers = 0,
                };
            } else if (KeyCode key_code; get_key_code(static_cast<char>(c), key_code)) {
                event = KeyEvent{
                        .key_down = true,
                        .key_code = key_code,
                        .key_char = static_cast<char>(c),
                        .modifiers = 0,
                };
            } else
                return false;
            break;
        }

        if (const auto key_event = std::get_if<KeyEvent>(&event)) {
            KeyEvent release_ev = *key_event;
            release_ev.key_down = false;
            pending_events.push_back(release_ev);
        }

        return true;
    }

    Result get_key_code(char c, KeyCode &key_code) {
        switch (c) {
        case '\b':
            key_code = KeyCode::BACKSPACE;
            return Result::Ok;
        case '\r':
        case '\n':
            key_code = KeyCode::ENTER;
            return Result::Ok;
        case '\t':
            key_code = KeyCode::TAB;
            return Result::Ok;
        case '\x7f':
            key_code = KeyCode::ESCAPE;
            return Result::Ok;
        case '\x1b':
            key_code = KeyCode::ESCAPE;
            return Result::Ok;
        case ' ':
            key_code = KeyCode::SPACE;
            return Result::Ok;
        case '!':
            key_code = KeyCode::BANG;
            return Result::Ok;
        case '@':
            key_code = KeyCode::AT;
            return Result::Ok;
        case '#':
            key_code = KeyCode::HASH;
            return Result::Ok;
        case '$':
            key_code = KeyCode::DOLLAR;
            return Result::Ok;
        case '%':
            key_code = KeyCode::PERCENT;
            return Result::Ok;
        case '^':
            key_code = KeyCode::CARET;
            return Result::Ok;
        case '&':
            key_code = KeyCode::AMPERSAND;
            return Result::Ok;
        case '*':
            key_code = KeyCode::ASTERISK;
            return Result::Ok;
        case '(':
            key_code = KeyCode::LPAREN;
            return Result::Ok;
        case ')':
            key_code = KeyCode::RPAREN;
            return Result::Ok;
        case '_':
            key_code = KeyCode::UNDERSCORE;
            return Result::Ok;
        case '+':
            key_code = KeyCode::PLUS;
            return Result::Ok;
        case '-':
            key_code = KeyCode::MINUS;
            return Result::Ok;
        case '=':
            key_code = KeyCode::EQUAL;
            return Result::Ok;
        case '{':
            key_code = KeyCode::LBRACE;
            return Result::Ok;
        case '}':
            key_code = KeyCode::RBRACE;
            return Result::Ok;
        case '[':
            key_code = KeyCode::LBRACKET;
            return Result::Ok;
        case ']':
            key_code = KeyCode::RBRACKET;
            return Result::Ok;
        case '|':
            key_code = KeyCode::PIPE;
            return Result::Ok;
        case '\\':
            key_code = KeyCode::BACKSLASH;
            return Result::Ok;
        case ':':
            key_code = KeyCode::COLON;
            return Result::Ok;
        case '"':
            key_code = KeyCode::DQUOTE;
            return Result::Ok;
        case ';':
            key_code = KeyCode::SEMICOLON;
            return Result::Ok;
        case '\'':
            key_code = KeyCode::SQUOTE;
            return Result::Ok;
        case '<':
            key_code = KeyCode::LESS;
            return Result::Ok;
        case '>':
            key_code = KeyCode::GREATER;
            return Result::Ok;
        case '?':
            key_code = KeyCode::HOOK;
            return Result::Ok;
        case ',':
            key_code = KeyCode::COMMA;
            return Result::Ok;
        case '.':
            key_code = KeyCode::PERIOD;
            return Result::Ok;
        case '/':
            key_code = KeyCode::SLASH;
            return Result::Ok;
        case '`':
            key_code = KeyCode::BQUOTE;
            return Result::Ok;
        case '~':
            key_code = KeyCode::TILDE;
            return Result::Ok;
        default:
            if ('a' <= c && c <= 'z') {
                key_code = static_cast<KeyCode>(c - 'a' + static_cast<int>(KeyCode::K_A));
                return Result::Ok;
            } else if ('A' <= c && c <= 'Z') {
                key_code = static_cast<KeyCode>(c - 'A' + static_cast<int>(KeyCode::K_A));
                return Result::Ok;
            } else if ('0' <= c && c <= '9') {
                key_code = static_cast<KeyCode>(c - '0' + static_cast<int>(KeyCode::K_0));
                return Result::Ok;
            }
        }
        return Result::Error("key not supported: 0x{:x}", c);
    }
}    // namespace nite::internal

#    else

#        include <bits/types/struct_timeval.h>
#        include <sys/ioctl.h>
#        include <sys/select.h>
#        include <sys/types.h>
#        include <termios.h>
#        include <unistd.h>

// strace ./nite -C -O -U
namespace nite::internal::console
{
    inline static std::string get_last_error() {
        return std::strerror(errno);
    }

    bool is_tty() {
        return isatty(STDOUT_FILENO);
    }

    Result clear() {
        $(print(CSI "2J"));
        return Result::Ok;
    }

    Result size(size_t &width, size_t &height) {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
            return Result::Error("error getting console size: {}", get_last_error());
        width = w.ws_col;
        height = w.ws_row;
        return Result::Ok;
    }

    Result print(const std::string &text) {
        if (write(STDOUT_FILENO, text.data(), text.size()) == -1)
            return Result::Error("error writing to the console: {}", get_last_error());
        return Result::Ok;
    }

    static struct termios old_term, new_term;
    static std::string old_locale;

    Result init() {
        // Refer to: man 3 termios
        if (tcgetattr(STDIN_FILENO, &old_term) == -1)
            return Result::Error("error getting terminal attributes: {}", get_last_error());

        new_term = old_term;
        cfmakeraw(&new_term);    // enable raw mode
        // cfmakeraw() does this:
        //      new_term->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
        //      new_term->c_oflag &= ~OPOST;
        //      new_term->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        //      new_term->c_cflag &= ~(CSIZE | PARENB);
        //      new_term->c_cflag |= CS8;
        new_term.c_cc[VMIN] = 0;     // enable polling read
        new_term.c_cc[VTIME] = 0;    // enable polling read
        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1)
            return Result::Error("error setting terminal attributes: {}", get_last_error());

        // Set locale to utf-8
        old_locale = std::setlocale(LC_CTYPE, NULL);
        if (std::setlocale(LC_CTYPE, NITE_DEFAULT_LOCALE) == NULL)
            return Result::Error("error setting locale to '{}'", NITE_DEFAULT_LOCALE);

        $(print(CSI "?1049h"));    // Enter alternate buffer
        $(print(CSI "?25l"));      // Hide console cursor
        $(clear());

        // Refer to: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_
        // Refer to: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Alt-and-Meta-Keys
        // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol

        // Enable kitty keyboard protocol
        // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
        // flags = 1 | 4 = 5
        //       = `Disambiguate escape codes` and `Report alternate keys`
        $(print(CSI ">5u"));

        // Mouse specific
        $(print(CSI "?1000h"));    // Send Mouse X & Y on button press and release
        $(print(CSI "?1002h"));    // Use Cell Motion Mouse Tracking (move and drag tracking)
        $(print(CSI "?1003h"));    // Use All Motion Mouse Tracking
        $(print(CSI "?1006h"));    // Enable SGR Mouse Mode
        // Other specific
        $(print(CSI "?1004h"));    // Send FocusIn/FocusOut events
        $(print(CSI "?30l"));      // Do not show scroll bar
        return Result::Ok;
    }

    Result restore() {
        // Other specific
        $(print(CSI "?30h"));      // Show scroll bar
        $(print(CSI "?1004l"));    // Do not send FocusIn/FocusOut events
        // Mouse specific
        $(print(CSI "?1006l"));    // Disable SGR Mouse Mode
        $(print(CSI "?1003l"));    // Do not use All Motion Mouse Tracking
        $(print(CSI "?1002l"));    // Do not use Cell Motion Mouse Tracking (move and drag tracking)
        $(print(CSI "?1000l"));    // Do not send Mouse X & Y on button press and release

        // Disable kitty keyboard protocol
        $(print(CSI "<u"));

        $(clear());
        $(print(CSI "?25h"));      // Show console cursor
        $(print(CSI "?1049l"));    // Exit alternate buffer

        // Restore locale
        if (std::setlocale(LC_CTYPE, old_locale.c_str()) == NULL)
            return Result::Error("error restoring locale to '{}'", old_locale);
        // Restore old terminal modes
        if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) == -1)
            return Result::Error("error setting terminal attributes: {}", get_last_error());
        return Result::Ok;
    }
}    // namespace nite::internal::console

namespace nite::internal
{
    Result get_key_code(char c, KeyCode &key_code);

    bool con_read(std::string &str) {
        // more efficient version and takes all available input
        static char buf[256];
        memset(buf, 0, sizeof(buf));
        str = "";

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 2 * 1000;

        for (;;) {
            int ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
            if (ret == -1)
                break;    // Call failed
            if (ret == 0)
                break;    // Nothing available

            ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
            if (len == -1)
                break;    // Call failed
            if (len == 0)
                break;    // Nothing available

            str.append(buf, len);
        }
        return !str.empty();
    }

    // -------------------------------------------------------------------
    // <mouse_sequence> := CSI '<' NUMBER ';' NUMBER ';' NUMBER 'M';
    // <focus_sequence> := CSI ('O' | 'I');
    //
    // ESC      := \033
    // CSI      := \033\[
    // DIGIT    := [0-9]+
    // NUMBER   := DIGIT+
    // -------------------------------------------------------------------

#        define PARSER_TERMINATOR '\n'

    class Parser {
        size_t index = 0;
        std::string text;

        // Previous mouse click time point
        std::chrono::high_resolution_clock::time_point prev_mcl_tp = std::chrono::high_resolution_clock::time_point();

        Result parse_mouse(Event &event) {
            const auto mev_tp = std::chrono::high_resolution_clock::now();

            $(expect_csi());
            $(expect('<'));

            uint8_t control_byte = 0;
            size_t x_coord = 0;
            size_t y_coord = 0;

            $(expect_number(control_byte));
            $(expect(';'));
            $(expect_number(x_coord));
            $(expect(';'));
            $(expect_number(y_coord));
            if (!match_any('M', 'm'))
                return Result::Error("expected 'm' or 'M'");

            // Bit layout of `control_byte`
            // 0x * * * * * * * *
            //    7 6 5 4 3 2 1 0
            // 0 - button number
            // 1 - button number
            // 2 - shift
            // 3 - meta or alt
            // 4 - control
            // 5 - mouse dragging (ignored)
            // 6 - button number
            // 7 - button number
            MouseEventKind kind;
            MouseButton button = MouseButton::NONE;
            uint8_t modifiers = 0;

            const uint8_t button_number = (control_byte & 0b0000'0011) | ((control_byte & 0b1100'0000) >> 4);
            const bool dragging = (control_byte & 0b0010'0000) == 0b0010'0000;

            switch (button_number) {
            case 0:
                kind = dragging ? MouseEventKind::MOVED : MouseEventKind::CLICK;
                button = dragging ? MouseButton::NONE : MouseButton::LEFT;
                break;
            case 1:
                kind = dragging ? MouseEventKind::MOVED : MouseEventKind::CLICK;
                button = dragging ? MouseButton::NONE : MouseButton::MIDDLE;
                break;
            case 2:
                kind = dragging ? MouseEventKind::MOVED : MouseEventKind::CLICK;
                button = dragging ? MouseButton::NONE : MouseButton::RIGHT;
                break;
            case 3:
                kind = MouseEventKind::MOVED;
                break;
            case 4:
                kind = dragging ? MouseEventKind::MOVED : MouseEventKind::SCROLL_UP;
                break;
            case 5:
                kind = dragging ? MouseEventKind::MOVED : MouseEventKind::SCROLL_DOWN;
                break;
            case 6:
                if (dragging)
                    return Result::Error();
                kind = MouseEventKind::SCROLL_LEFT;
                break;
            case 7:
                if (dragging)
                    return Result::Error();
                kind = MouseEventKind::SCROLL_RIGHT;
                break;
            default:
                return Result::Error();
            }

            if (kind == MouseEventKind::CLICK) {
                // avoid mouse down events
                if (current() == 'M') {
                    kind = MouseEventKind::MOVED;
                    button = MouseButton::NONE;
                } else {
                    // Double click time difference is 500ms
                    if (mev_tp - prev_mcl_tp <= std::chrono::milliseconds(500)) {
                        kind = MouseEventKind::DOUBLE_CLICK;
                        prev_mcl_tp = std::chrono::high_resolution_clock::time_point();
                    } else
                        prev_mcl_tp = mev_tp;
                }
            }

            if ((control_byte >> 2) & 0b001)
                modifiers |= KEY_SHIFT;
            if ((control_byte >> 2) & 0b010)
                modifiers |= KEY_ALT;
            if ((control_byte >> 2) & 0b100)
                modifiers |= KEY_CTRL;

            event = MouseEvent{
                    .kind = kind,
                    .button = button,
                    .pos = {.col = x_coord - 1, .row = y_coord - 1},
                    .modifiers = modifiers,
            };
            return Result::Ok;
        }

        // CSI NUMBER (':' NUMBER? (':' NUMBER?)?)? (';' NUMBER?)? [ABCDEFHPQSu~]
        // 0x0d         -> Enter key
        // 0x7f | 0x08  -> Backspace
        // 0x09         -> Tab
        // any printable char
        Result parse_key_and_focus(Event &event) {
            switch (advance()) {
            case 0x0d:    // Enter key
                event = KeyEvent{
                        .key_down = true,
                        .key_code = KeyCode::ENTER,
                        .key_char = current(),
                        .modifiers = 0,
                };
                return Result::Ok;
            case 0x7f:
            case 0x08:    // Backspace key
                event = KeyEvent{
                        .key_down = true,
                        .key_code = KeyCode::BACKSPACE,
                        .key_char = current(),
                        .modifiers = 0,
                };
                return Result::Ok;
            case 0x09:    // Tab key
                event = KeyEvent{
                        .key_down = true,
                        .key_code = KeyCode::TAB,
                        .key_char = current(),
                        .modifiers = 0,
                };
                return Result::Ok;
            case *ESC: {
                $(expect('['));    // Now we are past the CSI

                if (match('O')) {
                    event = FocusEvent{.focus_gained = false};
                    return Result::Ok;
                } else if (match('I')) {
                    event = FocusEvent{.focus_gained = true};
                    return Result::Ok;
                }

                int key_val_unsh = 0;              // unshifted key val
                std::optional<char> key_val_sh;    // shifted key val
                std::optional<uint8_t> key_modifiers;
                char functional;

                $(expect_number(key_val_unsh));

                if (match(':')) {
                    if (uint8_t val; match_number(val))
                        key_val_sh = val;
                    if (match(':')) {
                        size_t val;
                        match_number(val);    // base layout key (ignored)
                    }
                }

                if (match(';'))
                    // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol/#modifiers
                    if (uint8_t val; match_number(val))
                        key_modifiers = saturated_sub(val, static_cast<uint8_t>(1));

                if (match_any('A', 'B', 'C', 'D', 'E', 'F', 'H', 'P', 'Q', 'S', 'u', '~'))
                    functional = current();
                else
                    return Result::Error("expected one of 'ABCDEFHPQSu~'");

                KeyEvent kev;

                if (key_val_sh) {
                    kev.key_char = *key_val_sh;
                    $(get_key_code(kev.key_char, kev.key_code));
                } else {
                    if (key_val_unsh == 1) {
                        switch (functional) {
                        case 'A':
                            kev.key_code = KeyCode::UP;
                            break;
                        case 'B':
                            kev.key_code = KeyCode::DOWN;
                            break;
                        case 'C':
                            kev.key_code = KeyCode::RIGHT;
                            break;
                        case 'D':
                            kev.key_code = KeyCode::LEFT;
                            break;
                        case 'F':
                            kev.key_code = KeyCode::END;
                            break;
                        case 'H':
                            kev.key_code = KeyCode::HOME;
                            break;
                        case 'P':
                            kev.key_code = KeyCode::F1;
                            break;
                        case 'Q':
                            kev.key_code = KeyCode::F2;
                            break;
                        case 'S':
                            kev.key_code = KeyCode::F4;
                            break;
                        default:
                            return Result::Error("key not supported");
                        }
                    } else if (functional == '~') {
                        switch (key_val_unsh) {
                        case 1:
                            // VT220 supports this
                            kev.key_code = KeyCode::HOME;
                            break;
                        case 2:
                            kev.key_code = KeyCode::INSERT;
                            break;
                        case 3:
                            kev.key_code = KeyCode::DELETE;
                            break;
                        case 5:
                            kev.key_code = KeyCode::PAGE_UP;
                            break;
                        case 6:
                            kev.key_code = KeyCode::PAGE_DOWN;
                            break;
                        case 7:
                            kev.key_code = KeyCode::HOME;
                            break;
                        case 8:
                            kev.key_code = KeyCode::END;
                            break;
                        case 11:
                            kev.key_code = KeyCode::F1;
                            break;
                        case 12:
                            kev.key_code = KeyCode::F2;
                            break;
                        case 13:
                            kev.key_code = KeyCode::F3;
                            break;
                        case 14:
                            kev.key_code = KeyCode::F4;
                            break;
                        case 15:
                            kev.key_code = KeyCode::F5;
                            break;
                        case 17:
                            kev.key_code = KeyCode::F6;
                            break;
                        case 18:
                            kev.key_code = KeyCode::F7;
                            break;
                        case 19:
                            kev.key_code = KeyCode::F8;
                            break;
                        case 20:
                            kev.key_code = KeyCode::F9;
                            break;
                        case 21:
                            kev.key_code = KeyCode::F10;
                            break;
                        case 23:
                            kev.key_code = KeyCode::F11;
                            break;
                        case 24:
                            kev.key_code = KeyCode::F12;
                            break;
                        case 25:
                            // VT220 supports this
                            kev.key_code = KeyCode::F13;
                            break;
                        case 26:
                            // VT220 supports this
                            kev.key_code = KeyCode::F14;
                            break;
                        case 28:
                            // VT220 supports this
                            kev.key_code = KeyCode::F15;
                            break;
                        case 29:
                            // VT220 supports this
                            kev.key_code = KeyCode::F16;
                            break;
                        case 31:
                            // VT220 supports this
                            kev.key_code = KeyCode::F17;
                            break;
                        case 32:
                            // VT220 supports this
                            kev.key_code = KeyCode::F18;
                            break;
                        case 33:
                            // VT220 supports this
                            kev.key_code = KeyCode::F19;
                            break;
                        case 34:
                            // VT220 supports this
                            kev.key_code = KeyCode::F20;
                            break;
                        default:
                            return Result::Error("key not supported");
                        }
                    } else if (functional == 'u') {
                        switch (key_val_unsh) {
                        case 9:
                            kev.key_code = KeyCode::TAB;
                            break;
                        case 13:
                            kev.key_code = KeyCode::ENTER;
                            break;
                        case 27:
                            kev.key_code = KeyCode::ESCAPE;
                            break;
                        case 127:
                            kev.key_code = KeyCode::BACKSPACE;
                            break;
                        case 57376:
                            kev.key_code = KeyCode::F13;
                            break;
                        case 57377:
                            kev.key_code = KeyCode::F14;
                            break;
                        case 57378:
                            kev.key_code = KeyCode::F15;
                            break;
                        case 57379:
                            kev.key_code = KeyCode::F16;
                            break;
                        case 57380:
                            kev.key_code = KeyCode::F17;
                            break;
                        case 57381:
                            kev.key_code = KeyCode::F18;
                            break;
                        case 57382:
                            kev.key_code = KeyCode::F19;
                            break;
                        case 57383:
                            kev.key_code = KeyCode::F20;
                            break;
                        case 57384:
                            kev.key_code = KeyCode::F21;
                            break;
                        case 57385:
                            kev.key_code = KeyCode::F22;
                            break;
                        case 57386:
                            kev.key_code = KeyCode::F23;
                            break;
                        case 57387:
                            kev.key_code = KeyCode::F24;
                            break;
                        case 57417:
                            kev.key_code = KeyCode::LEFT;
                            break;
                        case 57418:
                            kev.key_code = KeyCode::RIGHT;
                            break;
                        case 57419:
                            kev.key_code = KeyCode::UP;
                            break;
                        case 57420:
                            kev.key_code = KeyCode::DOWN;
                            break;
                        case 57421:
                            kev.key_code = KeyCode::PAGE_UP;
                            break;
                        case 57422:
                            kev.key_code = KeyCode::PAGE_DOWN;
                            break;
                        case 57423:
                            kev.key_code = KeyCode::HOME;
                            break;
                        case 57424:
                            kev.key_code = KeyCode::END;
                            break;
                        case 57425:
                            kev.key_code = KeyCode::INSERT;
                            break;
                        case 57426:
                            kev.key_code = KeyCode::DELETE;
                            break;
                        default:
                            kev.key_char = static_cast<char>(key_val_unsh);
                            $(get_key_code(kev.key_char, kev.key_code));
                            break;
                        }
                    } else {
                        kev.key_char = static_cast<char>(key_val_unsh);
                        $(get_key_code(kev.key_char, kev.key_code));
                    }
                }

                if (key_modifiers) {
                    const auto val = *key_modifiers;
                    if (val & 0b0000'0001)
                        kev.modifiers |= KEY_SHIFT;
                    if (val & 0b0000'0010)
                        kev.modifiers |= KEY_ALT;
                    if (val & 0b0000'0100)
                        kev.modifiers |= KEY_CTRL;
                    if (val & 0b0000'1000)
                        kev.modifiers |= KEY_SUPER;
                    if (val & 0b0010'0000)
                        kev.modifiers |= KEY_META;
                }

                event = kev;
                return Result::Ok;
            }
            default: {
                KeyCode key_code;
                $(get_key_code(current(), key_code));
                event = KeyEvent{
                        .key_down = true,
                        .key_code = key_code,
                        .key_char = current(),
                        .modifiers = 0,
                };
                return Result::Ok;
            }
            }
        }

        // CSI [ABCDHF]
        // ESC 'O' [PQRS]
        Result parse_key_legacy(Event &event) {
            // TODO: implement all legacy key codes
            $(expect(*ESC));

            if (match('O')) {
                KeyCode key_code;
                switch (advance()) {
                case 'P':
                    key_code = KeyCode::F1;
                    break;
                case 'Q':
                    key_code = KeyCode::F2;
                    break;
                case 'R':
                    key_code = KeyCode::F3;
                    break;
                case 'S':
                    key_code = KeyCode::F4;
                    break;
                default:
                    return Result::Error("key not supported");
                }
                event = KeyEvent{
                        .key_down = true,
                        .key_code = key_code,
                        .key_char = 0,
                        .modifiers = 0,
                };
                return Result::Ok;
            } else if (match('[')) {
                KeyCode key_code;
                switch (advance()) {
                case 'A':
                    key_code = KeyCode::UP;
                    break;
                case 'B':
                    key_code = KeyCode::DOWN;
                    break;
                case 'C':
                    key_code = KeyCode::RIGHT;
                    break;
                case 'D':
                    key_code = KeyCode::LEFT;
                    break;
                case 'H':
                    key_code = KeyCode::HOME;
                    break;
                case 'F':
                    key_code = KeyCode::END;
                    break;
                default:
                    return Result::Error("key not supported");
                }
                event = KeyEvent{
                        .key_down = true,
                        .key_code = key_code,
                        .key_char = 0,
                        .modifiers = 0,
                };
                return Result::Ok;
            }

            return Result::Error("key not supported");
        }

        bool parse(Event &event) {
            if (text == ESC) {
                advance();
                event = KeyEvent{
                        .key_down = true,
                        .key_code = KeyCode::ESCAPE,
                        .key_char = *ESC,
                        .modifiers = 0,
                };
                return true;
            }

            const size_t old_index = index;
            if (parse_mouse(event))
                return true;

            index = old_index;
            if (const auto result = parse_key_and_focus(event))
                return true;

            index = old_index;
            if (const auto result = parse_key_legacy(event))
                return true;
            return false;
        }

        Result expect_csi() {
            if (peek() == *ESC && peek(1) == '[') {
                advance();
                advance();
                return Result::Ok;
            } else
                return Result::Error("expected CSI sequence");
        }

        bool match_digit() {
            if (const char c = peek(); '0' <= c && c <= '9') {
                advance();
                return true;
            }
            return false;
        }

        template<std::integral Integer>
        bool match_number(Integer &number) {
            std::string str;
            while (str.size() <= 4 && match_digit())
                str += current();
            if (str.empty())
                return false;

            const auto result = std::from_chars<Integer>(str.data(), str.data() + str.size(), number, 10);
            if (result.ec == std::errc())
                return true;
            return false;
        }

        template<std::integral Integer>
        Result expect_number(Integer &number) {
            if (match_number(number))
                return Result::Ok;
            return Result::Error("expected number");
        }

        template<typename... Char>
            requires(std::same_as<Char, char> && ...)
        bool match_any(Char... chars) {
            const char p = peek();
            if (((p == chars) || ...)) {
                advance();
                return true;
            }
            return false;
        }

        bool match(char c) {
            if (peek() == c) {
                advance();
                return true;
            }
            return false;
        }

        Result expect(char c) {
            if (peek() == c) {
                advance();
                return Result::Ok;
            }
            return Result::Error("expected '{}' found '{}'", c, peek());
        }

        char current() const {
            if (index == 0 || index - 1 >= text.size())
                return PARSER_TERMINATOR;
            return text[index - 1];
        }

        char peek(const size_t i = 0) const {
            if (index + i >= text.size())
                return PARSER_TERMINATOR;
            return text[index + i];
        }

        char advance() {
            if (index >= text.size())
                return PARSER_TERMINATOR;
            return text[index++];
        }

      public:
        explicit Parser(const std::string &text) : text(text) {}

        ~Parser() = default;

        std::queue<Event> parse_events() {
            std::queue<Event> events;
            while (index < text.size()) {
                if (Event event; parse(event))
                    events.push(event);
            }
            return events;
        }
    };

    bool PollRawEvent(Event &event) {
        static std::queue<Event> pending_events = []() {
            // See: man 2 sigaction
            static struct sigaction sa {};
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sa.sa_handler = [](int) { pending_events.push(ResizeEvent{GetWindowSize()}); };
            sigaction(SIGWINCH, &sa, NULL);

            return std::queue<Event>();
        }();

        std::string text;
        if (con_read(text)) {
            std::queue<Event> events = Parser(text).parse_events();

            while (!events.empty()) {
                auto ev = events.front();
                pending_events.push(ev);
                if (const KeyEvent *kev = std::get_if<KeyEvent>(&ev)) {
                    KeyEvent kev_release = *kev;
                    kev_release.key_down = false;
                    pending_events.push(kev_release);
                }
                events.pop();
            }
        }

        if (!pending_events.empty()) {
            event = pending_events.front();
            pending_events.pop();
            return true;
        }
        return false;
    }

    Result get_key_code(char c, KeyCode &key_code) {
        switch (c) {
        case '\033':
            key_code = KeyCode::ESCAPE;
            return Result::Ok;
        case ' ':
            key_code = KeyCode::SPACE;
            return Result::Ok;
        case '!':
            key_code = KeyCode::BANG;
            return Result::Ok;
        case '@':
            key_code = KeyCode::AT;
            return Result::Ok;
        case '#':
            key_code = KeyCode::HASH;
            return Result::Ok;
        case '$':
            key_code = KeyCode::DOLLAR;
            return Result::Ok;
        case '%':
            key_code = KeyCode::PERCENT;
            return Result::Ok;
        case '^':
            key_code = KeyCode::CARET;
            return Result::Ok;
        case '&':
            key_code = KeyCode::AMPERSAND;
            return Result::Ok;
        case '*':
            key_code = KeyCode::ASTERISK;
            return Result::Ok;
        case '(':
            key_code = KeyCode::LPAREN;
            return Result::Ok;
        case ')':
            key_code = KeyCode::RPAREN;
            return Result::Ok;
        case '_':
            key_code = KeyCode::UNDERSCORE;
            return Result::Ok;
        case '+':
            key_code = KeyCode::PLUS;
            return Result::Ok;
        case '-':
            key_code = KeyCode::MINUS;
            return Result::Ok;
        case '=':
            key_code = KeyCode::EQUAL;
            return Result::Ok;
        case '{':
            key_code = KeyCode::LBRACE;
            return Result::Ok;
        case '}':
            key_code = KeyCode::RBRACE;
            return Result::Ok;
        case '[':
            key_code = KeyCode::LBRACKET;
            return Result::Ok;
        case ']':
            key_code = KeyCode::RBRACKET;
            return Result::Ok;
        case '|':
            key_code = KeyCode::PIPE;
            return Result::Ok;
        case '\\':
            key_code = KeyCode::BACKSLASH;
            return Result::Ok;
        case ':':
            key_code = KeyCode::COLON;
            return Result::Ok;
        case '"':
            key_code = KeyCode::DQUOTE;
            return Result::Ok;
        case ';':
            key_code = KeyCode::SEMICOLON;
            return Result::Ok;
        case '\'':
            key_code = KeyCode::SQUOTE;
            return Result::Ok;
        case '<':
            key_code = KeyCode::LESS;
            return Result::Ok;
        case '>':
            key_code = KeyCode::GREATER;
            return Result::Ok;
        case '?':
            key_code = KeyCode::HOOK;
            return Result::Ok;
        case ',':
            key_code = KeyCode::COMMA;
            return Result::Ok;
        case '.':
            key_code = KeyCode::PERIOD;
            return Result::Ok;
        case '/':
            key_code = KeyCode::SLASH;
            return Result::Ok;
        case '`':
            key_code = KeyCode::BQUOTE;
            return Result::Ok;
        case '~':
            key_code = KeyCode::TILDE;
            return Result::Ok;
        default:
            if ('a' <= c && c <= 'z') {
                key_code = static_cast<KeyCode>(c - 'a' + static_cast<int>(KeyCode::K_A));
                return Result::Ok;
            } else if ('A' <= c && c <= 'Z') {
                key_code = static_cast<KeyCode>(c - 'A' + static_cast<int>(KeyCode::K_A));
                return Result::Ok;
            } else if ('0' <= c && c <= '9') {
                key_code = static_cast<KeyCode>(c - '0' + static_cast<int>(KeyCode::K_0));
                return Result::Ok;
            }
        }
        return Result::Error("key not supported: 0x{:x}", c);
    }
}    // namespace nite::internal

#    endif
#endif
