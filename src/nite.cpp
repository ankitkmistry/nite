#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <cwchar>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <wchar.h>

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
#    include <csignal>
#    include <cstring>
#    include <charconv>
#    include <clocale>
#    include <concepts>
#endif

#define ESC "\033"
#define CSI ESC "["

#define $(expr)                                                                                                                                      \
    if (const auto result = (expr); !result)                                                                                                         \
    return result

#ifdef OS_WINDOWS
#    include <wchar.h>

static std::string wc_to_string(const wchar_t c) {
    // Create conversion state
    std::mbstate_t state;
    memset(&state, 0, sizeof(state));
    // Create buffer
    char buf[16];
    // Convert
    size_t len;
    wcrtomb_s(&len, buf, sizeof(buf), c, &state);
    if (len == static_cast<size_t>(-1))
        return "";
    return std::string(buf, len);
}

#else

static std::string wc_to_string(const wchar_t c) {
    // Create conversion state
    std::mbstate_t state;
    memset(&state, 0, sizeof(state));
    // Create the buffer
    char buf[16];
    // Convert
    size_t len = wcrtomb(buf, c, &state);
    if (len == static_cast<size_t>(-1))
        return "";
    return std::string(buf, len);
}

#endif

static inline constexpr bool is_print(char c) {
    return 32 <= c && c <= 126;
}

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

    struct State::StateImpl {
        bool closed = false;

        // Events mechanism
        std::unordered_map<KeyCode, KeyState> key_states;

        std::array<BtnState, 4> btn_states;
        static_assert(4 == static_cast<int>(MouseButton::RIGHT) + 1, "Size of the array must match");

        Position mouse_pos;
        intmax_t mouse_scroll_v = 0;
        intmax_t mouse_scroll_h = 0;

        // Render mechanism
        std::chrono::duration<double> delta_time;
        std::queue<internal::CellBuffer> swapchain;
        std::stack<std::unique_ptr<internal::Box>> selected_stack;

        StateImpl() = default;

        internal::Box &get_selected() {
            return *selected_stack.top();
        }

        bool set_cell(size_t col, size_t row, wchar_t value, const Style style) {
            internal::Box &selected = get_selected();
            if (!selected.transform(col, row))
                return false;
            if (!selected.contains(col, row))
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

        internal::Cell &get_cell(size_t col, size_t row) {
            static internal::Cell sentinel;

            internal::Box &selected = get_selected();
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

    Size GetBufferSize(State &state) {
        return state.impl->swapchain.back().size();
    }

    Size GetPaneSize(State &state) {
        return state.impl->get_selected().get_size();
    }

    double GetDeltaTime(State &state) {
        return state.impl->delta_time.count();
    }

    bool ShouldWindowClose(State &state) {
        return state.impl->closed;
    }

    Result Initialize(State &state) {
        if (!internal::console::is_tty())
            return "cannot initialize in a non-terminal environment";
        if (const auto result = internal::console::init(); !result)
            return result;

        state.impl->closed = false;
        return true;
    }

    Result Cleanup() {
        return internal::console::restore();
    }

    void BeginDrawing(State &state) {
        internal::CellBuffer buf(GetWindowSize());
        state.impl->swapchain.push(buf);
        state.impl->selected_stack.push(std::make_unique<internal::StaticBox>(Position{.col = 0, .row = 0}, buf.size()));
    }

    void EndDrawing(State &state) {
        state.impl->mouse_scroll_v = 0;
        state.impl->mouse_scroll_h = 0;
        for (BtnState &btn_state: state.impl->btn_states)
            btn_state = {};

        state.impl->selected_stack.pop();

        switch (state.impl->swapchain.size()) {
        case 0:
            break;
        case 1: {
            const auto start = std::chrono::high_resolution_clock::now();

            const auto &cur_buf = state.impl->swapchain.front();
            const auto cur_size = cur_buf.size();

            for (size_t row = 0; row < cur_size.height; row++) {
                for (size_t col = 0; col < cur_size.width; col++) {
                    const auto &cell = cur_buf.at(col, row);
                    internal::console::set_cell(col, row, cell.value, cell.style);
                }
            }

            const auto end = std::chrono::high_resolution_clock::now();
            state.impl->delta_time = end - start;
            break;
        }
        default: {
            const auto start = std::chrono::high_resolution_clock::now();

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

            const auto end = std::chrono::high_resolution_clock::now();
            state.impl->delta_time = end - start;
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
        internal::Box &selected = state.impl->get_selected();

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
        internal::Box &selected = state.impl->get_selected();

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
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_selected()); no_box) {
            state.impl->selected_stack.push(std::make_unique<internal::NoBox>(*no_box));
            return;
        }

        state.impl->selected_stack.push(std::make_unique<internal::StaticBox>(state.impl->get_selected().get_pos() + top_left, size));
    }

    static void scroll_vertical(Position &pivot, ScrollPaneInfo &info, intmax_t value) {
        if (!info.show_vscroll_bar)
            return;

        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_vscroll) {
            const intmax_t abs_value = std::abs(value);
            for (intmax_t i = 0; i < abs_value; i++)
                info.on_vscroll(std::forward<ScrollPaneInfo &>(info));
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

    static void scroll_horizontal(Position &pivot, ScrollPaneInfo &info, intmax_t value) {
        if (!info.show_hscroll_bar)
            return;

        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_hscroll) {
            const intmax_t abs_value = std::abs(value);
            for (intmax_t i = 0; i < abs_value; i++)
                info.on_hscroll(std::forward<ScrollPaneInfo &>(info));
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

    void BeginScrollPane(State &state, Position &pivot, ScrollPaneInfo info) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_selected()); no_box) {
            state.impl->selected_stack.push(std::make_unique<internal::NoBox>(*no_box));
            return;
        }

        const auto mouse_pos = GetMousePosition(state) - state.impl->get_selected().get_pos();
        if (internal::StaticBox(state.impl->get_selected().get_pos() + info.pos, info.min_size).contains(mouse_pos)) {
            scroll_horizontal(pivot, info, GetMouseScrollH(state));
            scroll_vertical(pivot, info, GetMouseScrollV(state));
        }

        const auto left_scroll_btn = Position{.col = 0, .row = info.min_size.height - 1} + info.pos;
        const auto right_scroll_btn = Position{.col = info.min_size.width - 2, .row = info.min_size.height - 1} + info.pos;
        const auto top_scroll_btn = Position{.col = info.min_size.width - 1, .row = 0} + info.pos;
        const auto bottom_scroll_btn = Position{.col = info.min_size.width - 1, .row = info.min_size.height - 2} + info.pos;
        const auto home_cell_btn = Position{.col = info.min_size.width - 1, .row = info.min_size.height - 1} + info.pos;

        if (mouse_pos == left_scroll_btn)
            scroll_horizontal(pivot, info, -(GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == right_scroll_btn)
            scroll_horizontal(pivot, info, (GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == top_scroll_btn)
            scroll_vertical(pivot, info, -(GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == bottom_scroll_btn)
            scroll_vertical(pivot, info, (GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == home_cell_btn && (IsMouseClicked(state, MouseButton::LEFT) || IsMouseDoubleClicked(state, MouseButton::LEFT)))
            pivot = {};

        state.impl->selected_stack.push(
                std::make_unique<internal::ScrollBox>(
                        info.show_scroll_home, info.show_hscroll_bar, info.show_vscroll_bar, info.scroll_bar,
                        state.impl->get_selected().get_pos() + info.pos, pivot, info.min_size, info.max_size
                )
        );
    }

    void BeginGridPane(State &state, GridPaneInfo info) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_selected()); no_box) {
            state.impl->selected_stack.push(std::make_unique<internal::NoBox>(*no_box));
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

        state.impl->selected_stack.push(
                std::make_unique<internal::GridBox>(state.impl->get_selected().get_pos() + info.pos, info.size, num_cols, num_rows, grid)
        );
    }

    void BeginGridCell(State &state, size_t col, size_t row) {
        if (const auto no_box = dynamic_cast<internal::NoBox *>(&state.impl->get_selected()); no_box) {
            state.impl->selected_stack.push(std::make_unique<internal::NoBox>(*no_box));
            return;
        }
        if (const auto grid_box = dynamic_cast<internal::GridBox *>(&state.impl->get_selected()); grid_box)
            state.impl->selected_stack.push(grid_box->get_grid_cell(col, row));
        else
            state.impl->selected_stack.push(std::make_unique<internal::NoBox>());
    }

    void BeginNoPane(State &state) {
        state.impl->selected_stack.push(std::make_unique<internal::NoBox>());
    }

    void EndPane(State &state) {
        const internal::Box &box = state.impl->get_selected();
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
            if (scroll_box->show_vscroll_bar() && min_size.height != max_size.height) {
                const auto vscroll_start = Position{.col = min_size.width - 1, .row = 1} + pivot;
                const auto vscroll_end = Position{.col = min_size.width - 1, .row = min_size.height - 2} + pivot;
                DrawLine(state, vscroll_start, vscroll_end, scroll.v_bar.value, scroll.v_bar.style);

                const auto top_scroll_btn = Position{.col = min_size.width - 1, .row = 0} + pivot;
                const auto bottom_scroll_btn = Position{.col = min_size.width - 1, .row = min_size.height - 2} + pivot;
                SetCell(state, scroll.top.value, top_scroll_btn, scroll.top.style);
                SetCell(state, scroll.bottom.value, bottom_scroll_btn, scroll.bottom.style);

                const size_t row = (double) pivot.row / max_size.height * (vscroll_end.row - vscroll_start.row);
                const auto node_start = Position{.col = min_size.width - 1, .row = row + 1} + pivot;
                const size_t max_row = (double) (max_size.height - min_size.height - 1) / max_size.height * (vscroll_end.row - vscroll_start.row);
                const size_t node_height = (vscroll_end.row - vscroll_start.row) - max_row;
                DrawLine(state, node_start, {.col = node_start.col, .row = node_start.row + node_height}, scroll.v_node.value, scroll.v_node.style);
            }
            // Horizontal scroll
            if (scroll_box->show_hscroll_bar() && min_size.width != max_size.width) {
                const auto hscroll_start = Position{.col = 1, .row = min_size.height - 1} + pivot;
                const auto hscroll_end = Position{.col = min_size.width - 2, .row = min_size.height - 1} + pivot;
                DrawLine(state, hscroll_start, hscroll_end, scroll.h_bar.value, scroll.h_bar.style);

                const auto left_scroll_btn = Position{.col = 0, .row = min_size.height - 1} + pivot;
                const auto right_scroll_btn = Position{.col = min_size.width - 2, .row = min_size.height - 1} + pivot;
                SetCell(state, scroll.left.value, left_scroll_btn, scroll.left.style);
                SetCell(state, scroll.right.value, right_scroll_btn, scroll.right.style);

                const size_t col = (double) pivot.col / max_size.width * (hscroll_end.col - hscroll_start.col);
                const auto node_start = Position{.col = col + 1, .row = min_size.height - 1} + pivot;
                const size_t max_col = (double) (max_size.width - min_size.width - 1) / max_size.width * (hscroll_end.col - hscroll_start.col);
                const size_t node_width = (hscroll_end.col - hscroll_start.col) - max_col;
                DrawLine(state, node_start, {.col = node_start.col + node_width, .row = node_start.row}, scroll.h_node.value, scroll.h_node.style);
            }
        }
        state.impl->selected_stack.pop();
    }

    void DrawBorder(State &state, const Border &border) {
        const internal::Box &selected = state.impl->get_selected();

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

    void DrawHDivider(State &state, size_t row, wchar_t value, Style style) {
        DrawLine(state, {.col = 0, .row = row}, {.col = state.impl->get_selected().get_size().width, .row = row}, value, style);
    }

    void DrawVDivider(State &state, size_t col, wchar_t value, Style style) {
        DrawLine(state, {.col = col, .row = 0}, {.col = col, .row = state.impl->get_selected().get_size().height}, value, style);
    }

    void Text(State &state, TextInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.text.size(), .height = 1})
                    .contains(GetMousePosition(state) - state.impl->get_selected().get_pos())) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::forward<TextInfo &>(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::forward<TextInfo &>(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::forward<TextInfo &>(info));
            } else if (info.on_hover)
                info.on_hover(std::forward<TextInfo &>(info));
        }

        for (size_t i = 0; char c: info.text) {
            state.impl->set_cell(info.pos.col + i, info.pos.row, c, info.style);
            i++;
        }
    }

    void TextBox(State &state, TextBoxInfo info) {
        if (internal::StaticBox(info.pos, info.size).contains(GetMousePosition(state) - state.impl->get_selected().get_pos())) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::forward<TextBoxInfo &>(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::forward<TextBoxInfo &>(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::forward<TextBoxInfo &>(info));
            } else if (info.on_hover)
                info.on_hover(std::forward<TextBoxInfo &>(info));
        }

        std::vector<std::string> lines;
        std::istringstream ss(info.text);
        for (std::string line; std::getline(ss, line);)
            if (info.wrap)
                for (size_t i = 0; i < line.size(); i += info.size.width)
                    lines.push_back(line.substr(i, info.size.width));
            else
                lines.push_back(line);

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

    void ProgressBar(State &state, ProgressBarInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.length, .height = 1})
                    .contains(GetMousePosition(state) - state.impl->get_selected().get_pos())) {
            if (size_t count = GetMouseClickCount(state, MouseButton::LEFT); count > 0) {
                if (info.on_click)
                    while (count--)
                        info.on_click(std::forward<ProgressBarInfo &>(info));
            } else if (size_t count = GetMouseClickCount(state, MouseButton::RIGHT); count > 0) {
                if (info.on_menu)
                    while (count--)
                        info.on_menu(std::forward<ProgressBarInfo &>(info));
            } else if (size_t count = GetMouseClick2Count(state, MouseButton::LEFT); count > 0) {
                if (info.on_click2)
                    while (count--)
                        info.on_click2(std::forward<ProgressBarInfo &>(info));
            } else if (info.on_hover)
                info.on_hover(std::forward<ProgressBarInfo &>(info));
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

    void SimpleTable(State &state, SimpleTableInfo info) {
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
                const auto &cell_text = info.data[row * info.num_cols + col];
                if (cell_text.size() + 1 >= max)
                    max = cell_text.size() + 1;
            }
            total_width += max_col_widths[col] = max;
        }

        Size size = {.width = total_width, .height = info.num_rows};
        BeginPane(state, info.pos, size);

        if (info.show_border)
            ;    // TODO: implement this
        else {
            size_t y = 0;
            for (size_t row = 0; row < info.num_rows; row++) {
                size_t x = 0;
                for (size_t col = 0; col < info.num_cols; col++) {
                    const auto &cell_text = info.data[row * info.num_cols + col];
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
        }

        EndPane(state);
    }

    template<class... Ts>
    struct overloaded : Ts... {
        using Ts::operator()...;
    };
    // Some compilers might require this explicit deduction guide
    template<class... Ts>
    overloaded(Ts...) -> overloaded<Ts...>;

    bool PollEvent(State &state, Event &event) {
        if (internal::PollRawEvent(event)) {
            std::visit(
                    overloaded{
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
                            },
                            [](const auto &) {}
                    },
                    event
            );
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

    Position GetMousePosition(const State &state) {
        return state.impl->mouse_pos;
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
        // Refer to https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
        if (style.mode & STYLE_RESET)
            print(CSI "0m");
        if (style.mode & STYLE_BOLD)
            print(CSI "1m");
        if (style.mode & STYLE_UNDERLINE)
            print(CSI "4m");
        if (style.mode & STYLE_INVERSE)
            print(CSI "7m");

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

    void set_cell(const size_t col, const size_t row, const wchar_t value, const Style style) {
        gotoxy(col, row);
        set_style(style);
        // std::fputwc(value, stdout);
        print(wc_to_string(value));
    }
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
        return std::string{static_cast<const char *>(err_msg_buf), size};
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
            return std::format("error getting console info: {}", get_last_error());
        if (!FillConsoleOutputCharacter(h_con, ' ', csbi.dwSize.X * csbi.dwSize.Y, coord_screen, &chars_written))
            return std::format("error writing to console: {}", get_last_error());

        // Set the buffer attributes.
        if (!GetConsoleScreenBufferInfo(h_con, &csbi))
            return std::format("error getting console info: {}", get_last_error());
        if (!FillConsoleOutputAttribute(
                    h_con, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, csbi.dwSize.X * csbi.dwSize.Y, coord_screen, &chars_written
            ))
            return std::format("error setting console color: {}", get_last_error());

        // Put the cursor in the top left corner.
        if (!SetConsoleCursorPosition(h_con, coord_screen))
            return std::format("error setting console position: {}", get_last_error());
        return true;
    }

    Result size(size_t &width, size_t &height) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(h_con, &info))
            return std::format("error getting console size: {}", get_last_error());

        const int columns = info.srWindow.Right - info.srWindow.Left + 1;
        const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        width = columns > 0 ? columns : 0;
        height = rows > 0 ? rows : 0;
        return true;
    }

    Result print(const std::string &text) {
        static const HANDLE h_con = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!WriteConsole(h_con, text.c_str(), text.size(), NULL, NULL))
            return std::format("error printing to console: {}", get_last_error());
        return true;
    }

    static DWORD old_in_mode = 0, old_out_mode = 0;
    static UINT old_console_cp = 0;
    static std::string old_locale;

    Result init() {
        static const HANDLE h_conin = GetStdHandle(STD_INPUT_HANDLE);
        static const HANDLE h_conout = GetStdHandle(STD_OUTPUT_HANDLE);

        if (!GetConsoleMode(h_conout, &old_out_mode))
            return std::format("error getting console out mode: {}", get_last_error());
        if (!GetConsoleMode(h_conin, &old_in_mode))
            return std::format("error getting console in mode: {}", get_last_error());
        if ((old_console_cp = GetConsoleOutputCP()) == 0)
            return std::format("error getting console code page: {}", get_last_error());

        DWORD out_mode = 0;
        out_mode |= ENABLE_PROCESSED_OUTPUT;
        out_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        out_mode |= DISABLE_NEWLINE_AUTO_RETURN;
        if (!SetConsoleMode(h_conout, out_mode))
            return std::format("error setting console in mode: {}", get_last_error());

        DWORD in_mode = 0;
        in_mode |= ENABLE_EXTENDED_FLAGS;
        in_mode |= ENABLE_MOUSE_INPUT;
        in_mode |= ENABLE_WINDOW_INPUT;
        in_mode &= ~ENABLE_QUICK_EDIT_MODE;
        if (!SetConsoleMode(h_conin, in_mode))
            return std::format("error setting console out mode: {}", get_last_error());

        if (!SetConsoleOutputCP(CP_UTF8))
            return std::format("error setting console code page: {}", get_last_error());

        // Set locale to utf-8
        old_locale = std::setlocale(LC_CTYPE, NULL);
        std::setlocale(LC_CTYPE, "en_US.utf8");

        $(print(CSI "?1049h"));    // Enter alternate buffer
        $(print(CSI "?25l"));      // Hide console cursor
        $(print(CSI "?30l"));      // Do not show scroll bar

        $(clear());
        return true;
    }

    Result restore() {
        $(clear());

        $(print(CSI "?30h"));      // Show scroll bar
        $(print(CSI "?25h"));      // Show console cursor
        $(print(CSI "?1049l"));    // Exit alternate buffer

        // Restore locale
        std::setlocale(LC_CTYPE, old_locale.c_str());

        if (!SetConsoleOutputCP(old_console_cp))
            return std::format("error setting console code page: {}", get_last_error());
        if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), old_in_mode))
            return std::format("error setting console in mode: {}", get_last_error());
        if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), old_out_mode))
            return std::format("error setting console out mode: {}", get_last_error());
        return true;
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
#    include <unistd.h>
#    include <termios.h>
#    include <sys/ioctl.h>
#    include <bits/types/struct_timeval.h>
#    include <sys/select.h>
#    include <sys/types.h>

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
        return true;
    }

    Result size(size_t &width, size_t &height) {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
            return std::format("error getting console size: {}", get_last_error());
        width = w.ws_col;
        height = w.ws_row;
        return true;
    }

    Result print(const std::string &text) {
        if (write(STDOUT_FILENO, text.data(), text.size()) == -1)
            return std::format("error writing to the console: {}", get_last_error());
        return true;
    }

    static struct termios old_term, new_term;
    static std::string old_locale;

    Result init() {
        // Refer to: man 3 termios
        if (tcgetattr(STDIN_FILENO, &old_term) == -1)
            return std::format("error getting terminal attributes: {}", get_last_error());
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
            return std::format("error setting terminal attributes: {}", get_last_error());

        // Set locale to utf-8
        old_locale = std::setlocale(LC_CTYPE, NULL);
        std::setlocale(LC_CTYPE, "en_US.utf8");

        $(print(CSI "?1049h"));    // Enter alternate buffer
        $(print(CSI "?25l"));      // Hide console cursor
        $(clear());

        // Refer to: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Functions-using-CSI-_-ordered-by-the-final-character_s_
        // Refer to: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h3-Alt-and-Meta-Keys
        // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol

        // Enable kitty keyboard protocol
        // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol/#progressive-enhancement
        // flags = 1 | 4 = 5
        $(print(CSI ">5u"));

        $(print(CSI "?1039h"));    // Send ESC when Alt modifies a key

        // Mouse specific
        $(print(CSI "?1000h"));    // Send Mouse X & Y on button press and release
        $(print(CSI "?1002h"));    // Use Cell Motion Mouse Tracking (move and drag tracking)
        $(print(CSI "?1003h"));    // Use All Motion Mouse Tracking
        $(print(CSI "?1006h"));    // Enable SGR Mouse Mode
        // Other specific
        $(print(CSI "?1004h"));    // Send FocusIn/FocusOut events
        $(print(CSI "?30l"));      // Do not show scroll bar

        return true;
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
        // Keyboard specific
        $(print(CSI "?1039h"));    // Do not Send ESC when Alt modifies a key

        // Disable kitty keyboard protocol
        $(print(CSI "<u"));

        $(clear());
        $(print(CSI "?25h"));      // Show console cursor
        $(print(CSI "?1049l"));    // Exit alternate buffer

        // Restore locale
        std::setlocale(LC_CTYPE, old_locale.c_str());
        // Restore old terminal modes
        if (tcsetattr(STDIN_FILENO, TCSANOW, &old_term) == -1)
            return std::format("error setting terminal attributes: {}", get_last_error());
        return true;
    }
}    // namespace nite::internal::console

