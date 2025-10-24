#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cwchar>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <unordered_map>
#include <variant>

#include "nite.hpp"

#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__TOS_WIN__) || defined(__WINDOWS__)
#    define OS_WINDOWS
#else
#    error "Unsupported platform"
#endif

#define null (nullptr)

static inline constexpr bool is_print(char c) {
    return 32 <= c && c <= 126;
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

        class ViewBox : public Box {
            bool scroll_bar;
            ScrollBar scroll_style;
            Position pos;
            Position pivot;
            Size min_size;
            Size max_size;

          public:
            ViewBox(bool show_scroll_bar, const ScrollBar &scroll_style, const Position pos, const Position pivot, const Size min_size,
                    const Size max_size)
                : scroll_bar(show_scroll_bar), scroll_style(scroll_style), pos(pos), pivot(pivot), min_size(min_size), max_size(max_size) {}

            ViewBox() = default;
            ~ViewBox() = default;

            bool show_scroll_bar() const {
                return scroll_bar;
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

        std::array<BtnState, 3> btn_states;
        static_assert(3 == static_cast<int>(MouseButton::RIGHT) + 1, "Size of the array must match");

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
        for (BtnState &state: state.impl->btn_states)
            state = {};

        state.impl->selected_stack.pop();

        switch (state.impl->swapchain.size()) {
        case 0:
            break;
        case 1: {
            auto start = std::chrono::high_resolution_clock::now();

            auto &cur_buf = state.impl->swapchain.front();
            const auto cur_size = cur_buf.size();

            for (size_t row = 0; row < cur_size.height; row++) {
                for (size_t col = 0; col < cur_size.width; col++) {
                    const auto &cell = cur_buf.at(col, row);
                    internal::console::set_cell(col, row, cell.value, cell.style);
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            state.impl->delta_time = end - start;
            break;
        }
        default: {
            auto start = std::chrono::high_resolution_clock::now();

            auto prev_buf = std::move(state.impl->swapchain.front());
            const auto prev_size = prev_buf.size();
            state.impl->swapchain.pop();

            auto &cur_buf = state.impl->swapchain.front();
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

            auto end = std::chrono::high_resolution_clock::now();
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
        else {
            for (size_t col = col_start; col < col_end; col++) {
                const double row = std::lerp(row_start, row_end, (col - col_start) / std::abs(1. * col_end - col_start));
                state.impl->set_cell(col, row, fill, style);
            }
        }
    }

    void BeginPane(State &state, const Position top_left, const Size size) {
        state.impl->selected_stack.push(std::make_unique<internal::StaticBox>(state.impl->get_selected().get_pos() + top_left, size));
    }

    static void scroll_vertical(Position &pivot, ScrollPaneInfo &info, intmax_t value) {
        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_vscroll)
            info.on_vscroll(std::forward<ScrollPaneInfo &>(info));
        if (value > 0) {
            pivot.row += value;
        } else if (value < 0) {
            if (static_cast<size_t>(-value) <= pivot.row)
                pivot.row += value;
        }
        if (pivot.row >= info.max_size.height - info.min_size.height)
            pivot.row = info.max_size.height - info.min_size.height - 1;
    }

    static void scroll_horizontal(Position &pivot, ScrollPaneInfo &info, intmax_t value) {
        value *= info.scroll_factor;
        if (value == 0)
            return;
        if (info.on_hscroll)
            info.on_hscroll(std::forward<ScrollPaneInfo &>(info));
        if (value > 0) {
            pivot.col += value;
        } else if (value < 0) {
            if (static_cast<size_t>(-value) <= pivot.col)
                pivot.col += value;
        }
        if (pivot.col >= info.max_size.width - info.min_size.width)
            pivot.col = info.max_size.width - info.min_size.width - 1;
    }

    void BeginScrollPane(State &state, Position &pivot, ScrollPaneInfo info) {
        const auto mouse_pos = GetMousePosition(state) - state.impl->get_selected().get_pos();
        if (internal::StaticBox(state.impl->get_selected().get_pos() + info.pos, info.min_size).contains(mouse_pos)) {
            scroll_horizontal(pivot, info, GetMouseScrollH(state));
            scroll_vertical(pivot, info, GetMouseScrollV(state));
        }

        const auto left_scroll_btn = Position{.col = 0, .row = info.min_size.height - 1} + info.pos;
        const auto right_scroll_btn = Position{.col = info.min_size.width - 2, .row = info.min_size.height - 1} + info.pos;
        const auto top_scroll_btn = Position{.col = info.min_size.width - 1, .row = 0} + info.pos;
        const auto bottom_scroll_btn = Position{.col = info.min_size.width - 1, .row = info.min_size.height - 2} + info.pos;

        if (mouse_pos == left_scroll_btn)
            scroll_horizontal(pivot, info, -(GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == right_scroll_btn)
            scroll_horizontal(pivot, info, (GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == top_scroll_btn)
            scroll_vertical(pivot, info, -(GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));
        if (mouse_pos == bottom_scroll_btn)
            scroll_vertical(pivot, info, (GetMouseClickCount(state, MouseButton::LEFT) + GetMouseClick2Count(state, MouseButton::LEFT)));

        state.impl->selected_stack.push(
                std::make_unique<internal::ViewBox>(info.show_scroll_bar, info.scroll_bar, info.pos, pivot, info.min_size, info.max_size)
        );
    }

    void EndPane(State &state) {
        internal::Box &box = state.impl->get_selected();
        if (auto vbox = dynamic_cast<internal::ViewBox *>(&box); vbox && vbox->show_scroll_bar()) {
            const auto &scroll = vbox->get_scroll_style();
            const auto max_size = vbox->get_max_size();
            const auto min_size = vbox->get_min_size();
            const auto pivot = vbox->get_pivot();

            // Vertical scroll
            if (min_size.height != max_size.height) {
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

                // const auto right_cell = Position{.col = min_size.width - 1, .row = min_size.height - 1} + pivot;
                // SetCell(state, ' ', right_cell, {.mode = STYLE_NO_BG | STYLE_NO_FG});
            }
            // Horizontal scroll
            if (min_size.width != max_size.width) {
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

    void DrawBorder(State &state, Border border) {
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

    void Text(State &state, TextInfo info) {
        if (internal::StaticBox(info.pos, Size{.width = info.text.size(), .height = 1})
                    .contains(GetMousePosition(state) - state.impl->get_selected().get_pos())) {
            if (IsMouseClicked(state, MouseButton::LEFT)) {
                if (info.on_click)
                    info.on_click(std::forward<TextInfo &>(info));
            } else if (IsMouseClicked(state, MouseButton::RIGHT)) {
                if (info.on_menu)
                    info.on_menu(std::forward<TextInfo &>(info));
            } else if (IsMouseDoubleClicked(state, MouseButton::LEFT)) {
                if (info.on_click2)
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
            if (IsMouseClicked(state, MouseButton::LEFT)) {
                if (info.on_click)
                    info.on_click(std::forward<TextBoxInfo &>(info));
            } else if (IsMouseClicked(state, MouseButton::RIGHT)) {
                if (info.on_menu)
                    info.on_menu(std::forward<TextBoxInfo &>(info));
            } else if (IsMouseDoubleClicked(state, MouseButton::LEFT)) {
                if (info.on_click2)
                    info.on_click2(std::forward<TextBoxInfo &>(info));
            } else if (info.on_hover)
                info.on_hover(std::forward<TextBoxInfo &>(info));
        }

        size_t i = 0;
        for (size_t row = 0; row < info.size.height; row++)
            for (size_t col = 0; col < info.size.width; col++) {
                if (i < info.text.size()) {
                    state.impl->set_cell(info.pos.col + col, info.pos.row + row, info.text[i], info.style);
                    i++;
                } else
                    state.impl->set_cell(info.pos.col + col, info.pos.row + row, ' ', info.style);
            }
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
            print("\x1b[0m");
        if (style.mode & STYLE_BOLD)
            print("\x1b[1m");
        if (style.mode & STYLE_UNDERLINE)
            print("\x1b[4m");
        if (style.mode & STYLE_INVERSE)
            print("\x1b[7m");

        if ((style.mode & STYLE_NO_FG) == 0)
            // Set foreground color
            print("\x1b[38;2;" + std::to_string(style.fg.r) + ";" + std::to_string(style.fg.g) + ";" + std::to_string(style.fg.b) + "m");

        if ((style.mode & STYLE_NO_BG) == 0)
            // Set background color
            print("\x1b[48;2;" + std::to_string(style.bg.r) + ";" + std::to_string(style.bg.g) + ";" + std::to_string(style.bg.b) + "m");
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
    static std::string get_last_error() {
        char err_msg_buf[4096];
        DWORD size = FormatMessage(
                FORMAT_MESSAGE_FROM_SYSTEM                  // Searches the system message table
                        | FORMAT_MESSAGE_IGNORE_INSERTS,    // Ignores insert sequences in the message definition.
                null,                                       // Handle to the module containing the message table
                GetLastError(),                             // Error code to format
                0,                                          // Default language
                err_msg_buf,                                // Output buffer for the formatted message
                4096,                                       // Minimum size of the output buffer
                null                                        // No arguments for insert sequences
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
        if (!WriteConsole(h_con, text.c_str(), text.size(), null, null))
            return std::format("error printing to console: {}", get_last_error());
        return true;
    }

    static DWORD old_in_mode = 0, old_out_mode = 0;
    static UINT old_console_cp = 0;

    Result init() {
        static const HANDLE h_conin = GetStdHandle(STD_INPUT_HANDLE);
        static const HANDLE h_conout = GetStdHandle(STD_OUTPUT_HANDLE);

        if (!GetConsoleMode(h_conin, &old_in_mode))
            return std::format("error getting console in mode: {}", get_last_error());
        if (!GetConsoleMode(h_conout, &old_out_mode))
            return std::format("error getting console out mode: {}", get_last_error());
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

        print("\x1b[?1049h");    // Enter alternate buffer
        print("\x1b[?25l");      // Hide console cursor
        clear();
        return true;
    }

    Result restore() {
        clear();
        print("\x1b[?25h");      // Show console cursor
        print("\x1b[?1049l");    // Exit alternate buffer

        if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), old_in_mode))
            return std::format("error setting console in mode: {}", get_last_error());
        if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), old_out_mode))
            return std::format("error setting console out mode: {}", get_last_error());
        if (!SetConsoleOutputCP(old_console_cp))
            return std::format("error setting console code page: {}", get_last_error());
        return true;
    }
}    // namespace nite::internal::console

#endif

#ifdef OS_WINDOWS
namespace nite
{
    static bool get_key_mod(WORD virtual_key_code, uint8_t &key_mod);
    static bool get_key_code(WORD virtual_key_code, char key_char, KeyCode &key_code);

    bool internal::PollRawEvent(Event &event) {
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
}    // namespace nite
#endif