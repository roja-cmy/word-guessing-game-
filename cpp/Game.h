/* ============================================================================
   cpp/Game.h — moved
   ----------------------------------------------------------------------------
   The whole C++ engine now lives in the project root as a single, clearly
   separated pair of files, exactly as the project structure requires:

        game.h    -> the WordData struct + the WordHuntGame class interface
        game.cpp  -> the word database, all rules and the WebAssembly export

   This folder is kept only so that older build commands keep working:

        g++ -std=c++11 game.cpp cpp/Game.cpp -o wordhunt     (old, still fine)
        g++ -std=c++11 game.cpp -o wordhunt                  (new, preferred)

   Nothing in this file is required by the game; you may delete the cpp/
   folder without breaking anything.
   ========================================================================== */

#include "../game.h"
