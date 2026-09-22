# 🔍 WORD HUNT — *Guess. Learn. Compete.*

> A polished, production-ready word guessing game: user accounts, shared online scoreboard,
> clues, hints, facts, streaks and a strict no-repeat word system — with the **complete game
> engine written in C++**.

---

## 1. Project Overview

WORD HUNT is a modern educational word game. A player registers, chooses a category and a
difficulty, reads a **clue** and guesses the hidden word letter by letter. Every one of the 224
words carries a clue, a hint and an educational fact. Points, lives, streaks and the no-repeat
word selection are all decided by the C++ engine; the HTML/CSS layer only renders what the engine
returns.

The application is organised as a real product: landing page → login/register → dashboard →
category → difficulty → game → result → scoreboard/profile, with a persistent session so a player
is never asked to log in twice.

---

## 2. Problem Statement

> *Build a browser-based educational word game that looks and behaves like a professional product
> — with user accounts, a shared leaderboard and a large word database — while keeping every game
> rule inside a C++ engine, and make it deployable on a public URL that anyone can open.*

Browsers cannot execute C++ natively, so the engine is compiled once to **WebAssembly**
(`wasm/game.js` + `wasm/game.wasm`) and the page calls the exported C++ function
`wordhunt_command()` for every action.

---

## 3. Objective

* Keep the existing C++ engine intact (`WordData` struct, `WordHuntGame` class, no-repeat
  selection, scoring, hints, win/lose) and build the product features around it.
* 224 words (32 per category), each with a clue, a hint and a fact.
* Accounts, sessions, dashboard, profile and a **shared** scoreboard.
* Difficulty must genuinely change **which words** are offered, not only the number of lives.
* Deployable on a public URL with accounts and scores that survive a restart.

---

## 4. Features

| Area | Feature |
|---|---|
| 🏠 Landing page | Logo, tagline “Guess. Learn. Compete.”, description, **PLAY NOW**, **SCOREBOARD**, LOGIN / REGISTER, HOW TO PLAY, WHY PLAY, engine badge, and a small logged-in user status |
| 🔐 Authentication | Login **and** Register tabs, show/hide password, **Remember me**, full validation with clear error messages, duplicate username/email prevention; anonymous play is not allowed |
| 🧍 Session | Logged in until logout (localStorage *or* sessionStorage depending on “Remember me”), survives reloads; logout asks for confirmation and keeps all data |
| 📊 Dashboard | Welcome, total score, best score, games played, correct letters, wins/losses, storage mode, per-category progress cards, PLAY GAME / SCOREBOARD / PROFILE / LOGOUT |
| 🗂 Categories | 7 cards (icon, name, description, **words available**) with live progress; completed categories are marked |
| 🎚 Difficulty | EASY 8 lives · 1x, MEDIUM 6 lives · 1.5x, HARD 4 lives · 2x — every word is tagged EASY/MEDIUM/HARD and the engine filters the pool by the chosen level |
| 📚 Database | 224 words × 7 categories, each with `word, category, clue, hint1, hint2, fact, difficulty` |
| 🚫 No repeats | Per **user** and per **category** used-word lists live inside the C++ engine; restored on login (`SETUSED`) and persisted with the profile |
| 🏁 Completion | “Congratulations! You completed all words in this category.” → Another category / **Reset category** / Home |
| 🎮 Game screen | Chips for user, score, streak, category, difficulty, lives; clue box; word blanks; A–Z keyboard with correct/wrong/used states; used-letter pills; two hints; give up |
| 💡 Hints | Hint 1 = an additional clue (a vaguer one on HARD), Hint 2 = reveals one hidden letter; −15 points each |
| 🏆 Scoring | +10 per letter, +50 word bonus, −5 wrong, −15 hint, ×1 / ×1.5 / ×2 multiplier, never below zero — all in C++ |
| 🔥 Streak | +1 per word solved, reset on a loss, shown on home, dashboard, game, win, scoreboard and profile |
| 🎉 Win screen | WORD COMPLETE! · word · points earned · total score · streak · **Did you know?** fact · PLAY AGAIN / NEXT WORD / HOME / BACK / CHANGE CATEGORY / SCOREBOARD |
| 💀 Lose screen | GAME OVER · the word · score · lives used · TRY AGAIN / NEXT WORD / HOME / BACK / CHANGE CATEGORY / SCOREBOARD |
| ↩ Real BACK | Every Back button returns to the actual previous screen (WIN→GAME, DIFFICULTY→CATEGORY, CATEGORY→LOGIN, Scoreboard→previous) |
| 🏆 Scoreboard | Current score, best score, **games played**, **correct letters**, words completed, current & best streak, a podium for the top 3 and a ranked table (Rank · Player · Score · Best · Games · Correct · Words · Best streak) with the current player highlighted — built from **real stored scores** and **shared between all visitors of the public link** |
| 👤 Profile | Username, email, member since, score, best, games, correct letters, wins/losses, words, best streak, per-category completion, **Shared online scoreboard** connection panel, LOGOUT, BACK TO DASHBOARD |
| ❓ How to play | Dedicated screen with the 10 steps (also linked from the top navigation) |
| ⭐ Why play | Dedicated screen: memory, vocabulary, spelling, quick thinking, general knowledge, concentration |
| 🔊 Sound | click / correct / wrong / hint / win / lose — file slots in `assets/sounds/` with a synthesised fallback |
| ♿ Accessibility | Full keyboard play, visible focus rings, `aria-live` messages, `aria-label`s, reduced-motion support |
| 📱 Responsive | Single-column mobile layout, 7-column keyboard, no horizontal scrolling, 320 px → 4 K |

