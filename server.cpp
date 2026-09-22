/* ============================================================================
   server.cpp — OPTIONAL C++ BACKEND FOR THE WORD GUESSING GAME (HANGMAN)
   ----------------------------------------------------------------------------
   Why this file exists
   --------------------
   Browsers cannot start a program, so a plain HTML page cannot call game.cpp
   directly. This tiny HTTP server (written in C++, using POSIX sockets) gives
   the HTML/CSS frontend a real C++ backend to talk to:

        browser  --POST /api/command-->  server.cpp  -->  game.cpp
                                                         handleCommand()
        browser  <--state block----------  server.cpp  <--  game.cpp

   The whole game logic is reused (not rewritten): we simply include game.cpp
   with HANGMAN_LIBRARY_ONLY so that its main() is not compiled twice.

   Build & run
   -----------
        g++ -std=c++11 server.cpp -o hangman-server -pthread
        ./hangman-server
        → open http://localhost:8080

   What it serves
   --------------
        GET  /                index.html
        GET  /style.css       style.css
        GET  /api/health      {"engine":"C++"}   (used by the page to detect it)
        POST /api/command     NEW|cat|diff|seed | GUESS|C | HINT | TIMEUP

   The session lives inside the C++ process, so the secret word is sent to the
   browser ONLY when the round is finished (WIN / LOSE) — unlike the stateless
   WebAssembly build, the player cannot peek at the answer in the page source.
   ========================================================================== */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>

#define HANGMAN_LIBRARY_ONLY        /* do not compile game.cpp's main() */
#include "game.cpp"                 /* ← all the Hangman logic comes from here */

using namespace std;

/* --------------------------------------------------------------------------
   The game session (kept in the C++ process)
   -------------------------------------------------------------------------- */
struct Session {
    string       word    = "";
    vector<char> guessed;
    int          wrong   = 0;
    int          max     = 6;
    int          score   = 0;
    int          diff    = 2;
    int          cat     = 0;
    bool         started = false;
};
Session session;

/* ---- reads one KEY|VALUE entry out of an engine reply ------------------ */
string field(const string& reply, const string& key) {
    string find = key + "|";
    size_t p = reply.find(find);
    if (p == string::npos) return "";
    size_t start = p + find.length();
    size_t end   = reply.find('\n', start);
    return reply.substr(start, end - start);
}

/* ---- copies the new state of the engine back into our session --------- */
void syncSession(const string& reply) {
    string w = field(reply, "WORD");
    if (w != "")                session.word    = w;
    string g = field(reply, "GUESSED");
    if (g != "")                session.guessed = parseLetters(g);
    session.wrong = atoi(field(reply, "WRONG").c_str());
    session.max   = atoi(field(reply, "MAX").c_str());
    session.score = atoi(field(reply, "SCORE").c_str());
    session.diff  = atoi(field(reply, "DIFF").c_str());
}

/* ---- turns the short browser command into a full protocol command ----- */
string expand(const string& shortCmd) {
    vector<string> p = split(shortCmd, '|');

    if (p[0] == "NEW") {
        session = Session();                       /* fresh round */
        session.cat  = atoi(p[1].c_str());
        session.diff = atoi(p[2].c_str());
        return shortCmd;                            /* NEW needs nothing else */
    }
    if (!session.started && session.word != "") session.started = true;

    /* the short commands are   GUESS|C   |   HINT|HINT   |   TIMEUP */
    string action = (p.size() > 1) ? p[1] : "TIMEUP";
    string cmd    = (action == "HINT") ? "HINT" : "GUESS";

    string letters = "";
    for (size_t i = 0; i < session.guessed.size(); i++) {
        if (i) letters += ",";
        letters += string(1, session.guessed[i]);
    }
    cmd += "|" + session.word + "|" + letters
        +  "|" + to_string(session.wrong) + "|" + to_string(session.max)
        +  "|" + action
        +  "|" + to_string(session.score) + "|" + to_string(session.diff)
        +  "|" + to_string(session.cat) + "|0";
    return cmd;
}

