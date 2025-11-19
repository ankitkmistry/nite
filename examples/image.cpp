// clang-format off
#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <format>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "nite.hpp"

using namespace nite;

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