---

## 5. Technologies Used

| Layer | Technology |
|---|---|
| Game engine | **C++11** — `WordData` struct, `WordHuntGame` class, `std::vector`, `std::string`, `rand()/srand()` |
| Browser build | **Emscripten / WebAssembly** → `wasm/game.js` + `wasm/game.wasm`, export `wordhunt_command()` |
| Frontend | HTML5 (sections, forms, SVG, ARIA) + CSS3 (variables, grid/flex, `clamp()`, keyframes) |
| Bridge | A small JavaScript layer that sends text commands and paints the returned state — it contains **no game rule** |
| Data | `localStorage` (default) **or** Supabase PostgreSQL (connected at runtime from the Profile page) |
| Crypto | Web Crypto **SHA-256** with a per-user random salt; passwords are never stored in plain text |
| Fonts | Google Fonts — *Plus Jakarta Sans* |

---

## 6. Why C++ Is Used

1. The brief requires the game logic in C++ — and it is respected literally: no rule is duplicated.
2. One engine, two front-ends: the same `game.cpp` runs in the browser (WebAssembly) and in the
   terminal (`./wordhunt`).
3. C++ is a natural fit for an engine: typed state, fast, and it forces a clean split between
   *rules* and *presentation*.
4. Educational value: classes, encapsulation, STL containers, enums, random numbers, string
   handling, validation and an export layer in a real product.

---

## 7. HTML / CSS Role

**HTML5** — structure only: the 13 screens (landing, auth, dashboard, category, difficulty, game,
win, game over, category completed, scoreboard, profile, how to play, why play), forms, the
explorer SVG, word blanks, the keyboard, the leaderboard table and all ARIA attributes.

**CSS3** — everything visual: the adventure world (sky, sun, drifting clouds, hills, swaying
trees) shared by every screen, the 3D stacked-shadow logo, the orange PLAY button with its shine
sweep, glass cards and translucent buttons, rounded corners, hover lift, press-down feedback,
disabled states, keyframe animations (screen transitions, letter pop, shake, explorer bob,
confetti), a responsive grid that stacks on mobile, and `prefers-reduced-motion` support.

