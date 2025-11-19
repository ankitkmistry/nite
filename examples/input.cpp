// clang-format off
#include "nite.hpp"

using namespace nite;

int main() {
    auto &state = GetState();
    Initialize(state);
    
    size_t align = 0;
    TextInputState text_state;

    while (!ShouldWindowClose(state)) {
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

    Cleanup();
    return 0;
}