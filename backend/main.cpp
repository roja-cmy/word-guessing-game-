/* ============================================================================
   backend/main.cpp — SMARTSEAT C++ backend (HTTP API server)
   ----------------------------------------------------------------------------
   A dependency-free C++ HTTP server (POSIX/Winsock sockets) that exposes the
   REST API consumed by the React frontend:

     GET  /api/health                    GET  /api/dashboard
     GET  /api/students                  POST /api/students/upload
     GET  /api/classrooms                POST /api/rooms/block
     POST /api/allocation/generate       POST /api/allocation/simulate
     POST /api/allocation/apply          GET  /api/allocation
     POST /api/allocation/move           POST /api/allocation/reallocation
     POST /api/allocation/undo           POST /api/allocation/redo
     GET  /api/recommendations/{roll}    GET  /api/replay
     POST /api/student/search            GET  /api/student/{roll}
     GET  /api/conflicts                 POST /api/conflicts/resolve
     GET  /api/analytics                 GET  /api/history
     POST /api/bootstrap                 GET  /api/rooms

   BUILD & RUN
     Windows : g++ -std=c++11 -O2 backend/main.cpp -lws2_32 -o smartseat.exe
               smartseat.exe            →  http://localhost:8080
     Linux   : g++ -std=c++11 -O2 backend/main.cpp -o smartseat && ./smartseat

   The frontend talks to it through fetch() when it is running; otherwise the
   page uses the bundled preview engine that mirrors this logic.
   ========================================================================== */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cstdio>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <thread>
  #define closesocket close
#endif

#include "models/Models.h"
#include "structures/Structures.h"
#include "services/AllocationEngine.h"
#include "services/SearchEngine.h"
#include "utils/JsonHandler.h"

using namespace std;

/* ==========================================================================
   1. SAMPLE DATA  (132 students · 5 classrooms · 7 subjects · 4 departments)
   ========================================================================== */
static unsigned int seedValue = 20260418;
static int nextRand(int mod) {                 /* tiny deterministic LCG */
    seedValue = (seedValue * 1103515245 + 12345) & 0x7fffffff;
    return (int)(seedValue % (unsigned int)mod);
}

static void buildSampleData(AllocationEngine& e) {
    const char* first[] = { "Hansika","Haneesha","Hanish","Arjun","Meera","Ravi","Sneha","Karthik",
        "Divya","Naveen","Priya","Vishnu","Ananya","Rahul","Keerthi","Surya","Lakshmi","Aditya",
        "Deepa","Manoj","Sowmya","Harish","Nithya","Praveen","Swathi","Gopal","Ramya","Vikram",
        "Jaya","Sathish" };
    const char* last[]  = { "Reddy","Sharma","Iyer","Nair","Rao","Verma","Menon","Pillai","Das",
        "Gupta","Khan","Joshi","Bose","Mehta" };
    const char* dept[]  = { "CSE","ECE","MECH","IT" };
    const char* sect[]  = { "A","B","C" };
    const char* subj[]  = { "Mathematics","Physics","Data Structures","DBMS",
                            "Operating Systems","Digital Electronics","Thermodynamics" };
    const char* cls[]   = { "II Year","III Year" };

    e.students.clear();
    e.rooms.clear();
    e.clearHistory();
    e.memory.clear();

    for (int i = 0; i < 132; i++) {
        string roll = "23" + string(dept[i % 4]) + (i < 10 ? "00" : (i < 100 ? "0" : ""))
                    + to_string(i + 1);
        string name = string(first[nextRand(30)]) + " " + string(last[nextRand(14)]);
        Student s(roll, name, dept[i % 4], cls[nextRand(2)], sect[nextRand(3)],
                  subj[nextRand(7)], "End Semester Examination");
        e.students.push_back(s);
    }

    e.rooms.push_back(Classroom("A-101", 5, 8));
    e.rooms.push_back(Classroom("A-102", 5, 7));
    e.rooms.push_back(Classroom("B-204", 4, 8));
    e.rooms.push_back(Classroom("B-205", 4, 7));
    e.rooms.push_back(Classroom("C-301", 5, 7));

    /* a few blocked seats (broken desks / camera positions) */
    e.rooms[0].grid[2][3].blocked = true;   e.rooms[0].grid[2][4].blocked = true;
    e.rooms[1].grid[1][1].blocked = true;   e.rooms[1].grid[4][6].blocked = true;
    e.rooms[2].grid[3][5].blocked = true;   e.rooms[3].grid[0][0].blocked = true;
    e.rooms[4].grid[2][2].blocked = true;

    e.generate();
}