**JavaScript (bridge only)** — build a text command → call the C++ engine → paint the returned
state, plus accounts, navigation history, the shared-database connection and sounds.

---

## 8. System Architecture

```text
        ┌───────────────────────────────────────────────────────────────┐
        │                           BROWSER                            │
        │  index.html (structure)  +  style.css (design)                │
        │  bridge: command → engine → paint state                       │
        │  accounts / scoreboard / used words → localStorage or cloud   │
        └───────────────┬───────────────────────────────────────────────┘
                        │ text commands   START|3|2|981273
                        │                 GUESS|C   HINT   GIVEUP
                        ▼                 RESETCAT|1   SETUSED|…   RESUME|…
        ┌───────────────────────────────────────────────────────────────┐
        │  wasm/game.js + wasm/game.wasm   (compiled from game.cpp)     │
        │  extern "C" const char* wordhunt_command(const char*)         │
        └───────────────┬───────────────────────────────────────────────┘
                        ▼
        ┌───────────────────────────────────────────────────────────────┐
        │  game.cpp — class WordHuntGame  (ALL THE RULES)               │
        │  WORD_DATABASE[224] · usedWords[7] (no-repeat)                │
        │  selectRandomWord() · checkGuess() · useHint()                │
        │  calculateScore() · checkWin() · checkGameOver()              │
        └───────────────┬───────────────────────────────────────────────┘
                        ▼  state block
        CAT|SPORTS  CLUE|A world-famous bat-and-ball game …
        LEN|7  MASK|C _ _ _ _ _ _  GUESSED|C,M  WRONG|P,R
        LIVES|6  MAX|6  SCORE|850  ROUND|150  STREAK|4  HINTS|2
        WORDS|12  REMAINING|3  COUNTS|32,32,32,32,32,32,32
        STATUS|PLAYING  EVENT|CORRECT  MESSAGE|Correct! C appears 1 time(s).
        DIFF|2  DONECATS|0,4  USED|FRUITS:MANGO,APPLE;SPORTS:CRICKET
```

The engine is **stateful**: the secret word, guessed letters, lives, score, streak, hints and the
used-word lists all live inside the C++ object. The browser only receives the mask and the public
counters — the answer (`WORD|…`) is released only when the round is finished.

---

## 9. Game Flow

```
LANDING ──PLAY NOW──► AUTH (only for guests) ──► CATEGORY ──► DIFFICULTY ──► GAME
   │                                              ▲                            │
   │                     already logged in ────────┘                            │
   │                                                                           ▼
   │                                        WORD COMPLETE  ◄── last letter ────┤
   │                                              │                    all lives lost
   │                       PLAY AGAIN / NEXT WORD / HOME / BACK            GAME OVER
   └── HOME (still logged in) ◄────────────────────┘        TRY AGAIN / NEXT WORD / …
```

1. **Landing** → PLAY NOW (guests go to login/register, logged-in players go straight to the categories).
2. **Auth** → login or register → “Welcome, \<name\>!” → category selection.
3. **Dashboard** → statistics, per-category progress and quick actions.
4. **Category** → 7 cards with the number of available words.
5. **Difficulty** → lives, word level and score multiplier.
6. **Game** → clue, blanks, hearts, keyboard, hints, used letters.
7. **Result** → win or lose screen, then continue.

---

## 10. C++ Concepts Used

