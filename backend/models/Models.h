/* ============================================================================
   backend/models/Models.h — SMARTSEAT data models
   ----------------------------------------------------------------------------
   Consolidates the model layer in one header:
        models/Student.h      -> struct Student
        models/Seat.h         -> struct Seat
        models/Classroom.h    -> struct Classroom  (2D seat grid)
        models/Allocation.h   -> AllocationRecord, Conflict, Move, QualityFactor,
                                 Action (undo/redo), UploadReport
   Only the allowed syllabus concepts are used: strings, arrays, 2D arrays and
   C++ containers (std::vector / std::map).
   ========================================================================== */
#ifndef SMARTSEAT_MODELS_H
#define SMARTSEAT_MODELS_H

#include <string>
#include <vector>
#include <map>

/* --------------------------------------------------------------------------
   STUDENT  (strings: name, roll number, section, class, subject, exam)
   -------------------------------------------------------------------------- */
struct Student {
    std::string roll;
    std::string name;
    std::string department;
    std::string className;
    std::string section;
    std::string subject;
    std::string exam;

    Student() {}
    Student(const std::string& r, const std::string& n, const std::string& d,
            const std::string& c, const std::string& s, const std::string& sub,
            const std::string& e)
        : roll(r), name(n), department(d), className(c), section(s), subject(sub), exam(e) {}

    bool isValid() const {
        return roll != "" && name != "" && section != "" && subject != "";
    }
    std::string missing() const {                 /* used by the upload validator */
        std::string m = "";
        if (roll == "")     m += "roll number, ";
        if (name == "")     m += "name, ";
        if (section == "")  m += "section, ";
        if (subject == "")  m += "subject, ";
        if (m.size() > 2) m = m.substr(0, m.size() - 2);
        return m;
    }
};

/* --------------------------------------------------------------------------
   SEAT  — one desk inside a classroom grid
   -------------------------------------------------------------------------- */
struct Seat {
    int row = 0;                 /* 1-based, shown to students   */
    int col = 0;                 /* 1-based, shown to students   */
    std::string roll = "";       /* occupant roll number         */
    bool blocked = false;
    bool reserved = false;

    std::string code(const std::string& room) const {
        return room + "-R" + std::to_string(row) + "-C" + std::to_string(col);
    }
    std::string status() const {
        if (blocked)             return "BLOCKED";
        if (roll != "")          return "OCCUPIED";
        if (reserved)            return "RESERVED";
        return "AVAILABLE";
    }
};

/* --------------------------------------------------------------------------
   CLASSROOM — a 2D ARRAY of seats (the core syllabus structure)
   -------------------------------------------------------------------------- */
struct Classroom {
    std::string name;
    int rows = 0;
    int cols = 0;
    std::vector<std::vector<Seat> > grid;      /* grid[row-1][col-1] */
    int blockedCount = 0;

    Classroom() {}
    Classroom(const std::string& n, int r, int c) : name(n), rows(r), cols(c) {
        build();
    }
    void build() {
        grid.assign(rows, std::vector<Seat>(cols));
        blockedCount = 0;
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++) {
                grid[i][j].row = i + 1;
                grid[i][j].col = j + 1;
                grid[i][j].roll = "";
                grid[i][j].blocked = false;
                grid[i][j].reserved = false;
            }
    }
    bool inside(int r, int c) const { return r >= 1 && r <= rows && c >= 1 && c <= cols; }
    bool free(int r, int c) const {
        if (!inside(r, c)) return false;
        const Seat& s = grid[r - 1][c - 1];
        return !s.blocked && s.roll == "" && !s.reserved;
    }
    int capacity() const { return rows * cols; }
    int occupied() const {
        int n = 0;
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                if (grid[i][j].roll != "") n++;
        return n;
    }
    int blockedSeats() const {
        int n = 0;
        for (int i = 0; i < rows; i++)
            for (int j = 0; j < cols; j++)
                if (grid[i][j].blocked) n++;
        return n;
    }
    int vacant() const { return capacity() - occupied() - blockedSeats(); }
    double utilization() const {
        if (capacity() == 0) return 0;
        return 100.0 * (occupied() + blockedSeats()) / capacity();
    }
};

/* --------------------------------------------------------------------------
   ALLOCATION RECORDS
   -------------------------------------------------------------------------- */
struct AllocationRecord {                 /* one student -> one seat */
    std::string roll;
    std::string room;
    int row = 0;
    int col = 0;
    std::string seat() const { return room + "-R" + std::to_string(row) + "-C" + std::to_string(col); }
};

struct Conflict {                         /* same-paper / same-section proximity */
    int id = 0;
    std::string room, seat, roll, name, subject;
    std::string neighbourSeat, neighbourRoll, neighbourName, neighbourSubject;
    std::string type;                     /* SAME_PAPER / SAME_SECTION / SAME_CLASS */
    std::string severity;                 /* HIGH / MEDIUM / LOW                    */
    std::string status;                   /* OPEN / RESOLVED                        */
};

struct Move {                             /* one entry of the movement history */
    std::string roll;
    std::string from;
    std::string to;
    std::string reason;
    std::string time;
};

struct QualityFactor {                    /* why the quality score is what it is */
    std::string label;
    int points = 0;                       /* negative = penalty                   */
    std::string detail;
};

/* one reversible action for the UNDO / REDO stack */
struct Action {
    std::string type;                     /* MOVE / BLOCK / UNBLOCK / REALLOCATE  */
    std::string roll;
    std::string fromRoom; int fromRow = 0; int fromCol = 0;
    std::string toRoom;   int toRow = 0;   int toCol = 0;
    std::string note;
};

/* report returned by the Excel/CSV upload validator */
struct UploadReport {
    int total = 0, accepted = 0, duplicates = 0, missing = 0, invalid = 0;
    std::vector<std::string> messages;
};

/* a single step of the allocation replay (presentation feature) */
struct ReplayStep {
    int index = 0;
    std::string roll, name, room;
    int row = 0, col = 0;
    int conflictsSoFar = 0;
};

#endif /* SMARTSEAT_MODELS_H */
