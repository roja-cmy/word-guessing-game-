/* ============================================================================
   backend/services/AllocationEngine.h — the SMARTSEAT allocation brain
   ----------------------------------------------------------------------------
   Consolidates:
        services/AllocationEngine.h  -> class AllocationEngine
        services/ConflictEngine.h    -> class ConflictEngine

   HOW THE INTELLIGENT ALLOCATION WORKS
   ------------------------------------
   1. Students are grouped by SUBJECT (C++ container: std::map).
   2. The groups are interleaved round-robin so that consecutive students in
      the waiting line never share the same paper.
   3. The interleaved line is pushed into an AllocationQueue (QUEUE) and served
      one student at a time.
   4. For every student the engine SCANS the 2D seat grid of every room
      (2D ARRAY + SEARCHING) and scores each free seat:
         same paper in a left/right/front/back neighbour  -> +60 penalty
         same section in a neighbour                      -> +15
         same class in a neighbour                        -> + 8
         neighbour already sat beside me in a past exam    -> +12  (memory)
   5. The seat with the lowest penalty wins (0 = perfect). Nothing is assigned
      sequentially, so students with the same paper are separated whenever the
      room layout allows it.
   6. ConflictEngine then inspects every seat again and reports the remaining
      proximity problems (HIGH = same paper adjacent, MEDIUM = same paper
      diagonal or same section adjacent).
   7. The Allocation Quality Score is computed from conflicts, vacant seats,
      room utilisation and invalid assignments.
   ========================================================================== */
#ifndef SMARTSEAT_ALLOCATION_ENGINE_H
#define SMARTSEAT_ALLOCATION_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctime>

#include "models/Models.h"
#include "structures/Structures.h"

/* -------------------------------------------------------------------------- */
/*  CONFLICT ENGINE — same-paper proximity detection                          */
/* -------------------------------------------------------------------------- */
class AllocationEngine;                    /* forward declaration */

class ConflictEngine {
public:
    /* inspects every occupied seat and its 8 neighbours */
    static void detect(AllocationEngine& engine);

    static int count(const std::vector<Conflict>& list, const std::string& severity) {
        int n = 0;
        for (size_t i = 0; i < list.size(); i++)
            if (severity == "ALL" || list[i].severity == severity) n++;
        return n;
    }
    static int open(const std::vector<Conflict>& list) {
        int n = 0;
        for (size_t i = 0; i < list.size(); i++) if (list[i].status == "OPEN") n++;
        return n;
    }
};

/* -------------------------------------------------------------------------- */
/*  ALLOCATION ENGINE                                                         */
/* -------------------------------------------------------------------------- */
class AllocationEngine {
public:
    /* ------------------------- state ------------------------- */
    std::vector<Student>   students;                 /* C++ container    */
    std::vector<Classroom> rooms;                    /* 2D seat grids    */
    std::vector<Conflict>  conflicts;
    std::vector<AllocationRecord> allocations;

    std::map<std::string, AllocationRecord> seatOfRoll;      /* roll -> seat   */
    std::map<std::string, std::string>      rollAtSeat;      /* "ROOM|R|C"->roll */
    std::map<std::string, StudentList*>     history;         /* roll -> LINKED LIST */
    std::map<std::string, std::vector<std::string> > memory; /* multi-exam memory  */

    HistoryStack undoStack;                           /* STACK for undo  */
    HistoryStack redoStack;                           /* STACK for redo  */

    int qualityScore = 0;
    std::vector<QualityFactor> qualityFactors;
    int reallocationCount = 0;
    std::vector<ReplayStep> replaySteps;
    int lastUnresolved = 0;
    std::string examName = "End Semester Examination";
    std::string examDate = "2026-04-18";

    AllocationEngine() {}
    ~AllocationEngine() { clearHistory(); }