/* ==========================================================================
   2. JSON SERIALISERS
   ========================================================================== */
static string studentJson(const Student& s) {
    return Json::obj(vector<string>{
        Json::kv("roll", s.roll), Json::kv("name", s.name),
        Json::kv("department", s.department), Json::kv("className", s.className),
        Json::kv("section", s.section), Json::kv("subject", s.subject),
        Json::kv("exam", s.exam) });
}
static string roomJson(const Classroom& r) {
    vector<string> blocked;
    for (int i = 0; i < r.rows; i++)
        for (int j = 0; j < r.cols; j++)
            if (r.grid[i][j].blocked) blocked.push_back(r.grid[i][j].code(r.name));
    char buf[32];
    sprintf(buf, "%.1f", r.utilization());
    return Json::obj(vector<string>{
        Json::kv("name", r.name), Json::kvn("rows", r.rows), Json::kvn("cols", r.cols),
        Json::kvn("capacity", r.capacity()), Json::kvn("occupied", r.occupied()),
        Json::kvn("vacant", r.vacant()), Json::kvn("blocked", r.blockedSeats()),
        Json::kv("utilization", buf), Json::arrS(blocked) });
}
static string seatGridJson(const Classroom& r, AllocationEngine& e) {
    vector<string> rows;
    for (int i = 0; i < r.rows; i++) {
        vector<string> cells;
        for (int j = 0; j < r.cols; j++) {
            const Seat& s = r.grid[i][j];
            string roll = s.roll, name = "", subject = "", section = "";
            if (roll != "") {
                Student* st = e.findStudent(roll);
                if (st) { name = st->name; subject = st->subject; section = st->section; }
            }
            cells.push_back(Json::obj(vector<string>{
                Json::kvn("row", s.row), Json::kvn("col", s.col),
                Json::kv("roll", roll), Json::kv("name", name),
                Json::kv("subject", subject), Json::kv("section", section),
                Json::kv("status", s.status()) }));
        }
        rows.push_back(Json::arr(cells));
    }
    return Json::arr(rows);
}
static string conflictJson(const Conflict& c) {
    return Json::obj(vector<string>{
        Json::kvn("id", c.id), Json::kv("room", c.room), Json::kv("seat", c.seat),
        Json::kv("roll", c.roll), Json::kv("name", c.name), Json::kv("subject", c.subject),
        Json::kv("neighbourSeat", c.neighbourSeat), Json::kv("neighbourRoll", c.neighbourRoll),
        Json::kv("neighbourName", c.neighbourName), Json::kv("neighbourSubject", c.neighbourSubject),
        Json::kv("type", c.type), Json::kv("severity", c.severity), Json::kv("status", c.status) });
}
static string moveJson(const Move& m) {
    return Json::obj(vector<string>{
        Json::kv("roll", m.roll), Json::kv("from", m.from), Json::kv("to", m.to),
        Json::kv("reason", m.reason), Json::kv("time", m.time) });
}

static AllocationEngine engine;                 /* the single engine instance */

/* ==========================================================================
   3. ROUTER  (method + path + body  ->  JSON response)
   ========================================================================== */
