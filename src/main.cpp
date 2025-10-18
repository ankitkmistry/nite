#include <cmath>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>
#include <format>

#include "nite.hpp"

using namespace nite;

template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
// Some compilers might require this explicit deduction guide
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

std::string btn_str(MouseButton btn) {
    switch (btn) {
    case MouseButton::NONE:
        return "NONE";
    case MouseButton::LEFT:
        return "LEFT";
    case MouseButton::RIGHT:
        return "RIGHT";
    }
}

void hello_test(State &state, std::vector<std::string> &lines) {
    auto size = GetBufferSize(state);
    Text(state, {
                        .text = "Hello, World",
                        .pos = {.col = 0, .row = 0},
    });
    Text(state, {
                        .text = std::format("Width: {}", size.width),
                        .pos = {.col = 0, .row = 1},
    });
    Text(state, {
                        .text = std::format("Height: {}", size.height),
                        .pos = {.col = 0, .row = 2},
    });

    // DrawLine(state, {.col = 0, .row = size.height - 1}, {.col = size.width, .row = 0}, ' ', {.bg = COLOR_FUCHSIA});
    // DrawLine(state, {.col = 0, .row = 0}, {.col = size.width, .row = size.height}, ' ', {.bg = COLOR_FUCHSIA});

    DrawLine(state, {.col = 0, .row = 3}, {.col = size.width, .row = 3}, '-', {.fg = COLOR_RED});

    const size_t height = size.height - 4;
    const auto start = lines.size() < height ? 0 : lines.size() - height;
    for (size_t i = start, j = 0; i < lines.size(); i++, j++)
        Text(state, {
                            .text = lines[i], .pos = {.col = 0, .row = 4 + j}
        });
}

void color_test(State &state) {
    auto size = GetBufferSize(state);

    for (size_t row = 0; row < size.height; row++)
        for (size_t col = 0; col < size.width; col++) {
            uint8_t x = std::lerp(0, 255, 1.0 * col / (size.width - 1));
            uint8_t y = std::lerp(0, 255, 1.0 * row / (size.height - 1));

            auto t = time(0) * GetDeltaTime(state);
            auto p = sin(t) * sin(t);

            SetCell(state, ' ', {.col = col, .row = row}, {.bg = Color::from_rgb(255 * p, x, y)});
        }
}

int main() {
    auto &state = GetState();
    Initialize(state);

    std::vector<std::string> lines;
    Position mouse_pos;

    while (!ShouldWindowClose(state)) {
        Event event;
        while (PollEvent(event)) {
            std::visit(
                    overloaded{
                            [&](const KeyEvent &ev) {
                                std::string msg;
                                if (std::isprint(ev.key_char))
                                    msg = std::format(
                                            "KeyEvent -> key_down: {}, key_code: {}, key_char: {}", ev.key_down,
                                            KeyCodeInfo::DebugString(ev.key_code), ev.key_char
                                    );
                                else
                                    msg = std::format("KeyEvent -> key_down: {}, key_code: {}", ev.key_down, KeyCodeInfo::DebugString(ev.key_code));

                                lines.push_back(msg);
                                if (ev.key_code == KeyCode::K_Q)
                                    CloseWindow(state);
                            },
                            [&](const FocusEvent &ev) {
                                auto msg = std::format("FocusEvent -> focus {}", (ev.focus_gained ? "gained" : "lost"));
                                lines.push_back(msg);
                            },
                            [&](const ResizeEvent &ev) {
                                auto msg = std::format("ResizeEvent -> window resized {}x{}", ev.size.width, ev.size.height);
                                lines.push_back(msg);
                            },
                            [&](const MouseEvent &ev) {
                                switch (ev.kind) {
                                case MouseEventKind::DOWN:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse down {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                    break;
                                case MouseEventKind::UP:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse up {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                    break;
                                case MouseEventKind::MOVED:
                                    mouse_pos = ev.pos;
                                    // lines.push_back(std::format("MouseEvent ({}, {}) -> mouse moved", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_DOWN:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled down", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_UP:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled up", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_LEFT:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled left", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_RIGHT:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled right", ev.pos.col, ev.pos.row));
                                    break;
                                }
                            },
                            [&](const auto &) { lines.push_back("event caught"); },
                    },
                    event
            );
        }

        BeginDrawing(state);

        hello_test(state, lines);

        auto size = GetBufferSize(state);

        FillBackground(state, Color::from_hex(0x0950df));

        BeginPane(state, {.col = size.width / 2, .row = 0}, {.width = size.width / 2, .height = 3});
        {
            DrawBorder(state, BORDER_DEFAULT);
            FillBackground(state, Color::from_hex(0x165d2a));
            // Text(state, {
            //                     .text = std::format("delta time: {:.3f} secs", GetDeltaTime(state)),
            //                     .pos = {.x = 0,            .y = 0             },
            //                     .style{.fg = COLOR_WHITE, .mode = STYLE_NO_BG}
            // });
            Text(state, {
                                .text = std::format("FPS: {:.2f}", 1 / GetDeltaTime(state)),
                                .pos = {.x = 1,            .y = 1             },
                                .style{.fg = COLOR_WHITE, .mode = STYLE_NO_BG}
            });
        }
        EndPane(state);

        SetCell(state, ' ', mouse_pos, {.bg = COLOR_SILVER});

        EndDrawing(state);
    }

    Cleanup();
    return 0;
}