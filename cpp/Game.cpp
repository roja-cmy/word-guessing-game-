/* ============================================================================
   cpp/Game.cpp — moved
   ----------------------------------------------------------------------------
   The engine implementation moved to game.cpp in the project root
   (word database, no-repeat selection, guesses, lives, score, streak, hints,
   win/lose and the wordhunt_command() WebAssembly export).

   This translation unit is intentionally empty so that the old build command

        g++ -std=c++11 game.cpp cpp/Game.cpp -o wordhunt

   still compiles without duplicate symbols. Prefer the shorter command:

        g++ -std=c++11 game.cpp -o wordhunt

   You may delete the cpp/ folder completely.
   ========================================================================== */
