// =============================================================================
// RAII & Smart Pointers — Stage 5: The Leak Hunt
// leak_hunt.cpp
//
// This file contains THREE bugs. Your job:
//
//   Step 1 — Read the code and predict what will go wrong for each bug
//             BEFORE compiling. Write your predictions as comments.
//
//   Step 2 — Compile and run. Compare the cerr log against the
//             EXPECTED OUTPUT block at the bottom of this file.
//             Notice which "destroyed" lines are missing or wrong.
//
//   Step 3 — Compile with AddressSanitizer (see below). Each bug produces
//             a distinct ASan report — match each report to a bug.
//
//   Step 4 — Fix all three bugs using only RAII / smart-pointer tools.
//             No raw new/delete in your fix.
//
// Compile (buggy):
//   g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -g leak_hunt.cpp -o leak_hunt
//
// The three bug types:
//   Bug A — raw new with no delete on an exception path
//   Bug B — shared_ptr reference cycle (same as Stage 4)
//   Bug C — unique_ptr passed by value when the caller doesn't need ownership
// =============================================================================

#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <atomic>

static int next_id() {
    static std::atomic<int> counter{0};
    return ++counter;
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource: a simple named heap-allocated value that logs its lifetime.
// ─────────────────────────────────────────────────────────────────────────────
struct Resource {
    int         id_;
    std::string label_;
    int         value_;

    Resource(std::string label, int value)
        : id_(next_id()), label_(std::move(label)), value_(value)
    {
        std::cerr << "[Resource #" << id_ << " \"" << label_ << "\" val=" << value_ << "] born\n";
    }

    ~Resource() {
        std::cerr << "[Resource #" << id_ << " \"" << label_ << "\"] destroyed\n";
    }

    void describe() const {
        std::cerr << "  Resource #" << id_ << " \"" << label_ << "\" = " << value_ << "\n";
    }
};

// =============================================================================
// BUG A — raw new leaks on exception path
//
// process_data allocates a Resource with raw new, does some work,
// then deletes it. But if the work throws, the delete is skipped.
//
// Your prediction: ___________________________________________________________
// ASan symptom:    ___________________________________________________________
// Fix:             ___________________________________________________________
// =============================================================================
static void risky_work(int x) {
    if (x > 50)
        throw std::runtime_error("value too large");
    std::cerr << "  risky_work: processed " << x << "\n";
}

void process_data(int x) {
    Resource* r = new Resource("process_data", x);

    risky_work(x);

    r->describe();
    delete r; 
}

// =============================================================================
// BUG B — ???
//
// Your prediction: ___________________________________________________________
// ASan symptom:    ___________________________________________________________
// Fix:             ___________________________________________________________
// =============================================================================
struct Subscriber;

struct Publisher {
    int id_;
    std::shared_ptr<Subscriber> subscriber_; 

    explicit Publisher(int id) : id_(id) {
        std::cerr << "[Publisher  #" << id_ << "] born\n";
    }
    ~Publisher() {
        std::cerr << "[Publisher  #" << id_ << "] destroyed\n";
    }
};

struct Subscriber {
    int id_;
    std::shared_ptr<Publisher> publisher_;

    explicit Subscriber(int id) : id_(id) {
        std::cerr << "[Subscriber #" << id_ << "] born\n";
    }
    ~Subscriber() {
        std::cerr << "[Subscriber #" << id_ << "] destroyed\n";
    }
};

void run_pubsub() {
    auto pub = std::make_shared<Publisher>(1);
    auto sub = std::make_shared<Subscriber>(2);

    pub->subscriber_ = sub;
    sub->publisher_  = pub;

    std::cerr << "  pub.use_count=" << pub.use_count()
              << "  sub.use_count=" << sub.use_count() << "\n";
}

// =============================================================================
// BUG C — unique_ptr passed by value unnecessarily
//
// Your prediction: ___________________________________________________________
// Fix:             ___________________________________________________________
//
// Note: ASan won't catch this one — it's not a memory error, it's a logic
// error. The log will show the resource dying at the wrong time.
// =============================================================================
void print_resource(std::unique_ptr<Resource> r) { 
    std::cerr << "  print_resource: ";
    r->describe();
}

void use_resource() {
    auto r = std::make_unique<Resource>("use_resource", 77);

    print_resource(std::move(r)); 

    // What's the value of r now?
    std::cerr << "  back in use_resource, value=" << r->value_ << "\n"; 
}

// =============================================================================
// main — runs all three buggy scenarios
// =============================================================================
int main() {
    std::cerr << "=== Stage 5: Leak Hunt ===\n\n";

    // ── Bug A ──────────────────────────────────────────────────────────────
    std::cerr << "-- Bug A: process_data(10) --\n";
    try { process_data(10); } catch (...) {}
    std::cerr << "  (no exception — did the resource get destroyed?)\n\n";

    std::cerr << "-- Bug A: process_data(99) --\n";
    try {
        process_data(99);
    } catch (const std::exception& e) {
        std::cerr << "  caught: " << e.what() << "\n";
    }
    std::cerr << "  (exception path — was the resource destroyed before the throw?)\n\n";

    // ── Bug B ──────────────────────────────────────────────────────────────
    std::cerr << "-- Bug B: pub/sub cycle --\n";
    run_pubsub();
    std::cerr << "  (did Publisher and Subscriber both get destroyed?)\n\n";

    // ── Bug C ──────────────────────────────────────────────────────────────
    std::cerr << "-- Bug C: unique_ptr by value --\n";
    // Comment this out once you understand the crash — fix it first.
    use_resource();
    std::cerr << "  (was the resource still alive after print_resource returned?)\n\n";

    std::cerr << "=== end of main ===\n";
    return 0;
}

// =============================================================================
// EXPECTED OUTPUT after all three bugs are fixed
// (IDs may differ; the important thing is the pairing of born/destroyed)
//
// === Stage 5: Leak Hunt ===
//
// -- Bug A: process_data(10) --
// [Resource #1 "process_data" val=10] born
//   risky_work: processed 10
//   Resource #1 "process_data" = 10
// [Resource #1 "process_data"] destroyed        <-- must appear BEFORE "no exception"
//   (no exception — did the resource get destroyed?)
//
// -- Bug A: process_data(99) --
// [Resource #2 "process_data" val=99] born
// [Resource #2 "process_data"] destroyed        <-- must appear BEFORE "caught"
//   caught: value too large
//   (exception path — was the resource destroyed before the throw?)
//
// -- Bug B: pub/sub cycle --
// [Publisher  #1] born
// [Subscriber #2] born
//   pub.use_count=1  sub.use_count=1             <-- use_count stays 1 with weak_ptr fix
// [Subscriber #2] destroyed                     <-- both must appear here
// [Publisher  #1] destroyed
//   (did Publisher and Subscriber both get destroyed?)
//
// -- Bug C: unique_ptr by value --
// [Resource #3 "use_resource" val=77] born
//   print_resource:   Resource #3 "use_resource" = 77
//   back in use_resource, value=77               <-- resource still alive
// [Resource #3 "use_resource"] destroyed         <-- destroyed when use_resource() returns
//   (was the resource still alive after print_resource returned?)
//
// === end of main ===
// =============================================================================

// =============================================================================
// FIX GUIDE (read only after attempting fixes yourself)
//
// Bug A fix:
//   Replace:  Resource* r = new Resource("process_data", x);
//   With:     auto r = std::make_unique<Resource>("process_data", x);
//   Delete the manual `delete r;` line.
//   unique_ptr's destructor runs even when an exception unwinds the stack.
//
// Bug B fix:
//   In Publisher, change:
//       std::shared_ptr<Subscriber> subscriber_;
//   to:
//       std::weak_ptr<Subscriber> subscriber_;
//   Then to use it: if (auto s = subscriber_.lock()) { ... }
//
// Bug C fix:
//   Change the signature of print_resource to take by const reference:
//       void print_resource(const Resource& r)
//   or to take a raw (non-owning) pointer:
//       void print_resource(const Resource* r)
//   Remove the std::move at the call site.
//   Rule of thumb: if a function doesn't need to *own* the resource,
//   it should not take a smart pointer by value.
// =============================================================================