    /* ---------------------- basic lookups --------------------- */
    Student* findStudent(const std::string& roll) {              /* SEARCHING */
        for (size_t i = 0; i < students.size(); i++)
            if (students[i].roll == roll) return &students[i];
        return 0;
    }
    int roomIndex(const std::string& name) const {
        for (size_t i = 0; i < rooms.size(); i++)
            if (rooms[i].name == name) return (int)i;
        return -1;
    }
    Classroom* room(const std::string& name) {
        int i = roomIndex(name);
        return i < 0 ? 0 : &rooms[i];
    }
    bool wasNeighbourBefore(const std::string& a, const std::string& b) {
        std::map<std::string, std::vector<std::string> >::iterator it = memory.find(a);
        if (it == memory.end()) return false;
        for (size_t i = 0; i < it->second.size(); i++)            /* linear search */
            if (it->second[i] == b) return true;
        return false;
    }

    /* --------------------- history helpers -------------------- */
    void clearHistory() {
        for (std::map<std::string, StudentList*>::iterator it = history.begin();
             it != history.end(); ++it) { delete it->second; it->second = 0; }
        history.clear();
    }
    void addHistory(const std::string& roll, const std::string& from,
                    const std::string& to, const std::string& reason) {
        if (history.find(roll) == history.end()) history[roll] = new StudentList();
        Move m; m.roll = roll; m.from = from; m.to = to; m.reason = reason;
        char buf[32]; time_t t = time(0);
        strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
        m.time = buf;
        history[roll]->append(m);
    }
    std::vector<Move> historyOf(const std::string& roll) {
        std::map<std::string, StudentList*>::iterator it = history.find(roll);
        if (it == history.end() || it->second == 0) return std::vector<Move>();
        return it->second->toVector();
    }
    std::vector<Move> allHistory() {
        std::vector<Move> out;
        for (std::map<std::string, StudentList*>::iterator it = history.begin();
             it != history.end(); ++it) {
            if (it->second == 0) continue;
            std::vector<Move> v = it->second->toVector();
            for (size_t i = 0; i < v.size(); i++) out.push_back(v[i]);
        }
        return out;
    }

    /* --------------------- allocation reset ------------------- */
    void clearAllocation() {
        allocations.clear();
        seatOfRoll.clear();
        rollAtSeat.clear();
        conflicts.clear();
        undoStack.clear();
        redoStack.clear();
        replaySteps.clear();
        for (size_t i = 0; i < rooms.size(); i++) {
            rooms[i].build();
            /* keep the blocked seats of the room layout */
        }
    }

    /* ======================================================================
       1. INTERLEAVE THE WAITING LINE (no two neighbours share a paper)
       ====================================================================== */
    std::vector<Student> planOrder() {
        /* group by subject */
        std::map<std::string, std::vector<Student> > groups;
        for (size_t i = 0; i < students.size(); i++)
            groups[students[i].subject].push_back(students[i]);

        std::vector<std::vector<Student> > buckets;
        for (std::map<std::string, std::vector<Student> >::iterator it = groups.begin();
             it != groups.end(); ++it) buckets.push_back(it->second);

        /* simple selection sort: biggest subject group first (arrays + loops) */
        for (size_t i = 0; i < buckets.size(); i++)
            for (size_t j = i + 1; j < buckets.size(); j++)
                if (buckets[j].size() > buckets[i].size()) {
                    std::vector<Student> t = buckets[i]; buckets[i] = buckets[j]; buckets[j] = t;
                }

        /* round-robin: take one student from each group in turn */
        std::vector<Student> order;
        std::vector<int> cursor(buckets.size(), 0);
        bool more = true;
        while (more) {
            more = false;
            for (size_t g = 0; g < buckets.size(); g++) {
                if (cursor[g] < (int)buckets[g].size()) {
                    order.push_back(buckets[g][cursor[g]++]);
                    more = true;
                }
            }
        }
        return order;
    }

    /* ======================================================================
       2. SEAT SCORING — how bad is this seat for this student?
       ====================================================================== */
    int seatPenalty(Classroom& rm, int r, int c, const Student& st) {
        int penalty = 0;
        static const int dr[4] = { 0, 0, -1, 1 };
        static const int dc[4] = { -1, 1, 0, 0 };
        for (int k = 0; k < 4; k++) {
            int rr = r + dr[k], cc = c + dc[k];
            if (!rm.inside(rr, cc)) continue;
            const Seat& nb = rm.grid[rr - 1][cc - 1];
            if (nb.roll == "") continue;
            Student* other = findStudent(nb.roll);
            if (other == 0) continue;
            if (other->subject == st.subject)        penalty += 60;   /* same paper  */
            else if (other->section == st.section)   penalty += 15;   /* same section*/
            else if (other->className == st.className) penalty += 8;  /* same class  */
            if (wasNeighbourBefore(st.roll, other->roll)) penalty += 12;
        }
        return penalty;
    }