| Concept | Where |
|---|---|
| `struct` | `WordData { word, category, clue, hint1, hint2, fact, difficulty }` |
| Class + encapsulation | `class WordHuntGame` — private state, public methods |
| `enum` | `GuessResult { GUESS_ERROR, GUESS_INVALID, GUESS_DUPLICATE, GUESS_CORRECT, GUESS_WRONG, GUESS_WIN, GUESS_LOST }` |
| Array of structs | `WordData WORD_DATABASE[224]` |
| `std::string` | secret word, masked word, clues, messages |
| `std::vector<char>` | `guessedLetters`, `wrongLetters` |
| `std::vector<std::string>` | **`usedWords[NUM_CATEGORIES]`** — the no-repeat system |
| Static members | `categoryName()`, `categoryCount()`, `wordsInCategory()`, `livesForDifficulty()`, `multiplierFor()` |
| Random numbers | `srand(seed)` + `rand() % pool.size()` over the *unused* pool only |
| Loops & conditionals | masking, letter searching, win/lose checks, validation |
| `<sstream>` | splitting the protocol string and the used-word CSV |
| `<cctype>` | `toupper()`, A–Z validation |
| `extern "C"` | `wordhunt_command()` exported to WebAssembly |

Main methods: `selectCategory()`, `selectDifficulty()`, `selectRandomWord()`, `getNextWord()`,
`checkGuess()`, `isLetterInWord()`, `revealLetter()`, `useHint()`, `calculateScore()`,
`checkWin()`, `checkGameOver()`, `resetGame()`, `resetCategory()`, `setUsedWords()`,
`getUsedWords()`, `categoryCompleted()`, `wordsRemaining()`, `getMaskedWord()`,
`getGameStatus()`, `handleCommand()`.

---

## 11. The No-Repeat Algorithm (in C++)

```cpp
const WordData* WordHuntGame::selectRandomWord(unsigned int seed) {
    /* 1. every word of the category … 2. minus the used ones */
    vector<const WordData*> available = availableWords(categoryIndex);
    /* 3. nothing left? -> the category is completed */
    if (available.empty()) return NULL;

    /* 4. difficulty filters the pool (never blocks the game) */
    if (difficultyLevel == 1)      /* EASY   -> only EASY words   */
    else if (difficultyLevel == 2) /* MEDIUM -> EASY + MEDIUM     */
    else                           /* HARD   -> MEDIUM + HARD     */
    if (pool.empty()) pool = available;

    /* 5. random pick inside the reduced pool */
    const WordData* chosen = pool[rand() % pool.size()];

    /* 6. marked as used IMMEDIATELY */
    usedWords[categoryIndex].push_back(chosen->word);
    return chosen;
}
```

* Tracking is **per category** (`usedWords[0..6]`), so switching categories never frees words:
  ANIMALS → ELEPHANT, SPORTS → CRICKET, ANIMALS → TIGER … ELEPHANT stays unavailable.
* Tracking is **per user**: on login the browser sends `SETUSED|ANIMALS:ELEPHANT;…` so the
  exclusion list is restored inside the engine, and the engine returns the updated list after
  every round so it can be persisted. Reloading the page never brings a solved word back.
* “Reset word progress” and “Reset category” call `RESETCAT|index` — the only way to free words,
  and both ask for confirmation first.

---

## 12. Project Folder Structure

```
word-hunt/
│
├── index.html        → all 13 screens + the bridge (self-contained: embeds style.css)
├── style.css         → the complete stylesheet (external version of the same rules)
├── game.h            → WordData struct + WordHuntGame class (engine interface)
├── game.cpp          → ★ THE ENGINE: 224-word database, no-repeat, all rules, WASM export
├── README.md         → this documentation
│
├── wasm/
│   ├── README.txt    → build instructions
│   ├── game.js       → generated by Emscripten
│   └── game.wasm     → generated by Emscripten
│
├── assets/
│   ├── images/       → screenshots (the app itself needs no image files)
│   └── sounds/       → click, correct, wrong, hint, win, lose (optional .mp3)
│
└── cpp/              → legacy folder (engine moved to the root); safe to delete
```

---

## 13. How to Install

```bash
git clone <your-repo> word-hunt && cd word-hunt

# A) terminal version — any C++11 compiler
g++ -std=c++11 game.cpp -o wordhunt

# B) browser version — Emscripten SDK (installed once)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk && ./emsdk install latest && ./emsdk activate latest && source ./emsdk_env.sh
cd ..

emcc -std=c++11 game.cpp -O2 -s MODULARIZE=1 \
     -s EXPORTED_FUNCTIONS=_wordhunt_command -o wasm/game.js
```

