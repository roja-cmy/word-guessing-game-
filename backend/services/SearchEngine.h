/* ============================================================================
   backend/services/SearchEngine.h — searching, pattern matching, analytics
   ----------------------------------------------------------------------------
   Consolidates:
        services/SearchEngine.h     -> class SearchEngine
        services/AnalyticsEngine.h  -> class AnalyticsEngine

   Syllabus concepts demonstrated here:
     * Searching      – linear search and binary search (on a roll-sorted copy)
     * Pattern matching – a hand written case-insensitive substring matcher used
                        for partial name / roll number / subject searches
     * Containers     – std::vector and std::map for every collection
   ========================================================================== */
#ifndef SMARTSEAT_SEARCH_ENGINE_H
#define SMARTSEAT_SEARCH_ENGINE_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "models/Models.h"
#include "services/AllocationEngine.h"

class SearchEngine {
public:
    /* ---------------------- string helpers ---------------------- */
    static std::string toUpper(std::string s) {
        for (size_t i = 0; i < s.size(); i++)
            if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 32);
        return s;
    }
    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
        return s.substr(a, b - a);
    }

    /* =====================================================================
       SEARCHING — 1. linear search by exact roll number
       ===================================================================== */
    static int linearSearch(const std::vector<Student>& list, const std::string& roll) {
        for (size_t i = 0; i < list.size(); i++)
            if (list[i].roll == roll) return (int)i;
        return -1;
    }

    /* 2. binary search on a copy sorted by roll number */
    static std::vector<Student> sortByRoll(std::vector<Student> list) {
        std::sort(list.begin(), list.end(), byRoll);
        return list;
    }
    static bool byRoll(const Student& a, const Student& b) { return a.roll < b.roll; }

    static int binarySearch(const std::vector<Student>& sorted, const std::string& roll) {
        int low = 0, high = (int)sorted.size() - 1;
        while (low <= high) {
            int mid = (low + high) / 2;
            if (sorted[mid].roll == roll) return mid;
            if (sorted[mid].roll < roll) low = mid + 1;
            else high = mid - 1;
        }
        return -1;
    }

    /* =====================================================================
       PATTERN MATCHING — naive case-insensitive substring search
       "han" matches Hansika, Haneesha, Hanish …
       ===================================================================== */
    static bool containsPattern(const std::string& text, const std::string& pattern) {
        std::string t = toUpper(text), p = toUpper(pattern);
        if (p == "") return true;
        if (p.size() > t.size()) return false;
        for (size_t i = 0; i + p.size() <= t.size(); i++) {
            size_t j = 0;
            while (j < p.size() && t[i + j] == p[j]) j++;
            if (j == p.size()) return true;
        }
        return false;
    }

    /* search one field ("name" / "roll" / "subject" / "section") */
    static std::vector<Student> search(const std::vector<Student>& list,
                                       const std::string& query,
                                       const std::string& field) {
        std::vector<Student> out;
        for (size_t i = 0; i < list.size(); i++) {
            std::string value = field == "roll" ? list[i].roll
                             : field == "subject" ? list[i].subject
                             : field == "section" ? list[i].section
                             : field == "department" ? list[i].department
                             : list[i].name;
            if (containsPattern(value, query)) out.push_back(list[i]);
        }
        return out;
    }

    /* student login: name + section + roll must all match */
    static int findStudentRecord(const std::vector<Student>& list,
                                 const std::string& name,
                                 const std::string& section,
                                 const std::string& roll) {
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].roll != roll) continue;
            if (!containsPattern(list[i].name, name)) continue;
            if (section != "" && list[i].section != section) continue;
            return (int)i;
        }
        return -1;
    }
};

/* -------------------------------------------------------------------------- */
/*  ANALYTICS ENGINE — every number shown on the dashboard / charts           */
/* -------------------------------------------------------------------------- */
class AnalyticsEngine {
public:
    struct RoomStat {
        std::string name; int capacity, occupied, vacant, blocked, conflicts;
        double utilization;
    };

    static std::map<std::string, int> countBy(const std::vector<Student>& list,
                                              const std::string& field) {
        std::map<std::string, int> out;
        for (size_t i = 0; i < list.size(); i++) {
            std::string key = field == "subject" ? list[i].subject
                            : field == "department" ? list[i].department
                            : field == "section" ? list[i].section
                            : list[i].className;
            out[key] = out[key] + 1;               /* std::map container */
        }
        return out;
    }

    static std::vector<RoomStat> roomStats(AllocationEngine& e) {
        std::vector<RoomStat> out;
        for (size_t i = 0; i < e.rooms.size(); i++) {
            RoomStat s;
            s.name = e.rooms[i].name;
            s.capacity = e.rooms[i].capacity();
            s.occupied = e.rooms[i].occupied();
            s.blocked = e.rooms[i].blockedSeats();
            s.vacant = e.rooms[i].vacant();
            s.utilization = e.rooms[i].utilization();
            s.conflicts = 0;
            out.push_back(s);
        }
        for (size_t i = 0; i < e.conflicts.size(); i++) {
            for (size_t j = 0; j < out.size(); j++)
                if (out[j].name == e.conflicts[i].room) { out[j].conflicts++; break; }
        }
        return out;
    }

    static std::map<std::string, int> conflictsPerSubject(AllocationEngine& e) {
        std::map<std::string, int> out;
        for (size_t i = 0; i < e.conflicts.size(); i++)
            out[e.conflicts[i].subject] = out[e.conflicts[i].subject] + 1;
        return out;
    }

    static std::map<std::string, int> studentsPerRoom(AllocationEngine& e) {
        std::map<std::string, int> out;
        for (size_t i = 0; i < e.rooms.size(); i++) out[e.rooms[i].name] = e.rooms[i].occupied();
        return out;
    }
};

#endif /* SMARTSEAT_SEARCH_ENGINE_H */
