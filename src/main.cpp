#include <cstddef>
#include <string>
#include <variant>
#include <vector>
#include <format>

#include "nite.hpp"

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// Some compilers might require this explicit deduction guide
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string btn_str(nite::MouseButton btn) {
    switch (btn) {
    case nite::MouseButton::NONE:
        return "NONE";
    case nite::MouseButton::LEFT:
        return "LEFT";
    case nite::MouseButton::RIGHT:
        return "RIGHT";
    }
}

int main() {

    /*
    nite::begin_drawing()

    * Rect
    * Text


    nite::end_drawing()
    */

    auto &state = nite::GetState();

    nite::Initialize(state);

    std::vector<std::string> lines;

    while (!nite::ShouldWindowClose(state)) {
        nite::Event event;
        while (nite::PollEvent(event)) {
            std::visit(
                    overloaded{
                            [&](const nite::KeyEvent &ev) {
                                auto msg = std::format(
                                        "KeyEvent -> key_down: {}, key_code: {} key_char: {}", ev.key_down,
                                        nite::KeyCodeInfo::DebugString(ev.key_code), ev.key_char
                                );
                                lines.push_back(msg);
                                if (ev.key_code == nite::KeyCode::K_Q)
                                    nite::CloseWindow(state);
                            },
                            [&](const nite::FocusEvent &ev) {
                                auto msg = std::format("FocusEvent -> focus {}", (ev.focus_gained ? "gained" : "lost"));
                                lines.push_back(msg);
                            },
                            [&](const nite::ResizeEvent &ev) {
                                auto msg = std::format("ResizeEvent -> window resized {}x{}", ev.size.width, ev.size.height);
                                lines.push_back(msg);
                            },
                            [&](const nite::MouseEvent &ev) {
                                switch (ev.kind) {
                                case nite::MouseEventKind::DOWN:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse down {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                    break;
                                case nite::MouseEventKind::UP:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse up {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                    break;
                                case nite::MouseEventKind::MOVED:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse moved", ev.pos.col, ev.pos.row));
                                    break;
                                case nite::MouseEventKind::SCROLL_DOWN:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled down", ev.pos.col, ev.pos.row));
                                    break;
                                case nite::MouseEventKind::SCROLL_UP:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled up", ev.pos.col, ev.pos.row));
                                    break;
                                case nite::MouseEventKind::SCROLL_LEFT:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled left", ev.pos.col, ev.pos.row));
                                    break;
                                case nite::MouseEventKind::SCROLL_RIGHT:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled right", ev.pos.col, ev.pos.row));
                                    break;
                                }
                            },
                            [&](const auto &) { lines.push_back("event caught"); },
                    },
                    event
            );
        }

        nite::BeginDrawing(state);

        auto size = nite::GetBufferSize(state);
        nite::Text(
                state, {
                               .text = "Hello, World",
                               .pos = {.col = 0, .row = 0},
        }
        );
        nite::Text(
                state, {
                               .text = std::format("Width: {}", size.width),
                               .pos = {.col = 0, .row = 1},
        }
        );
        nite::Text(
                state, {
                               .text = std::format("Height: {}", size.height),
                               .pos = {.col = 0, .row = 2},
        }
        );
        nite::Text(
                state, {
                               .text = std::string(size.width, '-'),
                               .pos = {.col = 0, .row = 3},
        }
        );

        const size_t height = size.height - 4;
        const auto start = lines.size() < height ? 0 : lines.size() - height;
        for (size_t i = start, j = 0; i < lines.size(); i++, j++)
            nite::Text(
                    state, {
                                   .text = lines[i], .pos = {.col = 0, .row = 4 + j}
            }
            );

        nite::EndDrawing(state);
    }

    nite::Cleanup();
    return 0;
}