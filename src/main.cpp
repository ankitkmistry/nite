#include <cctype>
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

void hello_test(State &state) {
    static std::vector<std::string> lines;
    static std::string text;
    static Position scroll_pivot;
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
                                lines.push_back(std::format("MouseEvent ({}, {}) -> double click {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                                break;
                            default:
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
    DrawLine(state, {.col = 0, .row = 3}, {.col = size.width, .row = 3}, '-', {.fg = COLOR_RED, .mode = STYLE_RESET | STYLE_BOLD});


    BeginScrollPane(
            state, scroll_pivot,
            {
                    .pos = {.col = 0,                .row = 4                 },
                    .min_size = {.width = size.width,     .height = size.height - 4},
                    .max_size = {.width = size.width * 2, .height = size.height * 2},
                    .scroll_factor = 2,
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

int main() {
    auto &state = GetState();
    Initialize(state);

    int row_diff = 0;
    int col_diff = 0;

    while (!ShouldWindowClose(state)) {
        Event event;
        while (PollEvent(state, event)) {
            std::visit(
                    overloaded{
                            [&](const KeyEvent &ev) {
                                if (ev.key_down)
                                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                                        CloseWindow(state);
                            },
                            [&](const auto &) {},
                    },
                    event
            );
        }

        BeginDrawing(state);
        BeginGridPane(
                state, {
                               .pos = {},
                               .size = GetBufferSize(state),
                               .col_sizes = {(double) (50 - col_diff), (double) (50 + col_diff)},
                               .row_sizes = {(double) (50 - row_diff), (double) (50 + row_diff)},
        }
        );

        BeginGridCell(state, 0, 0);
        {
            Text(state, {"Hello from 0, 0"});
            Text(state, {
                                .text = "+ Col",
                                .pos = {.col = 0, .row = 1},
                                .on_click =
                                        [&](const nite::TextInfo &) {
                                            if (col_diff < 50)
                                                col_diff++;
                                        },
                                .on_click2 =
                                        [&](const nite::TextInfo &) {
                                            if (col_diff < 50)
                                                col_diff++;
                                        },
            });
            Text(state, {
                                .text = "- Col",
                                .pos = {.col = 6, .row = 1},
                                .on_click =
                                        [&](const nite::TextInfo &) {
                                            if (col_diff > -50)
                                                col_diff--;
                                        },
                                .on_click2 =
                                        [&](const nite::TextInfo &) {
                                            if (col_diff > -50)
                                                col_diff--;
                                        },
            });
            Text(state, {
                                .text = "+ Row",
                                .pos = {.col = 0, .row = 2},
                                .on_click =
                                        [&](const nite::TextInfo &) {
                                            if (row_diff < 50)
                                                row_diff++;
                                        },
                                .on_click2 =
                                        [&](const nite::TextInfo &) {
                                            if (row_diff < 50)
                                                row_diff++;
                                        },
            });
            Text(state, {
                                .text = "- Row",
                                .pos = {.col = 6, .row = 2},
                                .on_click =
                                        [&](const nite::TextInfo &) {
                                            if (row_diff > -50)
                                                row_diff--;
                                        },
                                .on_click2 =
                                        [&](const nite::TextInfo &) {
                                            if (row_diff > -50)
                                                row_diff--;
                                        },
            });
            FillBackground(state, COLOR_WHITE);
            FillForeground(state, COLOR_BLACK);
        }
        EndPane(state);

        BeginGridCell(state, 0, 1);
        {
            Text(state, {"Hello from 0, 1"});
            FillBackground(state, COLOR_RED);
        }
        EndPane(state);

        BeginGridCell(state, 1, 1);
        {
            Text(state, {"Hello from 1, 1"});
            FillBackground(state, COLOR_BLUE);
        }
        EndPane(state);

        BeginGridCell(state, 1, 0);
        {
            Text(state, {"Hello from 1, 0"});
            FillBackground(state, COLOR_GREEN);
        }
        EndPane(state);

        EndPane(state);
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