// =============================================================================
// RAII & Smart Pointers — Stage 4: The Reference Cycle
// cycle.cpp
//
// This file contains a deliberately broken program. Compile and run it.
// You will notice that the destructors for Order and Trader never fire —
// the log goes silent at program exit. That is a memory leak in a language
// that is supposed to "not leak."
//
// Your task:
//   1. Run the program and read the output. Note the missing "destroyed" lines.
//   2. Understand WHY the cycle prevents destruction (draw the ref-count graph).
//   3. Fix it by making ONE of the two shared_ptr members a weak_ptr instead.
//   4. Re-run. Every "born" line should now be matched by a "destroyed" line.
//
// Hint: ask yourself which direction of the link represents "I keep this alive"
// and which represents "I just want to observe it." Make the observer weak.
//
// Compile with:
//   g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -g cycle.cpp -o cycle
// =============================================================================

#include <iostream>
#include <memory>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// A simpler self-contained cycle that is easier to reason about.
// This version does NOT use enable_shared_from_this — just two structs
// that each hold a shared_ptr to the other.
// ─────────────────────────────────────────────────────────────────────────────
struct Node {
    int id_;
    std::shared_ptr<Node> next_; 

    explicit Node(int id) : id_(id) {
        std::cerr << "[Node #" << id_ << "] born\n";
    }

    ~Node() {
        std::cerr << "[Node #" << id_ << "] destroyed\n";
    }
};

int main() {
    std::cerr << "=== Stage 4: reference cycle demo ===\n\n";

    // ── Simple two-node cycle ──────────────────────────────────────────────
    std::cerr << "-- two-node cycle --\n";
    {
        auto a = std::make_shared<Node>(1);
        auto b = std::make_shared<Node>(2);

        a->next_ = b; 
        b->next_ = a; 

        std::cerr << "a.use_count=" << a.use_count()
                  << "  b.use_count=" << b.use_count() << "\n";
    }
    // Expected with the cycle:    neither "destroyed" line prints.
    // Expected after your fix:    both "destroyed" lines print here.

    std::cerr << "\n-- end of scope reached --\n";

    // ── What you should see after fixing ──────────────────────────────────
    // [Node #1] born
    // [Node #2] born
    // a.use_count=2  b.use_count=2
    // [Node #2] destroyed      ← only after fix
    // [Node #1] destroyed      ← only after fix
    // -- end of scope reached --

    return 0;
}

// Forward declaration needed because Order references Trader and vice versa.
struct Trader;

struct Order {
    int id_;
    std::string symbol_;
    std::shared_ptr<Trader> owner_;

    Order(int id, std::string sym)
        : id_(id), symbol_(std::move(sym))
    {
        std::cerr << "[Order  #" << id_ << " \"" << symbol_ << "\"] born\n";
    }

    ~Order() {
        std::cerr << "[Order  #" << id_ << " \"" << symbol_ << "\"] destroyed\n";
    }
};

struct Trader {
    int id_;
    std::string name_;
    std::shared_ptr<Order> active_order_;  // Trader owns its current Order

    Trader(int id, std::string name)
        : id_(id), name_(std::move(name))
    {
        std::cerr << "[Trader #" << id_ << " \"" << name_ << "\"] born\n";
    }

    ~Trader() {
        std::cerr << "[Trader #" << id_ << " \"" << name_ << "\"] destroyed\n";
    }

    void place_order(std::shared_ptr<Order> o) {
        active_order_ = o;
        o->owner_ = std::shared_ptr<Trader>(
            active_order_->owner_ // ?
        );
        std::cerr << "[Trader #" << id_ << "] placed order #" << o->id_ << "\n";
    }
};

// =============================================================================
// FIX GUIDE (read only after attempting it yourself)
//
// Change ONE member in Node:
//
//   std::weak_ptr<Node> next_;    // was shared_ptr<Node>
//
// Then to use it safely:
//
//   if (auto n = a->next_.lock()) {
//       std::cerr << "next is node #" << n->id_ << "\n";
//   } else {
//       std::cerr << "next is gone\n";
//   }
//
// Why this direction? Because "next_" is an observer — it doesn't imply
// "I keep the next node alive." The local variables a and b are the owners.
// weak_ptr models "I can see it, but I don't hold it."
// =============================================================================