    /* find the best free seat in every room for one student */
    bool bestSeat(const Student& st, std::string& outRoom, int& outR, int& outC) {
        int bestPenalty = 1000000;
        bool found = false;
        for (size_t i = 0; i < rooms.size(); i++) {
            Classroom& rm = rooms[i];
            for (int r = 1; r <= rm.rows; r++) {
                /* serpentine scan: left-to-right on odd rows, right-to-left on even */
                for (int step = 0; step < rm.cols; step++) {
                    int c = (r % 2 == 1) ? (step + 1) : (rm.cols - step);
                    if (!rm.free(r, c)) continue;
                    int p = seatPenalty(rm, r, c, st);
                    if (p < bestPenalty) {
                        bestPenalty = p; found = true;
                        outRoom = rm.name; outR = r; outC = c;
                    }
                    if (bestPenalty == 0) break;      /* perfect seat, stop searching */
                }
                if (bestPenalty == 0) break;
            }
            if (bestPenalty == 0) break;
        }
        return found;
    }

    /* place a student on a seat and register everything */
    void place(const Student& st, const std::string& roomName, int r, int c,
               const std::string& reason, bool recordHistory) {
        Classroom* rm = room(roomName);
        if (rm == 0) return;
        rm->grid[r - 1][c - 1].roll = st.roll;

        AllocationRecord rec; rec.roll = st.roll; rec.room = roomName;
        rec.row = r; rec.col = c;
        allocations.push_back(rec);
        seatOfRoll[st.roll] = rec;
        rollAtSeat[roomName + "|" + std::to_string(r) + "|" + std::to_string(c)] = st.roll;
        if (recordHistory) addHistory(st.roll, "—", rec.seat(), reason);
    }
    void unplace(const std::string& roll) {
        std::map<std::string, AllocationRecord>::iterator it = seatOfRoll.find(roll);
        if (it == seatOfRoll.end()) return;
        AllocationRecord rec = it->second;
        Classroom* rm = room(rec.room);
        if (rm) rm->grid[rec.row - 1][rec.col - 1].roll = "";
        rollAtSeat.erase(rec.room + "|" + std::to_string(rec.row) + "|" + std::to_string(rec.col));
        for (size_t i = 0; i < allocations.size(); i++)
            if (allocations[i].roll == roll) { allocations.erase(allocations.begin() + i); break; }
        seatOfRoll.erase(it);
    }

    /* ======================================================================
       3. GENERATE A FULL ARRANGEMENT
       ====================================================================== */
    void generate() {
        clearAllocation();
        std::vector<Student> order = planOrder();

        AllocationQueue queue;                       /* QUEUE: the waiting line */
        for (size_t i = 0; i < order.size(); i++) queue.enqueue(order[i]);

        std::vector<Student> served = queue.toVector();   /* keep the order for replay */
        Student st;
        int idx = 0;
        while (queue.dequeue(st)) {
            std::string rm; int r = 0, c = 0;
            if (bestSeat(st, rm, r, c)) place(st, rm, r, c, "Initial allocation", idx < 400);
            idx++;
        }

        ConflictEngine::detect(*this);
        computeQuality();
        buildReplay(served);
        rememberNeighbours();
    }

