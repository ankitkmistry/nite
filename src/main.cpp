// clang-format off

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
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

std::string quoted_str(std::string str) {
    std::stringstream ss;
    ss << std::quoted(str);
    str = ss.str();
    std::string new_str;
    for (size_t i = 0; i < str.size(); i++) {
        char c = str[i];
        if (std::isprint(c)) new_str += c;
        else new_str += std::format("\\x{:x}", c);
    }
    return new_str.substr(1, new_str.size() - 2);
}

std::string mod_str(uint8_t modifiers) {
    std::string result;
    if (modifiers & KEY_SHIFT) result += "SHIFT | ";
    if (modifiers & KEY_CTRL) result += "CTRL | ";
    if (modifiers & KEY_ALT) result += "ALT | ";
    if (modifiers & KEY_SUPER) result += "SUPER | ";
    if (modifiers & KEY_META) result += "META | ";
    if (!result.empty()) {
        result.pop_back();
        result.pop_back();
        result.pop_back();
        return result;
    } else return "NONE";
}

void hello_test(State &state) {
    static std::vector<std::string> lines;
    static std::string text;
    static Position scroll_pivot;
    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event,
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

void grid_test(State &state) {
    static int row_diff = 0;
    static int col_diff = 0;

    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event, 
            [&](const KeyEvent &ev) {
                if (ev.key_down)
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
            }
        );
    }

    BeginDrawing(state);
    BeginGridPane(state, {
        .pos = {},
        .size = GetBufferSize(state),
        .col_sizes = {(double) (50 - col_diff), (double) (50 + col_diff)},
        .row_sizes = {(double) (50 - row_diff), (double) (50 + row_diff)},
    });

    BeginGridCell(state, 0, 0); {
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
    } EndPane(state);

    BeginGridCell(state, 0, 1); {
        Text(state, {"Hello from 0, 1"});
        FillBackground(state, COLOR_RED);
    } EndPane(state);

    BeginGridCell(state, 1, 1); {
        Text(state, {"Hello from 1, 1"});
        FillBackground(state, COLOR_BLUE);
    } EndPane(state);

    BeginGridCell(state, 1, 0); {
        Text(state, {"Hello from 1, 0"});
        FillBackground(state, COLOR_GREEN);
    } EndPane(state);

    EndPane(state);
    EndDrawing(state);
}

void event_test(State &state) {
    static Position pivot;
    static std::vector<std::string> lines;

    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event,
            [&](const KeyEvent &ev) {
                if (ev.key_down) {
                    if (ev.key_code == KeyCode::K_C && ev.modifiers == 0)
                        lines.clear();
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
                }
                std::string msg;
                if (std::isprint(ev.key_char))
                    msg = std::format(
                        "KeyEvent -> key_down: {}, key_code: {}, key_char: {}, modifiers: {}", 
                        ev.key_down,
                        KeyCodeInfo::DebugString(ev.key_code), 
                        ev.key_char, 
                        mod_str(ev.modifiers)
                    );
                else
                    msg = std::format(
                    "KeyEvent -> key_down: {}, key_code: {}, modifiers: {}", 
                        ev.key_down, 
                        KeyCodeInfo::DebugString(ev.key_code), 
                        mod_str(ev.modifiers)
                    );
                lines.push_back(msg);
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
                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse click {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                    break;
                case MouseEventKind::DOUBLE_CLICK:
                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse click2 {}", ev.pos.col, ev.pos.row, btn_str(ev.button)));
                    break;
                case MouseEventKind::MOVED:
                    lines.push_back(std::format("MouseEvent ({}, {}) -> mouse moved", ev.pos.col, ev.pos.row));
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
            [&](const DebugEvent &ev) {
                lines.push_back(quoted_str(ev.text));
            }
        );
    }
    
    BeginDrawing(state);

    Text(state, {.text = "Hello World from Linux"});

    const auto size = GetBufferSize(state);
    
    BeginScrollPane(state, pivot, {
        .pos = {.col = 0,.row = 1},
        .min_size = {.width = size.width, .height = size.height - 1},
        .max_size = {.width = size.width * 2, .height = size.height * 2},
        .scroll_bar = SCROLL_LIGHT,
        .scroll_factor = 2,
    }); {
        size_t i = lines.size() < size.height - 1 ? 0 : lines.size() - (size.height - 1);
        for (size_t j = 0; i < lines.size(); i++, j++)
            Text(state, {
                .text = quoted_str(lines[i]),
                .pos = {.col = 0, .row = j},
            });
    } EndPane(state);

    EndDrawing(state);
}

// clang-format on

#include "stb_image.h"

class ImageView {
    std::vector<uint8_t> img;
    size_t width;
    size_t height;

  public:
    ImageView(int width, int height) : img(height * width, 0), width(width), height(height) {}

    ImageView(uint8_t *img, int width, int height) : img(img, img + height * width), width(width), height(height) {}

    uint8_t operator()(size_t x, size_t y) const {
        return img[y * width + x];
    }