static string route(const string& method, string path, const string& body) {

    /* strip the query string, keep it for filters */
    string query = "";
    size_t q = path.find('?');
    if (q != string::npos) { query = path.substr(q + 1); path = path.substr(0, q); }

    map<string, string> params;
    {   /* parse a=1&b=2 */
        stringstream ss(query); string item;
        while (getline(ss, item, '&')) {
            size_t e = item.find('=');
            if (e != string::npos) params[item.substr(0, e)] = item.substr(e + 1);
        }
    }
    map<string, string> payload;
    if (body.size() && body[0] == '{') JsonParser(body).object(payload);

    /* ---------------- health / bootstrap ---------------- */
    if (path == "/api/health")
        return Json::obj(vector<string>{ Json::kv("app", "SMARTSEAT"),
            Json::kv("engine", "C++"), Json::kv("status", "running") });

    if (path == "/api/bootstrap") { buildSampleData(engine);
        return Json::obj(vector<string>{ Json::kvn("students", engine.students.size()),
            Json::kvn("rooms", engine.rooms.size()), Json::kv("message", "Sample dataset loaded") }); }

    /* ---------------- dashboard ---------------- */
    if (path == "/api/dashboard") {
        int seats = 0, occupied = 0, vacant = 0, blocked = 0;
        for (size_t i = 0; i < engine.rooms.size(); i++) {
            seats += engine.rooms[i].capacity(); occupied += engine.rooms[i].occupied();
            vacant += engine.rooms[i].vacant(); blocked += engine.rooms[i].blockedSeats();
        }
        map<string, int> subjects = AnalyticsEngine::countBy(engine.students, "subject");
        return Json::obj(vector<string>{
            Json::kvn("students", engine.students.size()), Json::kvn("rooms", engine.rooms.size()),
            Json::kvn("seats", seats), Json::kvn("subjects", (int)subjects.size()),
            Json::kvn("occupied", occupied), Json::kvn("vacant", vacant),
            Json::kvn("blocked", blocked),
            Json::kvn("conflicts", (int)engine.conflicts.size()),
            Json::kvn("highConflicts", ConflictEngine::count(engine.conflicts, "HIGH")),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("undo", engine.undoStack.size()), Json::kvn("redo", engine.redoStack.size()),
            Json::kvn("reallocationCount", engine.reallocationCount) });
    }

    /* ---------------- students (search + filters) ---------------- */
    if (path == "/api/students") {
        string q = params.count("q") ? params["q"] : "";
        string subject = params.count("subject") ? params["subject"] : "";
        string section = params.count("section") ? params["section"] : "";
        string room = params.count("room") ? params["room"] : "";
        string conflictOnly = params.count("conflict") ? params["conflict"] : "";

        vector<Student> list = engine.students;
        if (q != "") list = SearchEngine::search(list, q,
                            params.count("field") ? params["field"] : "name");
        vector<string> out;
        for (size_t i = 0; i < list.size(); i++) {
            if (subject != "" && list[i].subject != subject) continue;
            if (section != "" && list[i].section != section) continue;
            string seat = "", seatRoom = "";
            map<string, AllocationRecord>::iterator it = engine.seatOfRoll.find(list[i].roll);
            if (it != engine.seatOfRoll.end()) { seat = it->second.seat(); seatRoom = it->second.room; }
            if (room != "" && seatRoom != room) continue;
            bool hasConflict = false;
            if (conflictOnly == "true")
                for (size_t j = 0; j < engine.conflicts.size(); j++)
                    if (engine.conflicts[j].roll == list[i].roll ||
                        engine.conflicts[j].neighbourRoll == list[i].roll) { hasConflict = true; break; }
            if (conflictOnly == "true" && !hasConflict) continue;
            vector<string> parts;
            parts.push_back(Json::kv("roll", list[i].roll));
            parts.push_back(Json::kv("name", list[i].name));
            parts.push_back(Json::kv("department", list[i].department));
            parts.push_back(Json::kv("className", list[i].className));
            parts.push_back(Json::kv("section", list[i].section));
            parts.push_back(Json::kv("subject", list[i].subject));
            parts.push_back(Json::kv("seat", seat));
            parts.push_back(Json::kv("room", seatRoom));
            parts.push_back(Json::kv("status", seat == "" ? "PENDING" : "ALLOCATED"));
            parts.push_back(Json::kvb("conflict", hasConflict));
            out.push_back(Json::obj(parts));
        }
        return Json::obj(vector<string>{ Json::kvn("total", out.size()), Json::arr(out) });
    }

    /* ---------------- upload (Excel/CSV rows) ---------------- */
    if (path == "/api/students/upload" && method == "POST") {
        vector<map<string, string> > rows;
        JsonParser p(body);
        if (!p.objectArray(rows) && body.size() && body[0] == '{') {
            map<string, string> one; JsonParser(body).object(one);
            if (!one.empty()) rows.push_back(one);
        }
        UploadResult res = parseUploadedRows(rows);
        for (size_t i = 0; i < res.students.size(); i++)
            engine.students.push_back(res.students[i]);       /* containers */
        engine.generate();
        vector<string> dup, inv;
        for (size_t i = 0; i < res.duplicates.size(); i++) dup.push_back(studentJson(res.duplicates[i]));
        for (size_t i = 0; i < res.invalid.size(); i++) inv.push_back(studentJson(res.invalid[i]));
        return Json::obj(vector<string>{
            Json::kvn("total", res.report.total), Json::kvn("accepted", res.report.accepted),
            Json::kvn("duplicates", res.report.duplicates),
            Json::kvn("missing", res.report.missing), Json::kvn("invalid", res.report.invalid),
            Json::arrS(res.report.messages), Json::arr(dup), Json::arr(inv),
            Json::kvn("students", engine.students.size()),
            Json::kvn("conflicts", engine.conflicts.size()),
            Json::kvn("quality", engine.qualityScore),
            Json::kv("message", "Allocation generated successfully") });
    }

    /* ---------------- classrooms ---------------- */
    if (path == "/api/classrooms" || path == "/api/rooms") {
        vector<string> out;
        for (size_t i = 0; i < engine.rooms.size(); i++) {
            vector<string> parts;
            parts.push_back(Json::kv("name", engine.rooms[i].name));
            parts.push_back(Json::kvn("rows", engine.rooms[i].rows));
            parts.push_back(Json::kvn("cols", engine.rooms[i].cols));
            parts.push_back(Json::kvn("capacity", engine.rooms[i].capacity()));
            parts.push_back(Json::kvn("occupied", engine.rooms[i].occupied()));
            parts.push_back(Json::kvn("vacant", engine.rooms[i].vacant()));
            parts.push_back(Json::kvn("blocked", engine.rooms[i].blockedSeats()));
            char buf[32]; sprintf(buf, "%.1f", engine.rooms[i].utilization());
            parts.push_back(Json::kv("utilization", buf));
            parts.push_back(Json::kv("grid", seatGridJson(engine.rooms[i], engine)));
            out.push_back(Json::obj(parts));
        }
        return Json::obj(vector<string>{ Json::kvn("total", out.size()), Json::arr(out) });
    }

    if (path == "/api/rooms/block" && method == "POST") {
        string err = "";
        bool ok = engine.setBlocked(payload.count("room") ? payload["room"] : "",
                    atoi((payload.count("row") ? payload["row"] : "0").c_str()),
                    atoi((payload.count("col") ? payload["col"] : "0").c_str()),
                    payload.count("blocked") ? payload["blocked"] == "true" : true, err);
        ConflictEngine::detect(engine); engine.computeQuality();
        return Json::obj(vector<string>{ Json::kvb("ok", ok), Json::kv("message", err == "" ? "Seat updated" : err) });
    }

    /* ---------------- allocation ---------------- */
    if (path == "/api/allocation/generate" && method == "POST") {
        engine.generate();
        return Json::obj(vector<string>{
            Json::kvn("allocated", engine.allocations.size()),
            Json::kvn("conflicts", engine.conflicts.size()),
            Json::kvn("quality", engine.qualityScore),
            Json::kv("message", "Allocation generated successfully") });
    }

    if (path == "/api/allocation") {
        vector<string> out;
        for (size_t i = 0; i < engine.allocations.size(); i++) {
            AllocationRecord r = engine.allocations[i];
            Student* s = engine.findStudent(r.roll);
            out.push_back(Json::obj(vector<string>{
                Json::kv("roll", r.roll), Json::kv("name", s ? s->name : ""),
                Json::kv("room", r.room), Json::kvn("row", r.row), Json::kvn("col", r.col),
                Json::kv("seat", r.seat()) }));
        }
        return Json::obj(vector<string>{ Json::kvn("total", out.size()),
            Json::kvn("quality", engine.qualityScore), Json::arr(out) });
    }

    /* what-if simulator: works on a copy, never touches the real arrangement */
    if (path == "/api/allocation/simulate" && method == "POST") {
        int count = payload.count("students") ? atoi(payload["students"].c_str())
                                              : (int)engine.students.size();
        AllocationEngine shadow;
        for (int i = 0; i < count && i < (int)engine.students.size(); i++)
            shadow.students.push_back(engine.students[i]);
        for (size_t i = 0; i < engine.rooms.size(); i++) {
            string n = engine.rooms[i].name;
            if (payload.count("rooms") && payload["rooms"].find(n) == string::npos) continue;
            shadow.rooms.push_back(Classroom(engine.rooms[i].name, engine.rooms[i].rows,
                                             engine.rooms[i].cols));
        }
        int extraBlocked = payload.count("blocked") ? atoi(payload["blocked"].c_str()) : 0;
        for (size_t i = 0; i < shadow.rooms.size() && extraBlocked > 0; i++)
            for (int r = 1; r <= shadow.rooms[i].rows && extraBlocked > 0; r++)
                for (int c = 1; c <= shadow.rooms[i].cols && extraBlocked > 0; c++)
                    if (shadow.rooms[i].free(r, c)) { shadow.rooms[i].grid[r-1][c-1].blocked = true; extraBlocked--; }
        shadow.memory = engine.memory;
        shadow.generate();

        int beforeConf = (int)engine.conflicts.size(), afterConf = (int)shadow.conflicts.size();
        int beforeVac = 0, afterVac = 0;
        for (size_t i = 0; i < engine.rooms.size(); i++) beforeVac += engine.rooms[i].vacant();
        for (size_t i = 0; i < shadow.rooms.size(); i++) afterVac += shadow.rooms[i].vacant();
        vector<string> roomsOut;
        for (size_t i = 0; i < shadow.rooms.size(); i++) roomsOut.push_back(roomJson(shadow.rooms[i]));
        return Json::obj(vector<string>{
            Json::kvn("beforeConflicts", beforeConf), Json::kvn("afterConflicts", afterConf),
            Json::kvn("beforeVacant", beforeVac), Json::kvn("afterVacant", afterVac),
            Json::kvn("beforeQuality", engine.qualityScore),
            Json::kvn("afterQuality", shadow.qualityScore),
            Json::kvn("students", shadow.students.size()),
            Json::kvn("rooms", shadow.rooms.size()),
            Json::arr(roomsOut), Json::kvb("applied", false) });
    }

    /* applying a simulation replaces the real arrangement */
    if (path == "/api/allocation/apply" && method == "POST") {
        int count = payload.count("students") ? atoi(payload["students"].c_str())
                                              : (int)engine.students.size();
        vector<Student> keep;
        for (int i = 0; i < count && i < (int)engine.students.size(); i++)
            keep.push_back(engine.students[i]);
        engine.students = keep;
        if (payload.count("rooms")) {
            vector<Classroom> keepRooms;
            for (size_t i = 0; i < engine.rooms.size(); i++)
                if (payload["rooms"].find(engine.rooms[i].name) != string::npos)
                    keepRooms.push_back(engine.rooms[i]);
            if (!keepRooms.empty()) engine.rooms = keepRooms;
        }
        engine.generate();
        return Json::obj(vector<string>{ Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()),
            Json::kv("message", "Proposed arrangement applied") });
    }

    /* manual move (records history + undo) */
    if (path == "/api/allocation/move" && method == "POST") {
        string err = "";
        bool ok = engine.moveStudent(payload["roll"], payload["room"],
                      atoi(payload["row"].c_str()), atoi(payload["col"].c_str()), err);
        return Json::obj(vector<string>{ Json::kvb("ok", ok),
            Json::kv("message", ok ? "Student moved" : err),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()) });
    }

    /* emergency reallocation through the QUEUE */
    if (path == "/api/allocation/reallocation" && method == "POST") {
        AllocationEngine::ReallocResult r = engine.emergencyReallocate(payload["room"]);
        return Json::obj(vector<string>{
            Json::kvn("affected", r.affected), Json::kvn("reallocated", r.reallocated),
            Json::kvn("unresolved", r.unresolved), Json::kvn("roomsUsed", r.roomsUsed),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()),
            Json::kv("message", "Emergency reallocation complete") });
    }

    if (path == "/api/allocation/undo" && method == "POST") {
        string msg; bool ok = engine.undo(msg);
        return Json::obj(vector<string>{ Json::kvb("ok", ok), Json::kv("message", msg),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()) });
    }
    if (path == "/api/allocation/redo" && method == "POST") {
        string msg; bool ok = engine.redo(msg);
        return Json::obj(vector<string>{ Json::kvb("ok", ok), Json::kv("message", msg),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()) });
    }

    /* recommended seats for a manual reallocation */
    if (path.rfind("/api/recommendations/", 0) == 0) {
        string roll = path.substr(21);
        vector<AllocationEngine::SeatSuggestion> s = engine.recommend(roll, 3);
        vector<string> out;
        for (size_t i = 0; i < s.size(); i++) {
            vector<string> r;
            r.push_back(Json::kv("room", s[i].room)); r.push_back(Json::kvn("row", s[i].row));
            r.push_back(Json::kvn("col", s[i].col));
            r.push_back(Json::kv("seat", s[i].room + "-R" + to_string(s[i].row) + "-C" + to_string(s[i].col)));
            r.push_back(Json::kvn("quality", s[i].quality));
            r.push_back(Json::arrS(s[i].reasons));
            out.push_back(Json::obj(r));
        }
        return Json::obj(vector<string>{ Json::arr(out) });
    }

    /* replay steps */
    if (path == "/api/replay") {
        vector<string> out;
        for (size_t i = 0; i < engine.replaySteps.size(); i++) {
            ReplayStep s = engine.replaySteps[i];
            out.push_back(Json::obj(vector<string>{
                Json::kvn("index", s.index), Json::kv("roll", s.roll),
                Json::kv("name", s.name), Json::kv("room", s.room),
                Json::kvn("row", s.row), Json::kvn("col", s.col),
                Json::kvn("conflictsSoFar", s.conflictsSoFar) }));
        }
        return Json::obj(vector<string>{ Json::kvn("total", out.size()), Json::arr(out) });
    }

    /* ---------------- student login + seat pass ---------------- */
    if (path == "/api/student/search" && method == "POST") {
        int idx = SearchEngine::findStudentRecord(engine.students, payload["name"],
                     payload.count("section") ? payload["section"] : "", payload["roll"]);
        if (idx < 0) return Json::obj(vector<string>{ Json::kvb("found", false),
            Json::kv("message", "No student found with those details. Please check and try again.") });
        Student s = engine.students[idx];
        map<string, AllocationRecord>::iterator it = engine.seatOfRoll.find(s.roll);
        if (it == engine.seatOfRoll.end())
            return Json::obj(vector<string>{ Json::kvb("found", true), Json::kv("allocated", "false") });
        AllocationRecord r = it->second;
        vector<AllocationEngine::Neighbour> nb = engine.neighboursOf(s.roll);
        vector<string> nbJson;
        for (size_t i = 0; i < nb.size(); i++)
            nbJson.push_back(Json::obj(vector<string>{
                Json::kv("relation", nb[i].relation), Json::kv("roll", nb[i].roll),
                Json::kv("name", nb[i].name), Json::kv("subject", nb[i].subject),
                Json::kv("section", nb[i].section), Json::kv("seat", nb[i].seat),
                Json::kvb("samePaper", nb[i].samePaper) }));
        return Json::obj(vector<string>{
            Json::kvb("found", true), Json::kv("allocated", "true"),
            Json::kv("roll", s.roll), Json::kv("name", s.name),
            Json::kv("department", s.department), Json::kv("section", s.section),
            Json::kv("subject", s.subject), Json::kv("exam", s.exam),
            Json::kv("examDate", engine.examDate), Json::kv("room", r.room),
            Json::kvn("row", r.row), Json::kvn("col", r.col), Json::kv("seat", r.seat()),
            Json::kv("passId", "SS-" + s.roll + "-" + r.room),
            Json::arr(nbJson) });
    }

    if (path.rfind("/api/student/", 0) == 0) {
        string roll = path.substr(13);
        Student* s = engine.findStudent(roll);
        if (!s) return Json::obj(vector<string>{ Json::kvb("found", false) });
        map<string, AllocationRecord>::iterator it = engine.seatOfRoll.find(roll);
        string seat = it != engine.seatOfRoll.end() ? it->second.seat() : "";
        string room = it != engine.seatOfRoll.end() ? it->second.room : "";
        return Json::obj(vector<string>{ Json::kvb("found", true),
            Json::kv("roll", s->roll), Json::kv("name", s->name),
            Json::kv("subject", s->subject), Json::kv("section", s->section),
            Json::kv("seat", seat), Json::kv("room", room) });
    }

    /* ---------------- conflicts ---------------- */
    if (path == "/api/conflicts") {
        string sev = params.count("severity") ? params["severity"] : "ALL";
        string status = params.count("status") ? params["status"] : "ALL";
        vector<string> out;
        for (size_t i = 0; i < engine.conflicts.size(); i++) {
            if (sev != "ALL" && engine.conflicts[i].severity != sev) continue;
            if (status != "ALL" && engine.conflicts[i].status != status) continue;
            out.push_back(conflictJson(engine.conflicts[i]));
        }
        return Json::obj(vector<string>{
            Json::kvn("total", out.size()),
            Json::kvn("high", ConflictEngine::count(engine.conflicts, "HIGH")),
            Json::kvn("medium", ConflictEngine::count(engine.conflicts, "MEDIUM")),
            Json::kvn("low", ConflictEngine::count(engine.conflicts, "LOW")),
            Json::kvn("resolved", (int)engine.conflicts.size() - ConflictEngine::open(engine.conflicts)),
            Json::arr(out) });
    }

    if (path == "/api/conflicts/resolve" && method == "POST") {
        int id = atoi(payload["id"].c_str());
        bool ok = false;
        for (size_t i = 0; i < engine.conflicts.size(); i++)
            if (engine.conflicts[i].id == id) { engine.conflicts[i].status = "RESOLVED"; ok = true; break; }
        return Json::obj(vector<string>{ Json::kvb("ok", ok),
            Json::kv("message", ok ? "Conflict marked as resolved" : "Conflict not found") });
    }

    /* ---------------- analytics ---------------- */
    if (path == "/api/analytics") {
        vector<string> subj, dept, sect;
        map<string, int> m;
        m = AnalyticsEngine::countBy(engine.students, "subject");
        for (map<string, int>::iterator it = m.begin(); it != m.end(); ++it)
            subj.push_back(Json::obj(vector<string>{ Json::kv("label", it->first), Json::kvn("value", it->second) }));
        m = AnalyticsEngine::countBy(engine.students, "department");
        for (map<string, int>::iterator it = m.begin(); it != m.end(); ++it)
            dept.push_back(Json::obj(vector<string>{ Json::kv("label", it->first), Json::kvn("value", it->second) }));
        m = AnalyticsEngine::countBy(engine.students, "section");
        for (map<string, int>::iterator it = m.begin(); it != m.end(); ++it)
            sect.push_back(Json::obj(vector<string>{ Json::kv("label", it->first), Json::kvn("value", it->second) }));

        vector<AnalyticsEngine::RoomStat> rs = AnalyticsEngine::roomStats(engine);
        vector<string> roomsOut;
        for (size_t i = 0; i < rs.size(); i++) {
            char buf[32]; sprintf(buf, "%.1f", rs[i].utilization);
            roomsOut.push_back(Json::obj(vector<string>{
                Json::kv("name", rs[i].name), Json::kvn("capacity", rs[i].capacity),
                Json::kvn("occupied", rs[i].occupied), Json::kvn("vacant", rs[i].vacant),
                Json::kvn("blocked", rs[i].blocked), Json::kv("utilization", buf),
                Json::kvn("conflicts", rs[i].conflicts) }));
        }
        map<string, int> cps = AnalyticsEngine::conflictsPerSubject(engine);
        vector<string> cSubj;
        for (map<string, int>::iterator it = cps.begin(); it != cps.end(); ++it)
            cSubj.push_back(Json::obj(vector<string>{ Json::kv("label", it->first), Json::kvn("value", it->second) }));

        vector<string> factors;
        for (size_t i = 0; i < engine.qualityFactors.size(); i++)
            factors.push_back(Json::obj(vector<string>{
                Json::kv("label", engine.qualityFactors[i].label),
                Json::kvn("points", engine.qualityFactors[i].points),
                Json::kv("detail", engine.qualityFactors[i].detail) }));

        return Json::obj(vector<string>{
            Json::kvn("students", engine.students.size()),
            Json::kvn("quality", engine.qualityScore),
            Json::kvn("conflicts", engine.conflicts.size()),
            Json::kvn("reallocationCount", engine.reallocationCount),
            Json::kv("subjects", Json::arr(subj)), Json::kv("departments", Json::arr(dept)),
            Json::kv("sections", Json::arr(sect)), Json::kv("rooms", Json::arr(roomsOut)),
            Json::kv("conflictsPerSubject", Json::arr(cSubj)),
            Json::kv("qualityFactors", Json::arr(factors)) });
    }

    /* ---------------- history (linked list) ---------------- */
    if (path == "/api/history") {
        vector<Move> moves = params.count("roll")
            ? engine.historyOf(params["roll"]) : engine.allHistory();
        vector<string> out;
        for (size_t i = 0; i < moves.size(); i++) out.push_back(moveJson(moves[i]));
        vector<string> undo, redo;
        vector<Action> ua = engine.undoStack.toVector(), ra = engine.redoStack.toVector();
        for (size_t i = 0; i < ua.size(); i++) undo.push_back(Json::obj(vector<string>{
            Json::kv("type", ua[i].type), Json::kv("roll", ua[i].roll), Json::kv("note", ua[i].note) }));
        for (size_t i = 0; i < ra.size(); i++) redo.push_back(Json::obj(vector<string>{
            Json::kv("type", ra[i].type), Json::kv("roll", ra[i].roll), Json::kv("note", ra[i].note) }));
        return Json::obj(vector<string>{ Json::kvn("total", out.size()),
            Json::arr(out), Json::arr(undo), Json::arr(redo),
            Json::kvn("undoDepth", engine.undoStack.size()),
            Json::kvn("redoDepth", engine.redoStack.size()) });
    }

    return Json::obj(vector<string>{ Json::kv("error", "Endpoint not found"),
                                     Json::kv("path", path) });
}

