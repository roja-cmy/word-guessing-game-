wasm/  —  the compiled C++ engine for the browser
================================================

This folder is where the WebAssembly build of the C++ game engine is placed.
The browser (index.html) automatically looks for `wasm/game.js` on every load
and, when it is found, calls the exported C++ function `wordhunt_command()`
directly — so the real C++ engine runs inside the page.

Build it once with the Emscripten SDK:

    emcc -std=c++11 game.cpp -O2 \
         -s MODULARIZE=1 \
         -s EXPORTED_FUNCTIONS=_wordhunt_command \
         -o wasm/game.js

This produces:

    wasm/game.js     the JavaScript glue (loaded by index.html)
    wasm/game.wasm   the compiled C++ engine  (created next to game.js)

Installing the Emscripten SDK (one time only):

    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh        # Windows: emsdk_env.bat

Then serve the project over http:// and open it:

    python3 -m http.server 8000
    → http://localhost:8000

The badge on the home screen will read "C++ ENGINE · WEBASSEMBLY".

If wasm/game.js is missing (static hosting, file://, GitHub Pages), the page
falls back to the clearly labelled "C++ ENGINE · OFFLINE PREVIEW" mode, which
reads the word database straight out of game.cpp and mirrors the rules of the
C++ class so the project still works as a demo. Compile the engine to replace
it with the real thing.

Terminal version (no browser, no Emscripten):

    g++ -std=c++11 game.cpp -o wordhunt
    ./wordhunt