    uint8_t &operator()(size_t x, size_t y) {
        return img[y * width + x];
    }

    size_t get_width() const {
        return width;
    }

    size_t get_height() const {
        return height;
    }
};

ImageView load_image(const char *filename) {
    int width, height, channels;
    unsigned char *img = stbi_load(filename, &width, &height, &channels, STBI_grey);
    if (img == NULL) {
        // std::cerr << "could not open file: " << filename << std::endl;
        std::exit(1);
    }
    // std::cout << "opened file: " << filename << std::endl;
    // std::cout << "width: " << width << std::endl;
    // std::cout << "height: " << height << std::endl;
    // std::cout << "channels: " << channels << std::endl;

    auto result = ImageView(img, width, height);
    stbi_image_free(img);
    return result;
}

std::array<ImageView, 3> load_image_rgb(const char *filename) {
    int width, height, channels;
    unsigned char *img = stbi_load(filename, &width, &height, &channels, STBI_rgb);
    if (img == NULL) {
        std::exit(1);
    }

    std::vector<unsigned char> red_data(width * height);
    for (size_t i = 0, j = 0; j < static_cast<size_t>(width * height); i += 3, j++)
        red_data[j] = img[i];

    std::vector<unsigned char> green_data(width * height);
    for (size_t i = 1, j = 0; j < static_cast<size_t>(width * height); i += 3, j++)
        green_data[j] = img[i];

    std::vector<unsigned char> blue_data(width * height);
    for (size_t i = 2, j = 0; j < static_cast<size_t>(width * height); i += 3, j++)
        blue_data[j] = img[i];

    std::array result{
            ImageView(red_data.data(), width, height),
            ImageView(green_data.data(), width, height),
            ImageView(blue_data.data(), width, height),
    };
    stbi_image_free(img);
    return result;
}

void write_pgm(const ImageView &image, const char *filename) {
    std::ofstream out(filename);
    out << std::format("P2\n{} {}\n255\n", image.get_width(), image.get_height());
    for (size_t y = 0; y < image.get_height(); y++) {
        for (size_t x = 0; x < image.get_width(); x++) {
            out << std::to_string(static_cast<int>(image(x, y))) << " ";
        }
        out << "\n";    // Add newline after each row for better readability
    }
}

ImageView down_scale(const ImageView &image, size_t x_scale, size_t y_scale) {
    const size_t width = image.get_width() / x_scale;
    const size_t height = image.get_height() / y_scale;

    ImageView result(width, height);

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t sum = 0;
            for (size_t dy = 0; dy < y_scale; dy++)
                for (size_t dx = 0; dx < x_scale; dx++)
                    sum += image(x * x_scale + dx, y * y_scale + dy);
            result(x, y) = sum / (x_scale * y_scale);
        }
    }

    return result;
}

int main1() {
    // by a factor of 25
    ImageView image = load_image("../res/musashi.jpg");
    write_pgm(image, "../res/musashi1.pgm");
    for (size_t i = 2; i <= 8; i++) {
        ImageView down_scaled = down_scale(image, i, i);
        write_pgm(down_scaled, std::format("../res/musashi{}.pgm", i).c_str());
    }
    return 0;
}

// clang-format off

void rgb_image_test(State &state) {
    static Position scroll_pivot;

    static auto array = load_image_rgb("../res/horn of salvation.jpg");
    // static auto array = load_image_rgb("../res/musashi.jpg");
    
    static auto image_r = down_scale(array[0], 6, 10);
    static auto image_g = down_scale(array[1], 6, 10);
    static auto image_b = down_scale(array[2], 6, 10);
    
    static const Size max_size {.width = image_r.get_width(), .height = image_r.get_height()};

    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event,
            [&](const KeyEvent &ev) {
                if (ev.key_down) {
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
                }
            }
        );
    }

    BeginDrawing(state);

    const auto size = GetBufferSize(state);

    BeginScrollPane(state, scroll_pivot, {
        .pos = {},
        .min_size = size,
        .max_size = max_size,
        .scroll_bar = SCROLL_LIGHT,
        .scroll_factor = 2,
        .show_hscroll_bar = true,
    }); {
        for (size_t y = 0; y < image_r.get_height(); y++)
            for (size_t x = 0; x < image_r.get_width(); x++)
                SetCell(state, ' ', {.x = x, .y = y}, {.bg = Color::from_rgb(image_r(x, y), image_g(x, y), image_b(x, y))});
    } EndPane(state);

    EndDrawing(state);
}

void image_test(State &state) {
    static Position scroll_pivot;
    static ImageView image = down_scale(load_image("../res/horn of salvation.jpg"), 6, 10);
    // static ImageView image = down_scale(load_image("../res/musashi.jpg"), 6, 10);

    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event,
            [&](const KeyEvent &ev) {
                if (ev.key_down) {
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
                }
            }
        );
    }

    BeginDrawing(state);

    const auto size = GetBufferSize(state);
    const Size max_size {.width = image.get_width(), .height = image.get_height()};

    BeginScrollPane(state, scroll_pivot, {
        .pos = {},
        .min_size = size,
        .max_size = max_size,
        .scroll_bar = SCROLL_LIGHT,
        .scroll_factor = 2,
        .show_hscroll_bar = true,
    }); {
        for (size_t y = 0; y < image.get_height(); y++)
            for (size_t x = 0; x < image.get_width(); x++)
                SetCell(state, ' ', {.x = x, .y = y}, {.bg = Color::from_rgb(image(x, y))});
    } EndPane(state);

    EndDrawing(state);
}