    /* ======================================================================
       4. QUALITY SCORE
       ====================================================================== */
    void computeQuality() {
        int high = 0, medium = 0, low = 0;
        for (size_t i = 0; i < conflicts.size(); i++) {
            if (conflicts[i].severity == "HIGH") high++;
            else if (conflicts[i].severity == "MEDIUM") medium++;
            else low++;
        }
        int invalid = 0;
        for (size_t i = 0; i < students.size(); i++)
            if (seatOfRoll.find(students[i].roll) == seatOfRoll.end()) invalid++;

        double util = 0; int usedRooms = 0;
        for (size_t i = 0; i < rooms.size(); i++) {
            if (rooms[i].occupied() > 0) { util += rooms[i].utilization(); usedRooms++; }
        }
        if (usedRooms) util /= usedRooms;

        int score = 100;
        qualityFactors.clear();

        int p1 = high * 4;  if (p1 > 40) p1 = 40;  score -= p1;
        int p2 = medium * 2; if (p2 > 20) p2 = 20; score -= p2;
        int p3 = low * 1;   if (p3 > 10) p3 = 10;  score -= p3;
        int p4 = invalid * 8; if (p4 > 24) p4 = 24; score -= p4;
        int p5 = 0;
        if (util < 60) { p5 = (int)((60.0 - util) * 0.5); if (p5 > 15) p5 = 15; score -= p5; }

        char buf[160];
        QualityFactor f;
        f.label = "Same-paper conflicts"; f.points = -p1;
        sprintf(buf, "%d high severity seat pairs still share a paper", high);
        f.detail = buf; qualityFactors.push_back(f);
        f.label = "Medium proximity issues"; f.points = -p2;
        sprintf(buf, "%d diagonal same-paper or same-section neighbours", medium);
        f.detail = buf; qualityFactors.push_back(f);
        f.label = "Minor proximity issues"; f.points = -p3;
        sprintf(buf, "%d same-class neighbours", low); f.detail = buf; qualityFactors.push_back(f);
        f.label = "Unseated students"; f.points = -p4;
        sprintf(buf, "%d students could not be placed", invalid); f.detail = buf; qualityFactors.push_back(f);
        f.label = "Room utilisation"; f.points = -p5;
        sprintf(buf, "Average utilisation of used rooms is %.0f%%", util); f.detail = buf;
        qualityFactors.push_back(f);

        if (score < 0) score = 0;
        qualityScore = score;
    }

    /* ======================================================================
       5. MANUAL MOVE + UNDO / REDO  (STACK)
       ====================================================================== */
    bool moveStudent(const std::string& roll, const std::string& roomName,
                     int r, int c, std::string& error) {
        Student* st = findStudent(roll);
        if (st == 0) { error = "Student " + roll + " was not found."; return false; }
        Classroom* rm = room(roomName);
        if (rm == 0) { error = "Room " + roomName + " does not exist."; return false; }
        if (!rm->inside(r, c)) { error = "Invalid seat position (room dimensions)."; return false; }
        if (!rm->free(r, c)) {
            error = rm->grid[r - 1][c - 1].blocked ? "That seat is blocked."
                                                   : "That seat is already occupied.";
            return false;
        }
        AllocationRecord old;
        bool had = false;
        std::map<std::string, AllocationRecord>::iterator it = seatOfRoll.find(roll);
        if (it != seatOfRoll.end()) { old = it->second; had = true; }

        if (had) unplace(roll);
        place(*st, roomName, r, c, "Manual move", true);

        Action a; a.type = "MOVE"; a.roll = roll;
        if (had) { a.fromRoom = old.room; a.fromRow = old.row; a.fromCol = old.col; }
        a.toRoom = roomName; a.toRow = r; a.toCol = c;
        a.note = "Moved to " + roomName + "-R" + std::to_string(r) + "-C" + std::to_string(c);
        undoStack.push(a);
        redoStack.clear();

        ConflictEngine::detect(*this);
        computeQuality();
        return true;
    }

    bool undo(std::string& message) {
        Action a;
        if (!undoStack.pop(a)) { message = "Nothing left to undo."; return false; }
        if (a.type == "MOVE") {
            if (a.fromRoom == "") {                       /* the seat was a fresh one */
                unplace(a.roll);
            } else {
                Student* st = findStudent(a.roll);
                if (st) { unplace(a.roll); place(*st, a.fromRoom, a.fromRow, a.fromCol, "Undo", true); }
            }
            message = "Undo: " + a.roll + " returned to " +
                      (a.fromRoom == "" ? "no seat" :
                       a.fromRoom + "-R" + std::to_string(a.fromRow) + "-C" + std::to_string(a.fromCol));
        } else if (a.type == "BLOCK" || a.type == "UNBLOCK") {
            Classroom* rm = room(a.toRoom);
            if (rm) { rm->grid[a.toRow - 1][a.toCol - 1].blocked = (a.type == "UNBLOCK"); }
            message = "Undo: seat status restored.";
        }
        redoStack.push(a);
        ConflictEngine::detect(*this);
        computeQuality();
        return true;
    }

