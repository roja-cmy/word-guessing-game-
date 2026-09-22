/* ============================================================================
   game.h — WORD HUNT : A Word Guessing Adventure  (C++ game engine)
   ----------------------------------------------------------------------------
   This file is the interface of the game engine. EVERY game rule lives here:
   the word database with clues/hints/facts, the per-category no-repeat system,
   guesses, lives, score, streak, hints, win/lose and the category-completed
   state.

   The HTML/CSS frontend never contains a rule. It sends one text command to
   the engine and paints the KEY|VALUE state that comes back.

   COMMANDS (browser -> C++)
     SETUSED|FRUITS:MANGO,APPLE;SPORTS:CRICKET   restore used-word history
     RESUME|<score>|<streak>|<wordsCompleted>    restore a player's progress
     START|<categoryIndex>|<difficulty>|<seed>   pick the next UNUSED word
     GUESS|<letter>
     HINT                                       1st = extra clue, 2nd = letter
     GIVEUP
     RESETCAT|<categoryIndex>                    free all words of one category
     RESET                                       new game (score 0, streak 0)
     STATUS

   STATE BLOCK (C++ -> browser)
     CAT|SPORTS
     CLUE|A world-famous bat-and-ball game ...
     HINTTEXT|It is especially popular in India ...   (after hint 1)
     LEN|7
     MASK|C _ _ _ _ _ _
     GUESSED|C,M
     WRONG|P,R
     LIVES|6
     MAX|6
     SCORE|850
     ROUND|150
     STREAK|4
     HINTS|2
     WORDS|12
     REMAINING|3          unused words left in this category
     STATUS|PLAYING       NOT_STARTED | PLAYING | WON | LOST | COMPLETED
     EVENT|NEW            NEW|CORRECT|WRONG|DUPLICATE|INVALID|HINT|WIN|LOSE|ERROR
     MESSAGE|Great! C appears 1 time(s). +10 points.
     DIFF|2
     WORD|CRICKET         only sent when the round is over
     USED|FRUITS:MANGO,APPLE;SPORTS:CRICKET
     DONECATS|0,4         categories the player has fully completed
     FACT|Cricket is ...  only sent when the round is won

   BUILD
     terminal : g++ -std=c++11 game.cpp -o wordhunt
     browser  : emcc -std=c++11 game.cpp -O2 -s MODULARIZE=1 \
                   -s EXPORTED_FUNCTIONS=_wordhunt_command -o wasm/game.js
   ========================================================================== */

#ifndef WORD_HUNT_GAME_H
#define WORD_HUNT_GAME_H

#include <string>
#include <vector>

/* number of categories in the word database (ANIMALS ... GENERAL) */
const int NUM_CATEGORIES = 7;

/* --------------------------------------------------------------------------
   One entry of the word database. Every word carries its own teaching
   material: a clue that helps the player think, two hints (one helpful,
   one vaguer) and a "did you know" fact shown on the win screen.
   -------------------------------------------------------------------------- */
struct WordData {
    std::string word;        /* the answer, always A-Z, no spaces         */
    std::string category;    /* ANIMALS, FRUITS, ...                      */
    std::string clue;        /* main description shown during the round   */
    std::string hint1;       /* helpful extra clue (hint button, 1st use) */
    std::string hint2;       /* vaguer clue used on HARD difficulty       */
    std::string fact;        /* educational fact shown after a win        */
    std::string difficulty;  /* EASY | MEDIUM | HARD                      */
};

/* result of one guess */
enum GuessResult {
    GUESS_ERROR = 0,   /* no game running / round finished */
    GUESS_INVALID,     /* not a letter A..Z                */
    GUESS_DUPLICATE,   /* letter already tried             */
    GUESS_CORRECT,     /* letter found in the word         */
    GUESS_WRONG,       /* letter not in the word           */
    GUESS_WIN,         /* letter found and word complete   */
    GUESS_LOST         /* letter wrong and no lives left   */
};

/* --------------------------------------------------------------------------
   THE GAME ENGINE
   -------------------------------------------------------------------------- */
class WordHuntGame {
public:
    WordHuntGame();

    /* ------------------------- setup ------------------------- */
    bool selectCategory(int index);          /* false = invalid index  */
    bool selectDifficulty(int level);        /* false = invalid level  */
    bool getNextWord(unsigned int seed = 0); /* false = category done  */
    void resetGame();                        /* score + streak to zero */
    void resetCategory(int index);           /* free one category      */

    /* ------------------------ gameplay ----------------------- */
    GuessResult checkGuess(char letter);
    void        revealLetter(char letter);   /* used by the hint system */
    bool        useHint();                   /* false = no hint left    */
    int         calculateScore(int base);    /* applies the multiplier  */
    bool        checkWin()  const;
    bool        checkGameOver() const;

    /* ------------- used-word tracking (no repeats) ------------ */
    void        setUsedWords(const std::string& csv);      /* restore   */
    std::string getUsedWords() const;                      /* persist   */
    bool        categoryCompleted(int index) const;
    int         wordsRemaining(int index) const;
    std::string completedCategories() const;   /* "0,4"                 */

    /* ------------------- state and queries ------------------- */
    std::string getMaskedWord() const;
    std::string getGameStatus() const { return gameStatus; }
    std::string getSecretWord()  const { return current.word; }
    int         getScore()       const { return score; }
    int         getStreak()      const { return streak; }

    /* -------------------- bridge to the UI ------------------- */
    std::string handleCommand(const std::string& command);

    /* -------------------- word bank helpers ------------------ */
    static int         categoryCount();
    static std::string categoryName(int index);
    static int         wordsInCategory(int index);
    static int         livesForDifficulty(int level);   /* 8 / 6 / 4  */
    static double      multiplierFor(int level);        /* 1 / 1.5 / 2 */
    static std::string difficultyName(int level);

private:
    bool            isLetterInWord(char letter) const;     /* letter search */
    const WordData* selectRandomWord(unsigned int seed);   /* no-repeat */
    std::string     buildState(const std::string& event);
    std::string     eventName(GuessResult result) const;
    std::string     finish(bool won);
    void            addPoints(int points);
    void            losePoints(int points);
    std::vector<const WordData*> availableWords(int category) const;

    /* ------------------ state of one game ------------------- */
    WordData             current;                 /* the word in play      */
    int                  categoryIndex;           /* 0 .. NUM_CATEGORIES-1 */
    int                  difficultyLevel;         /* 1 easy .. 3 hard      */
    std::vector<char>    guessedLetters;
    std::vector<char>    wrongLetters;
    std::vector<std::string> usedWords[NUM_CATEGORIES];  /* per category! */
    int                  remainingLives;
    int                  maxLives;
    int                  score;
    int                  roundScore;
    int                  streak;
    int                  hintsRemaining;
    int                  wordsCompleted;
    std::string          gameStatus;              /* NOT_STARTED/.../COMPLETED */
    std::string          lastMessage;
    std::string          revealedHint;            /* clue from hint 1 */
};

#endif /* WORD_HUNT_GAME_H */
