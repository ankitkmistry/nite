// clang-format off
#include <format>

#include "nite.hpp"

using namespace nite;

std::string btn_str(MouseButton btn) {
    switch (btn) {
    case MouseButton::NONE:
        return "NONE";
    case MouseButton::LEFT:
        return "LEFT";
    case MouseButton::MIDDLE:
        return "MIDDLE";
    case MouseButton::RIGHT:
        return "RIGHT";
    }
    return "";
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
            HandleEvent(event,
                [&](const KeyEvent &ev) {
                    if (ev.key_down) {
                        if (std::isprint(ev.key_char))
                            text += ev.key_char;
                        if (ev.key_code == KeyCode::K_C && ev.modifiers == 0)
                            lines.clear();
                        if (ev.key_code == KeyCode::F4 && ev.modifiers == 0)
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
                }
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

        BeginScrollPane(state, scroll_pivot, {
            .pos = {.col = 0, .row = 4},
            .min_size = {.width = size.width,     .height = size.height - 4},
            .max_size = {.width = size.width * 2, .height = size.height * 2},
            .scroll_factor = 2,
        }); {
            for (size_t i = 0; i < lines.size(); i++) 
                Text(state, {.text = lines[i], .pos = {.col = 0, .row = i}});
        } EndPane(state);

        FillBackground(state, Color::from_hex(0x0950df));

        BeginPane(state, {.col = size.width / 2, .row = 0}, {.width = size.width / 2, .height = 3}); {
            // DrawBorder(state, BORDER_DEFAULT);
            FillBackground(state, Color::from_hex(0x165d2a));
            Text(state, {
                .text = std::format("FPS: {:.2f}", 1 / GetDeltaTime(state)),
                .pos = {.x = 0,            .y = 0             },
                .style = {.fg = COLOR_WHITE, .mode = STYLE_NO_BG},
            });
            TextBox(state, {
                .text = std::format("{}", text),
                .pos = {.x = 0, .y = 1},
                .size = {GetPaneSize(state).width, 2},
                .style = {.bg = Color::from_hex(0x165d2a), .fg = COLOR_WHITE},
                .on_hover = [](TextBoxInfo &info) { info.style.bg = Color::from_hex(0x067bd8); },
                .on_click = [&](TextBoxInfo &) { text = "clicked"; },
            });
        } EndPane(state);

        SetCell(state, ' ', GetMousePosition(state), {.bg = COLOR_SILVER});

        EndDrawing(state);
    }

    Cleanup();
    return 0;
}