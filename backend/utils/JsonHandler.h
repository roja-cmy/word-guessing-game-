/* ============================================================================
   backend/utils/JsonHandler.h — tiny JSON writer / reader for SMARTSEAT
   ----------------------------------------------------------------------------
   Consolidates:
        utils/JsonHandler.h -> class Json (writing) + JsonParser (reading)
        utils/FileParser.h  -> parseUploadedRows() for the Excel/CSV upload

   The frontend sends flat JSON objects and arrays of flat objects, which is
   all this project needs, so the reader is deliberately small and easy to
   explain in a viva (no external JSON library required).
   ========================================================================== */
#ifndef SMARTSEAT_JSON_HANDLER_H
#define SMARTSEAT_JSON_HANDLER_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "models/Models.h"
#include "services/SearchEngine.h"

/* -------------------------------------------------------------------------- */
/*  WRITER                                                                     */
/* -------------------------------------------------------------------------- */
class Json {
public:
    static std::string esc(const std::string& s) {
        std::string out = "";
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "";
            else if (c == '\t') out += "\\t";
            else if ((unsigned char)c < 32) out += " ";
            else out += c;
        }
        return out;
    }
    static std::string str(const std::string& s) { return "\"" + esc(s) + "\""; }
    /* key/value helpers used to compose responses */
    static std::string kv(const std::string& k, const std::string& v) {
        return str(k) + ":" + str(v);
    }
    static std::string kvn(const std::string& k, long long v) {
        return str(k) + ":" + std::to_string(v);
    }
    static std::string kvb(const std::string& k, bool v) {
        return str(k) + ":" + (v ? "true" : "false");
    }
    static std::string obj(const std::vector<std::string>& parts) {
        std::string s = "{";
        for (size_t i = 0; i < parts.size(); i++) { if (i) s += ","; s += parts[i]; }
        return s + "}";
    }
    static std::string arr(const std::vector<std::string>& parts) {
        std::string s = "[";
        for (size_t i = 0; i < parts.size(); i++) { if (i) s += ","; s += parts[i]; }
        return s + "]";
    }
    /* an array of strings */
    static std::string arrS(const std::vector<std::string>& parts) {
        std::string s = "[";
        for (size_t i = 0; i < parts.size(); i++) { if (i) s += ","; s += str(parts[i]); }
        return s + "]";
    }
};

/* -------------------------------------------------------------------------- */
/*  READER — parses a flat JSON object or an array of flat objects            */
/* -------------------------------------------------------------------------- */
class JsonParser {
private:
    std::string src;
    size_t pos;

    void skipSpace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\n' ||
               src[pos] == '\r' || src[pos] == '\t')) pos++;
    }
    std::string readString() {
        std::string out = "";
        if (pos >= src.size() || src[pos] != '"') { pos++; return out; }
        pos++;
        while (pos < src.size() && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < src.size()) {
                pos++;
                if (src[pos] == 'n') out += "\n";
                else if (src[pos] == 't') out += "\t";
                else out += src[pos];
            } else out += src[pos];
            pos++;
        }
        pos++;                                   /* closing quote */
        return out;
    }
    std::string readValue() {
        skipSpace();
        if (pos < src.size() && src[pos] == '"') return readString();
        std::string out = "";
        while (pos < src.size() && src[pos] != ',' && src[pos] != '}' && src[pos] != ']')
            out += src[pos++];
        return SearchEngine::trim(out);
    }
    /* reads one flat object into pairs */
    void readObject(std::map<std::string, std::string>& out) {
        skipSpace();
        if (pos < src.size() && src[pos] == '{') pos++;
        while (pos < src.size()) {
            skipSpace();
            if (src[pos] == '}' || src[pos] == ']') { pos++; return; }
            std::string key = readString();
            skipSpace();
            if (pos < src.size() && src[pos] == ':') pos++;
            out[key] = readValue();
            skipSpace();
            if (pos < src.size() && src[pos] == ',') pos++;
        }
    }