/* ==========================================================================
   4. HTTP SERVER
   ========================================================================== */
static string httpHeader(int code, const string& type, size_t len) {
    string status = code == 200 ? "200 OK" : "404 Not Found";
    stringstream h;
    h << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << type << "\r\n"
      << "Access-Control-Allow-Origin: *\r\n"
      << "Access-Control-Allow-Headers: Content-Type\r\n"
      << "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
      << "Cache-Control: no-store\r\n"
      << "Content-Length: " << len << "\r\n"
      << "Connection: close\r\n\r\n";
    return h.str();
}

static void handleClient(int client) {
    char buffer[8192] = { 0 };
    int n = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) { closesocket(client); return; }

    string request(buffer, n);
    istringstream rs(request);
    string method, path, version;
    rs >> method >> path >> version;

    if (method == "OPTIONS") {                       /* CORS pre-flight */
        string r = httpHeader(200, "text/plain", 0);
        send(client, r.c_str(), (int)r.size(), 0);
        closesocket(client); return;
    }
    size_t bodyAt = request.find("\r\n\r\n");
    string body = bodyAt == string::npos ? "" : request.substr(bodyAt + 4);

    string response = route(method, path, body);
    string head = httpHeader(200, "application/json; charset=utf-8", response.size());
    send(client, head.c_str(), (int)head.size(), 0);
    send(client, response.c_str(), (int)response.size(), 0);

    cout << "[SMARTSEAT] " << method << " " << path << "\n";
    closesocket(client);
}

int main() {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    int port = 8080;
    buildSampleData(engine);                         /* demo dataset on startup */

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) { cout << "Could not create socket.\n"; return 1; }
    int yes = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Port " << port << " is busy — change PORT in main.cpp.\n"; return 1;
    }
    listen(serverFd, 16);

    cout << "=======================================================\n";
    cout << "   SMARTSEAT — Intelligent Examination Seating\n";
    cout << "   C++ engine running on http://localhost:" << port << "\n";
    cout << "   " << engine.students.size() << " students · " << engine.rooms.size()
         << " rooms · quality " << engine.qualityScore << "/100\n";
    cout << "=======================================================\n";

    while (true) {
        int client = accept(serverFd, 0, 0);
        if (client < 0) continue;
        handleClient(client);                        /* one request at a time */
    }
    return 0;
}