#endif

#ifdef OS_LINUX
namespace nite::internal
{
    Result get_key_code(char c, KeyCode &key_code);

    bool con_read(std::string &str) {
        static char buf[256];
        memset(buf, 0, sizeof(buf));

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 2 * 1000;

        int ret = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (ret == -1)
            return false;    // Call failed
        if (ret == 0)
            return false;    // Nothing available

        ssize_t len = read(STDIN_FILENO, buf, sizeof(buf));
        if (len == -1)
            return false;    // Call failed
        if (len == 0)
            return false;    // Nothing available

        str = std::string(buf, len);
        return true;
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

#    define PARSER_TERMINATOR '\n'

    class Parser {
        size_t index = 0;
        std::string text;

        // Previous mouse click time point
        inline static auto prev_mcl_tp = std::chrono::high_resolution_clock::time_point();

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
                return "expected 'm' or 'M'";

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
                    return false;
                kind = MouseEventKind::SCROLL_LEFT;
                break;
            case 7:
                if (dragging)
                    return false;
                kind = MouseEventKind::SCROLL_RIGHT;
                break;
            default:
                return false;
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
            return true;
        }

        // CSI NUMBER (':' NUMBER? (':' NUMBER?)?)? (';' NUMBER?)? [ABCDEFHPQSu~]
        // 0x0d         -> Enter key
        // 0x7f | 0x08  -> Backspace
        // 0x09         -> Tab
        // any printable char
        Result parse_key_and_focus(Event &event) {
            if (peek() != ESC) {
                switch (advance()) {
                case 0x0d:
                    event = KeyEvent{
                            .key_down = true,
                            .key_code = KeyCode::ENTER,
                            .key_char = current(),
                            .modifiers = 0,
                    };
                    return true;
                case 0x7f:
                case 0x08:
                    event = KeyEvent{
                            .key_down = true,
                            .key_code = KeyCode::BACKSPACE,
                            .key_char = current(),
                            .modifiers = 0,
                    };
                    return true;
                case 0x09:
                    event = KeyEvent{
                            .key_down = true,
                            .key_code = KeyCode::TAB,
                            .key_char = current(),
                            .modifiers = 0,
                    };
                    return true;
                default: {
                    KeyCode key_code;
                    $(get_key_code(current(), key_code));
                    event = KeyEvent{
                            .key_down = true,
                            .key_code = key_code,
                            .key_char = current(),
                            .modifiers = 0,
                    };
                    return true;
                }
                }
            } else if (peek(1) != '[') {
                advance();
                event = KeyEvent{
                        .key_down = true,
                        .key_code = KeyCode::ESCAPE,
                        .key_char = ESC,
                        .modifiers = 0,
                };
                return true;
            }

            int key_val_unsh = 0;              // unshifted key val
            std::optional<char> key_val_sh;    // shifted key val
            std::optional<uint8_t> key_modifiers;

            $(expect_csi());

            if (match('O')) {
                event = FocusEvent{.focus_gained = false};
                return true;
            } else if (match('I')) {
                event = FocusEvent{.focus_gained = true};
                return true;
            }

            $(expect_number(key_val_unsh));

            if (match(':')) {
                if (char val; match_number(val))
                    key_val_sh = val;
                if (match(':')) {
                    size_t val;
                    match_number(val);    // base layout key (ignored)
                }
            }

            if (match(';')) {
                // Refer to: https://sw.kovidgoyal.net/kitty/keyboard-protocol/#modifiers
                if (uint8_t val; match_number(val))
                    key_modifiers = saturated_sub(val, 1);
            }

            char functional;
            if (match_any('A', 'B', 'C', 'D', 'E', 'F', 'H', 'P', 'Q', 'S', 'u', '~'))
                functional = current();
            else
                return "expected one of 'ABCDEFHPQSu~'";

            KeyCode key_code;
            char key_char = 0;
            uint8_t modifiers = 0;

            if (key_val_sh) {
                key_char = *key_val_sh;
                $(get_key_code(key_char, key_code));
            } else {
                const int key_val = key_val_unsh;
                if (key_val == 1) {
                    switch (functional) {
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
                    case 'F':
                        key_code = KeyCode::END;
                        break;
                    case 'H':
                        key_code = KeyCode::HOME;
                        break;
                    case 'P':
                        key_code = KeyCode::F1;
                        break;
                    case 'Q':
                        key_code = KeyCode::F2;
                        break;
                    case 'S':
                        key_code = KeyCode::F4;
                        break;
                    default:
                        return "key not supported";
                    }
                } else if (functional == '~') {
                    switch (key_val) {
                    case 2:
                        key_code = KeyCode::INSERT;
                        break;
                    case 3:
                        key_code = KeyCode::DELETE;
                        break;
                    case 5:
                        key_code = KeyCode::PAGE_UP;
                        break;
                    case 6:
                        key_code = KeyCode::PAGE_DOWN;
                        break;
                    case 7:
                        key_code = KeyCode::HOME;
                        break;
                    case 8:
                        key_code = KeyCode::END;
                        break;
                    case 11:
                        key_code = KeyCode::F1;
                        break;
                    case 12:
                        key_code = KeyCode::F2;
                        break;
                    case 13:
                        key_code = KeyCode::F3;
                        break;
                    case 14:
                        key_code = KeyCode::F4;
                        break;
                    case 15:
                        key_code = KeyCode::F5;
                        break;
                    case 17:
                        key_code = KeyCode::F6;
                        break;
                    case 18:
                        key_code = KeyCode::F7;
                        break;
                    case 19:
                        key_code = KeyCode::F8;
                        break;
                    case 20:
                        key_code = KeyCode::F9;
                        break;
                    case 21:
                        key_code = KeyCode::F10;
                        break;
                    case 23:
                        key_code = KeyCode::F11;
                        break;
                    case 24:
                        key_code = KeyCode::F12;
                        break;
                    default:
                        return "key not supported";
                    }
                } else if (functional == 'u') {
                    switch (key_val) {
                    case 9:
                        key_code = KeyCode::TAB;
                        break;
                    case 13:
                        key_code = KeyCode::ENTER;
                        break;
                    case 27:
                        key_code = KeyCode::ESCAPE;
                        break;
                    case 127:
                        key_code = KeyCode::BACKSPACE;
                        break;
                    case 57376:
                        key_code = KeyCode::F13;
                        break;
                    case 57377:
                        key_code = KeyCode::F14;
                        break;
                    case 57378:
                        key_code = KeyCode::F15;
                        break;
                    case 57379:
                        key_code = KeyCode::F16;
                        break;
                    case 57380:
                        key_code = KeyCode::F17;
                        break;
                    case 57381:
                        key_code = KeyCode::F18;
                        break;
                    case 57382:
                        key_code = KeyCode::F19;
                        break;
                    case 57383:
                        key_code = KeyCode::F20;
                        break;
                    case 57384:
                        key_code = KeyCode::F21;
                        break;
                    case 57385:
                        key_code = KeyCode::F22;
                        break;
                    case 57386:
                        key_code = KeyCode::F23;
                        break;
                    case 57387:
                        key_code = KeyCode::F24;
                        break;
                    case 57417:
                        key_code = KeyCode::LEFT;
                        break;
                    case 57418:
                        key_code = KeyCode::RIGHT;
                        break;
                    case 57419:
                        key_code = KeyCode::UP;
                        break;
                    case 57420:
                        key_code = KeyCode::DOWN;
                        break;
                    case 57421:
                        key_code = KeyCode::PAGE_UP;
                        break;
                    case 57422:
                        key_code = KeyCode::PAGE_DOWN;
                        break;
                    case 57423:
                        key_code = KeyCode::HOME;
                        break;
                    case 57424:
                        key_code = KeyCode::END;
                        break;
                    case 57425:
                        key_code = KeyCode::INSERT;
                        break;
                    case 57426:
                        key_code = KeyCode::DELETE;
                        break;
                    default:
                        key_char = static_cast<char>(key_val);
                        $(get_key_code(key_char, key_code));
                        break;
                    }
                } else {
                    key_char = static_cast<char>(key_val);
                    $(get_key_code(key_char, key_code));
                }
            }

            if (key_modifiers) {
                const auto val = *key_modifiers;
                if (val & 0b0000'0001)
                    modifiers |= KEY_SHIFT;
                if (val & 0b0000'0010)
                    modifiers |= KEY_ALT;
                if (val & 0b0000'0100)
                    modifiers |= KEY_CTRL;
                if (val & 0b0000'1000)
                    modifiers |= KEY_SUPER;
                if (val & 0b0010'0000)
                    modifiers |= KEY_META;
            }

            event = KeyEvent{
                    .key_down = true,
                    .key_code = key_code,
                    .key_char = key_char,
                    .modifiers = modifiers,
            };

            return true;
        }

        bool parse(Event &event) {
            const size_t old_index = index;
            if (parse_mouse(event))
                return true;

            index = old_index;
            if (const auto result = parse_key_and_focus(event))
                return true;
            return false;
        }

        Result expect_csi() {
            if (peek() == ESC && peek(1) == '[') {
                advance();
                advance();
                return true;
            } else
                return "expected CSI sequence";
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
                return true;
            return "expected number";
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
                return true;
            }
            return std::format("expected '{}' found '{}'", c, peek());
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

        std::vector<Event> parse_events() {
            std::vector<Event> events;
            while (index < text.size()) {
                if (Event event; parse(event))
                    events.push_back(event);
            }
            return events;
        }
    };

    bool PollRawEvent(Event &event) {
        static std::vector<Event> pending_events = []() {
            // See: man 2 sigaction
            static struct sigaction sa{};
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sa.sa_handler = [](int) { pending_events.push_back(ResizeEvent{GetWindowSize()}); };
            sigaction(SIGWINCH, &sa, NULL);

            return std::vector<Event>();
        }();

        if (!pending_events.empty()) {
            event = pending_events.back();
            pending_events.pop_back();
            return true;
        }

        std::string text;
        if (!con_read(text))
            return false;

        std::vector<Event> events = Parser(text).parse_events();
        if (events.empty()) {
            return false;
            // event = DebugEvent{.text = std::format("Could not parse: {}", text)};
            // return true;
        }
        event = events.front();
        if (const KeyEvent *kev = std::get_if<KeyEvent>(&event)) {
            KeyEvent kev_release = *kev;
            kev_release.key_down = false;
            pending_events.push_back(kev_release);
        }
        events.erase(events.begin());
        for (const Event &event: events)
            pending_events.push_back(event);

        return true;
    }

    Result get_key_code(char c, KeyCode &key_code) {
        switch (c) {
        case '\033':
            key_code = KeyCode::ESCAPE;
            return true;
        case ' ':
            key_code = KeyCode::SPACE;
            return true;
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
        case '_':
            key_code = KeyCode::UNDERSCORE;
            return true;
        case '+':
            key_code = KeyCode::PLUS;
            return true;
        case '-':
            key_code = KeyCode::MINUS;
            return true;
        case '=':
            key_code = KeyCode::EQUAL;
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
        case '|':
            key_code = KeyCode::PIPE;
            return true;
        case '\\':
            key_code = KeyCode::BACKSLASH;
            return true;
        case ':':
            key_code = KeyCode::COLON;
            return true;
        case '"':
            key_code = KeyCode::DQUOTE;
            return true;
        case ';':
            key_code = KeyCode::SEMICOLON;
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
        case ',':
            key_code = KeyCode::COMMA;
            return true;
        case '.':
            key_code = KeyCode::PERIOD;
            return true;
        case '/':
            key_code = KeyCode::SLASH;
            return true;
        case '`':
            key_code = KeyCode::BQUOTE;
            return true;
        case '~':
            key_code = KeyCode::TILDE;
            return true;
        default:
            if ('a' <= c && c <= 'z') {
                key_code = static_cast<KeyCode>(c - 'a' + static_cast<int>(KeyCode::K_A));
                return true;
            } else if ('A' <= c && c <= 'Z') {
                key_code = static_cast<KeyCode>(c - 'A' + static_cast<int>(KeyCode::K_A));
                return true;
            } else if ('0' <= c && c <= '9') {
                key_code = static_cast<KeyCode>(c - '0' + static_cast<int>(KeyCode::K_0));
                return true;
            }
        }
        return std::format("key not supported: 0x{:x}", c);
    }
}    // namespace nite::internal
#endif