No npm packages, no build pipeline, no local server required to run it.

---

## 14. How to Run

```bash
# browser — any static server
python3 -m http.server 8000      # → http://localhost:8000

# terminal version
./wordhunt
```

> **Offline preview:** if `wasm/game.js` is missing, the page automatically falls back to the
> clearly labelled *“C++ engine (offline preview)”* mode, which reads the word database straight
> out of `game.cpp` at runtime (so the 224 words are **not** copied into JavaScript) and mirrors
> the rules of the `WordHuntGame` class. Compile the engine to run the real C++ in the page.

---

## 15. How C++ Communicates with the Frontend

**1. The page builds a command**

```js
await Engine.run('SETUSED|ANIMALS:ELEPHANT');      // restore used words
await Engine.run('RESUME|850|4|12');               // restore score / streak / words
await Engine.run('START|' + category + '|' + difficulty + '|' + seed);
await Engine.run('GUESS|C');
await Engine.run('HINT');
```

**2. It reaches the C++ engine through WebAssembly**

```cpp
extern "C" const char* wordhunt_command(const char* input) {
    engineReply = engine.handleCommand(std::string(input));   // ← WordHuntGame
    return engineReply.c_str();
}
```

**3. The page paints the state** — `CAT`, `CLUE`, `HINTTEXT`, `LEN`, `MASK`, `GUESSED`, `WRONG`,
`LIVES`, `MAX`, `SCORE`, `ROUND`, `STREAK`, `HINTS`, `WORDS`, `REMAINING`, `COUNTS`, `STATUS`,
`EVENT`, `MESSAGE`, `DIFF`, `DONECATS`, `USED`, and at the end of a round `WORD` + `FACT`.

Because the protocol is plain text, the same engine can be driven by a C++ HTTP server, a
WebSocket, a named pipe or a test harness without changing a line of `game.cpp`.

---

## 16. Accounts, Sessions and the Shared Scoreboard

Two interchangeable storage modes, chosen at runtime:

| Mode | Where data lives | When to use |
|---|---|---|
| **Local (default)** | `localStorage` in the player's browser | zero setup, works on any static host |
| **Cloud** | Supabase PostgreSQL through its REST API | a shared public deployment — every visitor sees the same accounts and leaderboard |

Both store exactly the same record per player:

```
id · username · email · salt · hash (SHA-256) · score · best · wins · losses
words · games · correct · streak · bestStreak · difficulty · used (per-category words) · joined
```

Passwords are **never stored in plain text**: each user gets a random salt and the stored value is
`SHA-256(salt + password)` computed with the Web Crypto API. Each account is an independent
record, so one player's score can never overwrite another's.

### Turning on the shared scoreboard (2 minutes, no code edit)