    bool redo(std::string& message) {
        Action a;
        if (!redoStack.pop(a)) { message = "Nothing left to redo."; return false; }
        if (a.type == "MOVE") {
            Student* st = findStudent(a.roll);
            if (st) { unplace(a.roll); place(*st, a.toRoom, a.toRow, a.toCol, "Redo", true); }
            message = "Redo: " + a.roll + " moved again.";
        } else if (a.type == "BLOCK" || a.type == "UNBLOCK") {
            Classroom* rm = room(a.toRoom);
            if (rm) rm->grid[a.toRow - 1][a.toCol - 1].blocked = (a.type == "BLOCK");
            message = "Redo: seat status reapplied.";
        }
        undoStack.push(a);
        ConflictEngine::detect(*this);
        computeQuality();
        return true;
    }

    /* ======================================================================
       6. BLOCK / UNBLOCK A SEAT
       ====================================================================== */
    bool setBlocked(const std::string& roomName, int r, int c, bool blocked, std::string& error) {
        Classroom* rm = room(roomName);
        if (rm == 0 || !rm->inside(r, c)) { error = "Invalid seat."; return false; }
        Seat& s = rm->grid[r - 1][c - 1];
        if (blocked && s.roll != "") { error = "Seat is occupied — move the student first."; return false; }
        s.blocked = blocked;
        Action a; a.type = blocked ? "BLOCK" : "UNBLOCK"; a.toRoom = roomName;
        a.toRow = r; a.toCol = c; a.note = "Seat status changed";
        undoStack.push(a); redoStack.clear();
        return true;
    }

    /* ======================================================================
       7. EMERGENCY REALLOCATION  (QUEUE)
       ====================================================================== */
    struct ReallocResult { int affected, reallocated, unresolved, roomsUsed; };

    ReallocResult emergencyReallocate(const std::string& closedRoom) {
        ReallocResult res; res.affected = 0; res.reallocated = 0; res.unresolved = 0; res.roomsUsed = 0;

        /* 1. identify the affected students by searching the room grid */
        std::vector<Student> affected;
        for (int r = 1; r <= room(closedRoom)->rows; r++)
            for (int c = 1; c <= room(closedRoom)->cols; c++) {
                std::string roll = room(closedRoom)->grid[r - 1][c - 1].roll;
                if (roll == "") continue;
                Student* st = findStudent(roll);
                if (st) { affected.push_back(*st); unplace(roll); }
            }
        res.affected = (int)affected.size();

        /* 2. put them into the QUEUE (waiting line) */
        AllocationQueue queue;
        for (size_t i = 0; i < affected.size(); i++) queue.enqueue(affected[i]);

        /* 3. serve them into the other rooms */
        std::map<std::string, bool> used;
        Student st;
        while (queue.dequeue(st)) {
            std::string rm; int r = 0, c = 0;
            bool placed = false;
            /* search the other rooms only */
            for (size_t i = 0; i < rooms.size() && !placed; i++) {
                if (rooms[i].name == closedRoom) continue;
                int best = 1000000;
                for (int rr = 1; rr <= rooms[i].rows; rr++)
                    for (int cc = 1; cc <= rooms[i].cols; cc++) {
                        if (!rooms[i].free(rr, cc)) continue;
                        int p = seatPenalty(rooms[i], rr, cc, st);
                        if (p < best) { best = p; rm = rooms[i].name; r = rr; c = cc; placed = true; }
                    }
            }
            if (placed) {
                place(st, rm, r, c, "Emergency reallocation", true);
                used[rm] = true;
                res.reallocated++;
                Action a; a.type = "MOVE"; a.roll = st.roll; a.fromRoom = closedRoom;
                a.fromRow = 0; a.fromCol = 0; a.toRoom = rm; a.toRow = r; a.toCol = c;
                a.note = "Emergency reallocation from " + closedRoom;
                undoStack.push(a);
            } else res.unresolved++;
        }
        redoStack.clear();
        res.roomsUsed = (int)used.size();
        reallocationCount += res.reallocated;

        ConflictEngine::detect(*this);
        computeQuality();
        rememberNeighbours();
        return res;
    }