public:
    JsonParser(const std::string& json) : src(json), pos(0) {}

    /* reads { "k":"v", ... } */
    bool object(std::map<std::string, std::string>& out) {
        skipSpace();
        if (pos >= src.size() || src[pos] != '{') return false;
        readObject(out);
        return true;
    }
    /* reads [ {...}, {...} ] */
    bool objectArray(std::vector<std::map<std::string, std::string> >& out) {
        skipSpace();
        if (pos >= src.size() || src[pos] != '[') return false;
        pos++;
        while (pos < src.size()) {
            skipSpace();
            if (pos < src.size() && src[pos] == ']') return true;
            std::map<std::string, std::string> one;
            readObject(one);
            if (!one.empty()) out.push_back(one);
            skipSpace();
            if (pos < src.size() && src[pos] == ',') pos++;
        }
        return out.size() > 0;
    }
    std::string get(const std::map<std::string, std::string>& m,
                    const std::string& key, const std::string& fallback = "") {
        std::map<std::string, std::string>::const_iterator it = m.find(key);
        return it == m.end() ? fallback : it->second;
    }
};

/* -------------------------------------------------------------------------- */
/*  FILE PARSER — turns uploaded rows into Student records + a validation      */
/*  report (duplicate roll numbers, missing fields, invalid records)           */
/* -------------------------------------------------------------------------- */
struct UploadResult {
    std::vector<Student> students;
    UploadReport report;
    std::vector<Student> duplicates;
    std::vector<Student> invalid;
};

inline UploadResult parseUploadedRows(const std::vector<std::map<std::string, std::string> >& rows) {
    UploadResult res;
    res.report.total = (int)rows.size();

    /* field names accepted from the Excel sheet (case/space insensitive) */
    for (size_t i = 0; i < rows.size(); i++) {
        const std::map<std::string, std::string>& row = rows[i];
        std::map<std::string, std::string> norm;
        for (std::map<std::string, std::string>::const_iterator it = row.begin();
             it != row.end(); ++it) {
            std::string k = SearchEngine::toUpper(it->first);
            while (k.size() && (k[0] == ' ' || k[0] == '_')) k = k.substr(1);
            /* remove every space and underscore so "Roll No" == "ROLLNO" */
            std::string clean = "";
            for (size_t q = 0; q < k.size(); q++)
                if (k[q] != ' ' && k[q] != '_' && k[q] != '.') clean += k[q];
            norm[clean] = SearchEngine::trim(it->second);
        }
        Student s;
        s.roll      = norm.count("ROLLNUMBER") ? norm["ROLLNUMBER"] : norm["ROLL"];
        s.name      = norm.count("NAME") ? norm["NAME"] : norm["STUDENTNAME"];
        s.department= norm.count("DEPARTMENT") ? norm["DEPARTMENT"] : norm["DEPT"];
        s.className = norm.count("CLASS") ? norm["CLASS"] : norm["YEAR"];
        s.section   = norm.count("SECTION") ? norm["SECTION"] : "";
        s.subject   = norm.count("SUBJECT") ? norm["SUBJECT"] : norm["PAPER"];
        s.exam      = norm.count("EXAM") ? norm["EXAM"] : "";

        if (!s.isValid()) {
            res.report.invalid++;
            res.invalid.push_back(s);
            res.report.messages.push_back("Row " + std::to_string(i + 2) + ": missing " +
                                          (s.missing() == "" ? "required fields" : s.missing()));
            continue;
        }
        /* duplicate roll number detection */
        bool dup = false;
        for (size_t j = 0; j < res.students.size(); j++)
            if (res.students[j].roll == s.roll) { dup = true; break; }
        if (dup) {
            res.report.duplicates++;
            res.duplicates.push_back(s);
            res.report.messages.push_back("Row " + std::to_string(i + 2) + ": duplicate roll number " + s.roll);
            continue;
        }
        res.students.push_back(s);
        res.report.accepted++;
    }
    return res;
}

#endif /* SMARTSEAT_JSON_HANDLER_H */