1. Create a free project on [supabase.com](https://supabase.com) and open **SQL Editor**.
2. Run:

```sql
create table wh_users (
  id         text primary key,
  username   text not null unique,
  email      text not null unique,
  salt       text not null,
  hash       text not null,
  score      int  default 0,
  best       int  default 0,
  wins       int  default 0,
  losses     int  default 0,
  words      int  default 0,
  games      int  default 0,
  correct    int  default 0,
  streak     int  default 0,
  beststreak int  default 0,
  difficulty int  default 2,
  used       text default '{}',
  joined     text
);
alter table wh_users enable row level security;
create policy "public read"   on wh_users for select using (true);
create policy "public write"  on wh_users for insert with check (true);
create policy "public update" on wh_users for update using (true);
```

3. Open your deployed app → **🏆 SCOREBOARD ▸ 🌐 Share this scoreboard online** → paste the
   **Project URL** and the **public anon key** → **CONNECT DATABASE**. The setting is saved in the
   browser and applied instantly; you can also pre-fill it for everybody by setting `CLOUD` at the
   top of the script in `index.html`.

The dashboard then shows **“Storage: ☁ Database”**: accounts, scores, games played, correct
letters, streaks, best scores and per-player used-word lists all live in PostgreSQL, so nothing is
lost when the server restarts and the leaderboard is shared by everyone who opens the public link.

> The `anon` key is a *public* key designed for the browser — never put a service-role key or a
> database password in frontend code. For a commercial product, move the write operations behind
> a server (see *Future enhancements*).

---

## 17. Deployment — getting a public URL you can share

The project is **100 % static**: no server-side runtime, no localhost dependency, relative paths
only. Any static host works.

### Option A — Netlify Drop (fastest, ~2 minutes)

1. Zip the project folder (`index.html`, `style.css`, `game.h`, `game.cpp`, `wasm/`, `assets/`).
2. Open <https://app.netlify.com/drop> and drag the zip (or the folder) onto the page.
3. Netlify returns a public URL such as `https://word-hunt.netlify.app` — send that link to your
   professor. Nothing else to configure.

### Option B — Vercel

```bash
npm i -g vercel
vercel          # answer the prompts, accept the defaults
vercel --prod   # prints the production URL
```

### Option C — GitHub Pages

```bash
git init && git add . && git commit -m "WORD HUNT"
git branch -M main
git remote add origin https://github.com/<you>/word-hunt.git
git push -u origin main
# Repository → Settings → Pages → Source: main / root
```

### Before you share the link, check

* ✅ `wasm/game.js` and `wasm/game.wasm` are uploaded (then the badge reads
  *“C++ engine running (WebAssembly)”*). Without them the app still works in offline preview
  mode, but the compiled engine is the real thing.
* ✅ `game.cpp` is uploaded — the offline preview reads the word database from it.
* ✅ `assets/sounds/*.mp3` are optional; missing files fall back to synthesised sounds.
* ✅ Connect the shared database (section 16) **before** sending the link if you want one
  leaderboard for everybody.
* ✅ Open the URL in a private window and run through the testing checklist below.

> Recommended production architecture (all free tiers):
> **Frontend → Netlify / Vercel · Database → Supabase (PostgreSQL) · Engine → WebAssembly**.

---

## 18. Testing Checklist (verified flow)

```
HOME → PLAY NOW → REGISTER → LOGIN → DASHBOARD → CATEGORY → DIFFICULTY → GAME
→ correct guess → wrong guess → HINT 1 (extra clue) → HINT 2 (revealed letter)
→ WORD COMPLETE → NEXT WORD (new unused word) → HOME (still logged in, score kept)
→ SCOREBOARD (games, correct letters, words, best streak updated)
→ FRUITS → MANGO → WIN → NEXT WORD → MANGO never appears again
→ BACK returns to the previous screen → LOGOUT → LOGIN with another account
→ the two accounts keep separate scores and separate used-word lists
```

Also verified: difficulty really changes the word pool · a completed category shows the completion
screen instead of repeating a word · repeated letters are refused with “You already guessed E!” ·
empty / multi-letter / invalid input give friendly messages · score never goes below zero ·
streak resets only on a loss · the leaderboard uses real stored scores · no horizontal scrolling
from 320 px to 4 K · no console errors on a normal play-through.

---

## 19. How to Explain This Project in the Viva

**What is the project?**
“WORD HUNT is an educational word guessing game. A player registers, picks a category and a
difficulty, reads a clue and guesses the word letter by letter. Wrong guesses cost a life and draw
a cartoon explorer; finishing a word gives points, a streak and an educational fact.”

**Why did we choose Hangman?**
“It is small enough to finish properly but touches every fundamental — structs, strings, vectors,
searching, counters, random numbers, validation and game state — and it needs a real interface,
which let us practise separating logic from presentation.”

**Why C++?**
“The brief required the game logic in C++, and it is the right tool for an engine: one class holds
all the state and all the rules. The same C++ file runs in the browser through WebAssembly and in
the terminal, so nothing is written twice.”

**What data structures are used?**
“A `struct WordData` for each of the 224 words — word, category, clue, two hints, a fact and a
difficulty tag — kept in an array of structs. `std::vector<char>` for the guessed and wrong
letters, `std::vector<std::string>` for the used words of each category, and `std::string` for the
word, the mask and the messages.”

**How is random word selection performed?**
“First the engine builds the list of *unused* words of that category, then it keeps only the words
whose difficulty tag matches the chosen level, then it calls `rand()` on that reduced pool and
immediately pushes the chosen word into `usedWords[category]`. So a word can never be selected
twice, and if the pool is empty the engine reports that the category is completed.”

**How are guesses stored?**
“In the `guessedLetters` vector. Before scoring, the engine checks whether the letter is already
there; if it is, the guess is refused with `GUESS_DUPLICATE`, so the same letter can never be
counted twice. Wrong letters are copied into `wrongLetters`.”

**How is the win condition checked?**
“`checkWin()` walks through every character of the secret word and looks for it in the guessed
vector. If any character is missing, the word is not complete. When all characters are found the
status becomes `WON`, a 50-point bonus times the multiplier is added, the streak increases, and
the word plus its fact are released to the interface. `checkGameOver()` compares the remaining
lives with zero.”

**How is the score calculated?**
“`calculateScore()` multiplies the base points by the difficulty multiplier — 1× easy, 1.5×
medium, 2× hard — and rounds. A correct letter gives 10 points per occurrence, finishing the word
gives 50, a wrong guess costs 5 and a hint costs 15. `losePoints()` clamps the total at zero.”

**How do multiple users work?**
“Every account is an independent record — username, email, salted password hash, score, best,
games played, correct letters, wins, losses, words solved, streak, best streak and its own
per-category used-word list. On login the browser sends that list to the engine with `SETUSED`, so
the no-repeat rule is per player. In local mode the records live in the browser; when a shared
Supabase database is connected from the Profile page, the same records live in PostgreSQL and the
leaderboard is shared by everybody who opens the public link.”

**How does HTML/CSS interact with C++?**
“Through a small text protocol. The page sends a line such as `GUESS|C`; the exported C++
function `wordhunt_command()` passes it to `WordHuntGame::handleCommand()`, which runs the rule
and returns a block of KEY|VALUE lines — mask, clue, lives, score, streak, status and a friendly
message. The JavaScript bridge only converts that block into DOM updates; it never decides a
rule.”

**What are the future enhancements?**
“A server-side backend with proper bcrypt password hashing and JWT sessions, a global
cross-device leaderboard with seasons, multiplayer duels, timed challenges, more languages and
word packs, and a unit-test suite that drives `handleCommand()` for every rule.”

---

## 20. Future Enhancements

* Server-side authentication (bcrypt + JWT) and a shared PostgreSQL leaderboard with seasons
* Multiplayer duels and friend challenges
* Timed rounds, daily challenge, achievements and XP levels
* More categories, more languages, larger word packs
* Hints that describe the word instead of revealing a letter
* Sound packs, background music
* Unit tests (`tests/test_game.cpp`) calling `handleCommand()` for every rule
* PWA install with full offline support

---

## 21. Responsibility Split

| Layer | Owns |
|---|---|
| **C++ (`game.h`, `game.cpp`)** | words · categories · clues · hints · facts · difficulty filtering · random selection · no-repeat tracking · guesses · lives · score · streak · win/lose · category completed · the whole protocol |
| **HTML5** | screens · forms · buttons · SVG explorer · keyboard · navigation · ARIA |
| **CSS3** | theme · cards · typography · buttons · animations · responsive layout |
| **JavaScript (bridge only)** | send command → receive state → update DOM · accounts/session storage · navigation history · shared-database connection · sounds |

---

Made with ❤️ as a college mini-project — **a C++ engine in a professional web app.**