    /* ======================================================================
       8. RECOMMENDED SEATS FOR A MANUAL REALLOCATION
       ====================================================================== */
    struct SeatSuggestion { std::string room; int row, col, quality; std::vector<std::string> reasons; };

    std::vector<SeatSuggestion> recommend(const std::string& roll, int limit) {
        std::vector<SeatSuggestion> out;
        Student* st = findStudent(roll);
        if (st == 0) return out;
        for (size_t i = 0; i < rooms.size(); i++) {
            Classroom& rm = rooms[i];
            for (int r = 1; r <= rm.rows; r++)
                for (int c = 1; c <= rm.cols; c++) {
                    if (!rm.free(r, c)) continue;
                    int p = seatPenalty(rm, r, c, *st);
                    SeatSuggestion s; s.room = rm.name; s.row = r; s.col = c;
                    s.quality = 100 - p; if (s.quality < 0) s.quality = 0;
                    if (p == 0) s.reasons.push_back("No same-paper or same-section neighbour");
                    if (p < 15) s.reasons.push_back("Excellent separation quality");
                    if (rm.utilization() >= 70) s.reasons.push_back("Good room utilisation");
                    else s.reasons.push_back("Room has spare capacity");
                    s.reasons.push_back("Seat is free and not blocked");
                    out.push_back(s);
                }
        }
        /* selection sort by quality, best first */
        for (size_t i = 0; i < out.size(); i++)
            for (size_t j = i + 1; j < out.size(); j++)
                if (out[j].quality > out[i].quality) { SeatSuggestion t = out[i]; out[i] = out[j]; out[j] = t; }
        if ((int)out.size() > limit) out.resize(limit);
        return out;
    }

    /* ======================================================================
       9. MULTI-EXAM MEMORY  (avoid repeating neighbours)
       ====================================================================== */
    void rememberNeighbours() {
        static const int dr[4] = { 0, 0, -1, 1 };
        static const int dc[4] = { -1, 1, 0, 0 };
        for (size_t i = 0; i < rooms.size(); i++) {
            Classroom& rm = rooms[i];
            for (int r = 1; r <= rm.rows; r++)
                for (int c = 1; c <= rm.cols; c++) {
                    std::string roll = rm.grid[r - 1][c - 1].roll;
                    if (roll == "") continue;
                    for (int k = 0; k < 4; k++) {
                        int rr = r + dr[k], cc = c + dc[k];
                        if (!rm.inside(rr, cc)) continue;
                        std::string nb = rm.grid[rr - 1][cc - 1].roll;
                        if (nb == "" || nb == roll) continue;
                        std::vector<std::string>& list = memory[roll];
                        bool exists = false;
                        for (size_t q = 0; q < list.size(); q++) if (list[q] == nb) { exists = true; break; }
                        if (!exists) list.push_back(nb);
                    }
                }
        }
    }

    /* ======================================================================
       10. REPLAY STEPS for the presentation animation
       ====================================================================== */
    void buildReplay(const std::vector<Student>& order) {
        replaySteps.clear();
        /* replay the same order and count the conflicts that appeared so far */
        std::vector<Classroom> shadow = rooms;
        for (size_t i = 0; i < shadow.size(); i++) shadow[i].build();
        int conflictsSoFar = 0;
        for (size_t i = 0; i < order.size(); i++) {
            std::map<std::string, AllocationRecord>::iterator it = seatOfRoll.find(order[i].roll);
            if (it == seatOfRoll.end()) continue;
            ReplayStep s; s.index = (int)i + 1; s.roll = order[i].roll; s.name = order[i].name;
            s.room = it->second.room; s.row = it->second.row; s.col = it->second.col;
            /* count same-paper neighbours on the shadow grid */
            int ri = roomIndex(it->second.room);
            if (ri >= 0) {
                shadow[ri].grid[it->second.row - 1][it->second.col - 1].roll = order[i].roll;
                static const int dr[4] = { 0, 0, -1, 1 };
                static const int dc[4] = { -1, 1, 0, 0 };
                for (int k = 0; k < 4; k++) {
                    int rr = it->second.row + dr[k], cc = it->second.col + dc[k];
                    if (!shadow[ri].inside(rr, cc)) continue;
                    std::string nb = shadow[ri].grid[rr - 1][cc - 1].roll;
                    if (nb == "") continue;
                    Student* other = findStudent(nb);
                    if (other && other->subject == order[i].subject) conflictsSoFar++;
                }
            }
            s.conflictsSoFar = conflictsSoFar;
            replaySteps.push_back(s);
        }
    }

