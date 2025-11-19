// clang-format off
#include "nite.hpp"

using namespace nite;

int main() {
    auto &state = GetState();
    Initialize(state);
    
    int row_diff = 0;
    int col_diff = 0;

    while (!ShouldWindowClose(state)) {
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

    Cleanup();
    return 0;
}