/* ---- removes the WORD| line while the round is still running ---------- */
string hideSecretWord(const string& reply) {
    string status = field(reply, "STATUS");
    if (status == "WON" || status == "LOST") return reply;   /* game over */
    string out = "", line;
    stringstream ss(reply);
    while (getline(ss, line)) {
        if (line.rfind("WORD|", 0) == 0) continue;           /* hide it */
        out += line + "\n";
    }
    return out;
}

/* --------------------------------------------------------------------------
   Small HTTP helpers
   -------------------------------------------------------------------------- */
string fileContent(const string& path) {
    ifstream f(path.c_str());
    if (!f) return "";
    stringstream ss; ss << f.rdbuf();
    return ss.str();
}

string httpReply(int code, const string& type, const string& body) {
    string status = (code == 200) ? "200 OK" : "404 Not Found";
    string head = "HTTP/1.1 " + status + "\r\n"
                  "Content-Type: " + type + "\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Cache-Control: no-store\r\n"
                  "Content-Length: " + to_string(body.size()) + "\r\n"
                  "Connection: close\r\n\r\n";
    return head + body;
}

/* ---- answers one browser connection ----------------------------------- */
void handleClient(int client) {
    char buffer[4096] = {0};
    int n = read(client, buffer, sizeof(buffer) - 1);
    if (n <= 0) { close(client); return; }

    string request(buffer, n);
    istringstream rs(request);
    string method, path;
    rs >> method >> path;

    /* ---- POST /api/command : talk to the C++ game engine ---- */
    if (method == "POST" && path.rfind("/api/command", 0) == 0) {
        size_t p = request.find("\r\n\r\n");
        string body = (p == string::npos) ? "" : request.substr(p + 4);

        string cmd    = expand(body);
        string reply  = handleCommand(cmd);        /* ← game.cpp does the work */
        syncSession(reply);
        reply = hideSecretWord(reply);

        cout << "[C++] " << body << "  ->  " << field(reply, "EVENT") << "\n";
        string response = httpReply(200, "text/plain; charset=utf-8", reply);
        send(client, response.c_str(), (int)response.size(), 0);
        close(client);
        return;
    }

    /* ---- GET /api/health : lets index.html detect the C++ backend ---- */
    if (path == "/api/health") {
        string r = httpReply(200, "application/json", "{\"engine\":\"C++\"}");
        send(client, r.c_str(), (int)r.size(), 0);
        close(client);
        return;
    }

    /* ---- static files ---- */
    if (path == "/" || path == "/index.html") {
        string html = fileContent("index.html");
        string r = httpReply(200, "text/html; charset=utf-8", html);
        send(client, r.c_str(), (int)r.size(), 0);
    } else if (path == "/style.css") {
        string css = fileContent("style.css");
        string r = httpReply(200, "text/css; charset=utf-8", css);
        send(client, r.c_str(), (int)r.size(), 0);
    } else if (path == "/game.cpp") {
        string cpp = fileContent("game.cpp");
        string r = httpReply(200, "text/plain; charset=utf-8", cpp);
        send(client, r.c_str(), (int)r.size(), 0);
    } else {
        string r = httpReply(404, "text/plain", "404 - not found");
        send(client, r.c_str(), (int)r.size(), 0);
    }
    close(client);
}

/* --------------------------------------------------------------------------
   main — a very small threaded TCP server on port 8080
   -------------------------------------------------------------------------- */
int main() {
    int port = 8080;

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { cout << "Could not create socket.\n"; return 1; }

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Port " << port << " is busy. Try another one.\n"; return 1;
    }
    listen(server, 10);

    cout << "=====================================================\n";
    cout << "  WORD GUESSING GAME — C++ backend running\n";
    cout << "  Open  http://localhost:" << port << "  in your browser\n";
    cout << "=====================================================\n";

    while (true) {
        int client = accept(server, NULL, NULL);
        if (client < 0) continue;
        thread(handleClient, client).detach();      /* one thread per request */
    }
    return 0;
}
