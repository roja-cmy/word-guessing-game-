/* ============================================================================
   backend/structures/Structures.h — SMARTSEAT syllabus data structures
   ----------------------------------------------------------------------------
   Consolidates the structure layer in one header:
        structures/StudentList.h      -> class StudentList     (LINKED LIST)
        structures/AllocationQueue.h  -> class AllocationQueue (QUEUE)
        structures/HistoryStack.h     -> class HistoryStack    (STACK)
   All three are hand written with nodes so the syllabus concepts are actually
   demonstrated (no std::list / std::queue / std::stack shortcuts).
   ========================================================================== */
#ifndef SMARTSEAT_STRUCTURES_H
#define SMARTSEAT_STRUCTURES_H

#include <string>
#include <vector>
#include "models/Models.h"

/* --------------------------------------------------------------------------
   LINKED LIST — student movement / allocation history
   Each node holds one Move (from seat -> to seat). New events are appended at
   the tail, so the list reads chronologically like a timeline.
   -------------------------------------------------------------------------- */
class StudentList {
private:
    struct Node {
        Move data;
        Node* next;
        Node(const Move& m) : data(m), next(0) {}
    };
    Node* head;
    Node* tail;
    int count;

public:
    StudentList() : head(0), tail(0), count(0) {}
    ~StudentList() { clear(); }

    void append(const Move& m) {
        Node* n = new Node(m);
        if (tail == 0) head = tail = n;
        else { tail->next = n; tail = n; }
        count++;
    }
    int size() const { return count; }
    bool empty() const { return count == 0; }

    /* walk the list and copy the moves into a vector for the UI timeline */
    std::vector<Move> toVector() const {
        std::vector<Move> out;
        for (Node* p = head; p != 0; p = p->next) out.push_back(p->data);
        return out;
    }
    /* last node without removing it (used to show the "current" seat) */
    bool last(Move& out) const {
        if (tail == 0) return false;
        out = tail->data;
        return true;
    }
    void clear() {
        Node* p = head;
        while (p != 0) { Node* n = p->next; delete p; p = n; }
        head = tail = 0; count = 0;
    }
};

/* --------------------------------------------------------------------------
   QUEUE — emergency reallocation waiting list
   Affected students join at the back and are served from the front (FIFO),
   exactly like a real waiting line outside the exam hall.
   -------------------------------------------------------------------------- */
class AllocationQueue {
private:
    struct QNode {
        Student data;
        QNode* next;
        QNode(const Student& s) : data(s), next(0) {}
    };
    QNode* frontNode;
    QNode* rearNode;
    int count;

public:
    AllocationQueue() : frontNode(0), rearNode(0), count(0) {}
    ~AllocationQueue() { clear(); }

    void enqueue(const Student& s) {
        QNode* n = new QNode(s);
        if (rearNode == 0) frontNode = rearNode = n;
        else { rearNode->next = n; rearNode = n; }
        count++;
    }
    bool dequeue(Student& out) {
        if (frontNode == 0) return false;
        QNode* n = frontNode;
        out = n->data;
        frontNode = n->next;
        if (frontNode == 0) rearNode = 0;
        delete n;
        count--;
        return true;
    }
    bool peek(Student& out) const {
        if (frontNode == 0) return false;
        out = frontNode->data;
        return true;
    }
    int size() const { return count; }
    bool empty() const { return count == 0; }
    std::vector<Student> toVector() const {
        std::vector<Student> out;
        for (QNode* p = frontNode; p != 0; p = p->next) out.push_back(p->data);
        return out;
    }
    void clear() {
        while (frontNode != 0) { QNode* n = frontNode->next; delete frontNode; frontNode = n; }
        rearNode = 0; count = 0;
    }
};

/* --------------------------------------------------------------------------
   STACK — undo / redo of every allocation modification
   push() on every action, pop() on undo. A second stack keeps undone actions
   for redo.
   -------------------------------------------------------------------------- */
class HistoryStack {
private:
    struct SNode {
        Action data;
        SNode* next;
        SNode(const Action& a) : data(a), next(0) {}
    };
    SNode* topNode;
    int count;

public:
    HistoryStack() : topNode(0), count(0) {}
    ~HistoryStack() { clear(); }

    void push(const Action& a) {
        SNode* n = new SNode(a);
        n->next = topNode;
        topNode = n;
        count++;
    }
    bool pop(Action& out) {
        if (topNode == 0) return false;
        SNode* n = topNode;
        out = n->data;
        topNode = n->next;
        delete n;
        count--;
        return true;
    }
    bool peek(Action& out) const {
        if (topNode == 0) return false;
        out = topNode->data;
        return true;
    }
    int size() const { return count; }
    bool empty() const { return count == 0; }
    std::vector<Action> toVector() const {
        std::vector<Action> out;
        for (SNode* p = topNode; p != 0; p = p->next) out.push_back(p->data);
        return out;                       /* newest first */
    }
    void clear() {
        while (topNode != 0) { SNode* n = topNode->next; delete topNode; topNode = n; }
        count = 0;
    }
};

#endif /* SMARTSEAT_STRUCTURES_H */
