#include <cctype>
#include <cmath>
#include <cstddef>
#include <iostream>
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

void hello_test(State &state) {
    auto size = GetBufferSize(state);
    Text(state, {
                        .text = "Hello, World (Control+q to quit)",
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

    DrawLine(state, {.col = 0, .row = 3}, {.col = size.width, .row = 3}, '-', {.fg = COLOR_RED, .mode = STYLE_RESET | STYLE_BOLD});
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
    std::string text;
    Position scroll_pivot;

    while (!ShouldWindowClose(state)) {
        Event event;
        while (PollEvent(state, event)) {
            std::visit(
                    overloaded{
                            [&](const KeyEvent &ev) {
                                if (ev.key_down) {
                                    if (std::isprint(ev.key_char))
                                        text += ev.key_char;
                                    if (ev.key_code == KeyCode::K_C && ev.modifiers == 0)
                                        lines.clear();
                                    if (ev.key_code == KeyCode::K_Q && ev.modifiers & KEY_CTRL)
                                        CloseWindow(state);
                                }
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
                                case MouseEventKind::CLICK:
                                    lines.push_back(std::format("MouseEvent ({}, {}) -> click {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                    break;
                                case MouseEventKind::DOUBLE_CLICK:
                                    lines.push_back(
                                            std::format("MouseEvent ({}, {}) -> double click {}", ev.pos.col, ev.pos.row, btn_str(ev.button))
                                    );
                                    break;
                                case MouseEventKind::MOVED:
                                    break;
                                case MouseEventKind::SCROLL_DOWN:
                                    // lines.push_back(std::format("MouseEvent ({}, {}) -> scrolled down", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_UP:
                                    // lines.push_back(std::format("MouseEvent ({}, {}) -> scrolled up", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_LEFT:
                                    // lines.push_back(std::format("MouseEvent ({}, {}) -> scrolled left", ev.pos.col, ev.pos.row));
                                    break;
                                case MouseEventKind::SCROLL_RIGHT:
                                    // lines.push_back(std::format("MouseEvent ({}, {}) -> scrolled right", ev.pos.col, ev.pos.row));
                                    break;
                                }
                            },
                            [&](const auto &) {},
                    },
                    event
            );
        }

        BeginDrawing(state);
        const Size size = GetBufferSize(state);

        hello_test(state);

        BeginScrollPane(
                state, scroll_pivot,
                {
                        .pos = {.col = 0,                .row = 4                 },
                        .min_size = {.width = size.width,     .height = size.height - 4},
                        .max_size = {.width = size.width * 2, .height = size.height * 2},
                        .scroll_factor = 1.5,
                        .show_scroll_bar = true,
        }
        );
        {
            for (size_t i = 0; i < lines.size(); i++) {
                Text(state, {
                                    .text = lines[i], .pos = {.col = 0, .row = i}
                });
            }
        }
        EndPane(state);

        FillBackground(state, Color::from_hex(0x0950df));

        BeginPane(state, {.col = size.width / 2, .row = 0}, {.width = size.width / 2, .height = 3});
        {
            // DrawBorder(state, BORDER_DEFAULT);
            FillBackground(state, Color::from_hex(0x165d2a));
            Text(state, {
                                .text = std::format("FPS: {:.2f}", 1 / GetDeltaTime(state)),
                                .pos = {.x = 0,            .y = 0             },
                                .style{.fg = COLOR_WHITE, .mode = STYLE_NO_BG},
            });
            TextBox(state, {
                                   .text = std::format("{}", text),
                                   .pos = {.x = 0, .y = 1},
                                   .size = {GetPaneSize(state).width, 2},
                                   .style{.bg = Color::from_hex(0x165d2a), .fg = COLOR_WHITE},
                                   .on_hover = [](TextBoxInfo &info) { info.style.bg = Color::from_hex(0x067bd8); },
                                   .on_click = [&](TextBoxInfo &) { text = "clicked"; },
            });
        }
        EndPane(state);

        SetCell(state, ' ', GetMousePosition(state), {.bg = COLOR_SILVER});

        EndDrawing(state);
    }

    Cleanup();
    return 0;
}

// std::visit(
//      overloaded{
//              [&](const KeyEvent &ev) {
//                  std::string msg;
//                  if (std::isprint(ev.key_char))
//                      msg = std::format(
//                              "KeyEvent -> key_down: {}, key_code: {}, key_char: {}", ev.key_down,
//                              KeyCodeInfo::DebugString(ev.key_code), ev.key_char
//                      );
//                  else
//                      msg = std::format("KeyEvent -> key_down: {}, key_code: {}", ev.key_down, KeyCodeInfo::DebugString(ev.key_code));
//                  lines.push_back(msg);
//                  if (ev.key_code == KeyCode::K_Q)
//                      CloseWindow(state);
//              },
//              [&](const FocusEvent &ev) {
//                  auto msg = std::format("FocusEvent -> focus {}", (ev.focus_gained ? "gained" : "lost"));
//                  lines.push_back(msg);
//              },
//              [&](const ResizeEvent &ev) {
//                  auto msg = std::format("ResizeEvent -> window resized {}x{}", ev.size.width, ev.size.height);
//                  lines.push_back(msg);
//              },
//              [&](const MouseEvent &ev) {
//                  switch (ev.kind) {
//                  case MouseEventKind::DOWN:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse down {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
//                      break;
//                  case MouseEventKind::UP:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse up {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
//                      break;
//                  case MouseEventKind::MOVED:
//                      mouse_pos = ev.pos;
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse moved", ev.pos.col, ev.pos.row));
//                      break;
//                  case MouseEventKind::SCROLL_DOWN:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled down", ev.pos.col, ev.pos.row));
//                      break;
//                  case MouseEventKind::SCROLL_UP:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled up", ev.pos.col, ev.pos.row));
//                      break;
//                  case MouseEventKind::SCROLL_LEFT:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled left", ev.pos.col, ev.pos.row));
//                      break;
//                  case MouseEventKind::SCROLL_RIGHT:
//                      lines.push_back(std::format("MouseEvent ({}, {}) -> mouse scrolled right", ev.pos.col, ev.pos.row));
//                      break;
//                  }
//              },
//              [&](const auto &) { lines.push_back("event caught"); },
//      },
//      event
// );