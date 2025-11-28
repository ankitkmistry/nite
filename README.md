# nite: An Immediate Mode Terminal Renderer

> [!WARNING]
> This project is under development. USE WITH CARE.
> I am not responsible for breaking changes in your code.
> 
> *I develop whenever I feel like ;)*

## Description
`nite` is a lightweight and efficient immediate mode terminal renderer designed for modern C++ applications. It provides a simple API for rendering text-based user interfaces in terminal environments, making it ideal for tools, games, and utilities that require dynamic terminal output.

## Features
- **Immediate Mode Rendering**: Simplifies the process of creating terminal-based UIs.
- **Cross-Platform Support**: Works on Linux, macOS, and Windows.
- **Customizable**: Easily extendable to suit your specific needs.
- **Unicode Support**: Handles wide characters and Unicode text rendering (uses UTF-8 by default).

## Requirements
- **C++20**: The project uses modern C++ features and requires a compiler that supports C++20.
- **CMake 3.15+**: For building the project.
- **Optional**: `ncurses` library for extended terminal capabilities (enable with `NITE_USE_NCURSES`).

## Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/ankitkmistry/nite.git
   cd nite
   ```

2. Create a build directory and configure the project:
   ```bash
   cmake -B build -S .
   ```

3. Build the project:
   ```bash
   cmake --build build
   ```

4. Run the executable:
   ```bash
   ./build/nite
   ```

## Example

A minimal example using a TextBox with align is as follows:

```cpp
#include <nite.hpp>

using namespace nite;

int main() {
    // Get the state to work on
    State &state = GetState();
    // Initialize the terminal
    Initialize(state);

    while (!ShouldWindowClose(state)) {
        // Look for events
        Event event;
        while (PollEvent(state, event)) {
            HandleEvent(event, 
                [&](const KeyEvent &ev) {
                    if (!ev.key_down)
                        return;
                    if (ev.key_code == KeyCode::ESCAPE && ev.modifiers == 0)
                        CloseWindow(state);
                }
            );
        }

        BeginDrawing(state);
        TextBox(state, {
            .text = "Hello, World",
            .pos = {.col = 0, .row = 0},
            .size = GetPaneSize(state),
            .style = {.bg = COLOR_GREEN, .fg = COLOR_SILVER},
            .align = Align::CENTER,
        });
        EndDrawing(state);
    }

    // Restore the terminal
    Cleanup();
    return 0;
}
```

## Documentation
For now there is no official documentation, you can refer to the source code.

## About the kind of C++ I am using
The design goals of this project should be:
- Should be fast (even on the sluggy windows console host)
- Should use less memory
- Should be minimal (Inspired from Raylib)
- Should follow the immediate mode philosophy (Inspired from Dear ImGui, *although some hacks may be possible*)
- The API should be straightforward and simple
- The API should allow the user to customise as they want

Currently, I intend to use only a small subset of C++ features:
- I use little to no template programming as it makes the compilation slower, though concepts are used
- I use `std::format` wherever I can but also keep in mind of other functions such as `std::from_chars` and `std::to_string`
- I forbid the use of exceptions as they make error handling slow and messy. I use the `nite::Result` type instead
- User API is simple. It makes use of C-style structural and procedural programming, though there is a little use of `dynamic_cast` (*I had to use it, there was no other way!!*)

## Why is this written in C++?
This library is written in C++ for the following reasons:
- Using C++20 is cool!
- I need namespaces
- I need STL
- I need RAII and memory management
- I need C++ concepts
- I need lambda expressions

## License
This project is licensed under the MIT License. See the `LICENSE.txt` file for details.

## Contributing
Contributions are welcome! Feel free to open issues or submit pull requests to improve the project.