    /* neighbour information for the student seat view */
    struct Neighbour { std::string roll, name, subject, section, seat, relation; bool samePaper; };
    std::vector<Neighbour> neighboursOf(const std::string& roll) {
        std::vector<Neighbour> out;
        std::map<std::string, AllocationRecord>::iterator it = seatOfRoll.find(roll);
        if (it == seatOfRoll.end()) return out;
        AllocationRecord rec = it->second;
        Classroom* rm = room(rec.room);
        if (!rm) return out;
        static const int dr[4] = { -1, 1, 0, 0 };
        static const int dc[4] = { 0, 0, -1, 1 };
        static const char* rel[4] = { "FRONT", "BACK", "LEFT", "RIGHT" };
        for (int k = 0; k < 4; k++) {
            int rr = rec.row + dr[k], cc = rec.col + dc[k];
            Neighbour n; n.relation = rel[k]; n.samePaper = false; n.roll = ""; n.seat = "";
            if (rm->inside(rr, cc)) {
                Seat& s = rm->grid[rr - 1][cc - 1];
                n.seat = s.code(rec.room);
                if (s.roll != "") {
                    Student* other = findStudent(s.roll);
                    if (other) {
                        n.roll = other->roll; n.name = other->name;
                        n.subject = other->subject; n.section = other->section;
                        n.samePaper = (other->subject == findStudent(roll)->subject);
                    }
                }
            }
            out.push_back(n);
        }
        return out;
    }
};

/* -------------------------------------------------------------------------- */
/*  CONFLICT ENGINE implementation (needs the full AllocationEngine type)      */
/* -------------------------------------------------------------------------- */
inline void ConflictEngine::detect(AllocationEngine& e) {
    e.conflicts.clear();
    static const int dr[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
    static const int dc[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
    int id = 1;
    for (size_t i = 0; i < e.rooms.size(); i++) {
        Classroom& rm = e.rooms[i];
        for (int r = 1; r <= rm.rows; r++)
            for (int c = 1; c <= rm.cols; c++) {
                Seat& s = rm.grid[r - 1][c - 1];
                if (s.roll == "") continue;
                Student* a = e.findStudent(s.roll);
                if (!a) continue;
                for (int k = 0; k < 8; k++) {
                    int rr = r + dr[k], cc = c + dc[k];
                    if (!rm.inside(rr, cc)) continue;
                    Seat& t = rm.grid[rr - 1][cc - 1];
                    if (t.roll == "" || t.roll == s.roll) continue;
                    /* report each pair only once */
                    if (s.roll > t.roll) continue;
                    Student* b = e.findStudent(t.roll);
                    if (!b) continue;
                    Conflict cf; cf.id = id++; cf.room = rm.name;
                    cf.seat = s.code(rm.name); cf.roll = a->roll; cf.name = a->name;
                    cf.subject = a->subject;
                    cf.neighbourSeat = t.code(rm.name); cf.neighbourRoll = b->roll;
                    cf.neighbourName = b->name; cf.neighbourSubject = b->subject;
                    cf.status = "OPEN";
                    bool adjacent = (k < 4);
                    if (a->subject == b->subject) {
                        cf.type = "SAME_PAPER";
                        cf.severity = adjacent ? "HIGH" : "MEDIUM";
                    } else if (a->section == b->section) {
                        cf.type = "SAME_SECTION";
                        cf.severity = adjacent ? "MEDIUM" : "LOW";
                    } else if (a->className == b->className) {
                        cf.type = "SAME_CLASS"; cf.severity = "LOW";
                    } else continue;
                    e.conflicts.push_back(cf);
                }
            }
    }
}

#endif /* SMARTSEAT_ALLOCATION_ENGINE_H */