void align_test(State &state) {
    static size_t align = 0;
    static double value = 0;
    static std::vector<StyledChar> motion = [] () {
        std::vector<StyledChar> result(SLEEK_MOTION.begin(), SLEEK_MOTION.end());
        for (auto &it: result)
            it.style.bg = COLOR_LIME;
        return result;
    } ();

    Event event;
    while (PollEvent(state, event)) {
        HandleEvent(event, 
            [&](const KeyEvent &ev) {
                if (ev.key_down) {
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
                    if (ev.key_code == KeyCode::ENTER && ev.modifiers == 0)
                        align++;
                }
            }
        );
    }

    if (value > 1)
        value = 0;
    value += GetDeltaTime(state) / 2;

    BeginDrawing(state);
    const auto size = GetBufferSize(state);

    TextBox(state, {
        .text = "Hello, World\nThis is an example of multiline text\nThis is amazing\n\nPress Esc to quit, Enter to change alignment",
        .size = size,
        .align = static_cast<Align>(align % 9),
    });

    // DrawVDivider(state, size.width / 2);
    // DrawHDivider(state, size.height / 2);
    // SetCell(state, BORDER_SLEEK.center.value, {.col = size.width / 2, .row = size.height / 2});

    ProgressBar(state, {
        .value = value,
        .pos = {.col = 0, .row = size.height / 2},
        .length = size.width,
        .motion = motion,
        .style = {.bg = COLOR_LIME},
    });

    EndDrawing(state);
}

void input_test(State &state){
    static size_t align = 0;
    static TextInputState text_state;

    Event event;
    while (PollEvent(state, event)) {
        text_state.capture_event(event);

        HandleEvent(event, [&](const KeyEvent &ev) {
            if (ev.key_down && ev.modifiers == 0) {
                if (ev.key_code == KeyCode::F4 && ev.modifiers == 0)
                    CloseWindow(state);
                if (ev.key_code == KeyCode::F2 && ev.modifiers == 0)
                    align++;
                if (ev.key_code == KeyCode::F3 && ev.modifiers == 0)
                    text_state.insert_char('\x1d'); // See the magic
            }
        });
    }

    BeginDrawing(state);

    TextInput(state, text_state, {
        .pos = {.col = 0, .row = 0},
        .size = GetPaneSize(state),
        .align = static_cast<Align>(align % 9),
    });

    EndDrawing(state);
}

// clang-format on

int main() {
    auto &state = GetState();
    Initialize(state);

    while (!ShouldWindowClose(state)) {
        rgb_image_test(state);
    }

    Cleanup();
    return 0;
}

// clang-format off

int main2() {
    auto &state = GetState();
    if (const auto result = Initialize(state); !result) {
        std::cerr << result.what() << std::endl;
        return 1;
    }

    bool header = false;
    bool border = false;
    bool color = false;

    while (!ShouldWindowClose(state)) {
        Event event;
        while (PollEvent(state, event)) {
            HandleEvent(
                event,
                [&](const KeyEvent &ev) {
                    if (ev.key_down) {
                        if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                            CloseWindow(state);
                        if (ev.key_code == KeyCode::K_H && ev.modifiers == 0)
                            header = !header;
                        if (ev.key_code == KeyCode::K_B && ev.modifiers == 0)
                            border = !border;
                        if (ev.key_code == KeyCode::K_C && ev.modifiers == 0)
                            color = !color;
                    }
                }
            );
        }

        BeginDrawing(state);
        SimpleTable(state, {
            .data = {
                "Name",	        "Telephone",	"Email",	            "Office",
                "Dr. Sally",	"555-1234",	    "sally@calpoly.edu",	"12-34",
                "Dr. Steve",	"555-5678",	    "steve@calpoly.edu",	"56-78",
                "Dr. Kathy",	"555-9012",	    "kathy@calpoly.edu",	"90-123",
            },
            .include_header_row = header,
            .num_cols = 4,
            .num_rows = 4,
            .pos = {},
            .header_style = color ? Style{.bg = COLOR_TEAL, .fg = COLOR_WHITE, .mode = STYLE_BOLD} : Style{.mode = STYLE_BOLD},
            .table_style = color ? Style{.bg = COLOR_NAVY, .fg = COLOR_SILVER} : Style{},
            .show_border = border,
            .border = TABLE_BORDER_LIGHT,
        });
        EndDrawing(state);
    }

    if (const auto result = Cleanup(); !result) {
        std::cerr << result.what() << std::endl;
        return 1;
    }
    return 0;
}