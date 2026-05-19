// =============================================================================
// RAII & Smart Pointers — Stage 6 (Stretch): Custom Deleters on unique_ptr
// custom_deleter.cpp
//
// You have already seen that std::unique_ptr<T> calls `delete` on the managed
// pointer when it goes out of scope. But not every resource is allocated with
// `new` — many C libraries give you a resource via a function like fopen,
// sqlite3_open, pthread_mutex_init, or socket(), and expect you to release it
// with a *different* function like fclose, sqlite3_close, pthread_mutex_destroy,
// or close().
//
// std::unique_ptr's full signature is:
//
//   template<class T, class Deleter = std::default_delete<T>>
//   class unique_ptr;
//
// The second template argument is the deleter — a callable that runs in place
// of `delete`. By substituting your own, you can wrap any C-style acquire/release
// API in RAII. This is the bridge from "smart pointers" to "RAII as a general
// technique."
//
// You will do this in two parts:
//   Part 1 — Wrap FILE* with a stateless lambda deleter.
//   Part 2 — Swap it for a STATEFUL functor that counts how many resources were
//            freed. Predict and verify the sizeof difference.
//
// Uncomment each part's main() block when you reach it. Comment out the previous.
//
// Compile with:
//   g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -g custom_deleter.cpp -o custom_deleter
// =============================================================================

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

// =============================================================================
// PART 1 — Stateless lambda deleter wrapping FILE*
//
// Background:
//   fopen()  returns a FILE*  — that's the acquire.
//   fclose() takes a FILE*    — that's the release.
//
// A raw FILE* has the same problems as a raw heap pointer: forget to close it
// and you leak the OS handle; close it twice and you get undefined behaviour.
// We will hand the close responsibility to a unique_ptr.
//
// Your tasks:
//   1. Define a lambda 'closer' that takes a FILE* and calls std::fclose on it
//      (guard for null).
//   2. Create a using-alias FilePtr = std::unique_ptr<FILE, decltype(closer)>.
//      QUESTION: why decltype(closer) and not just `auto` or a named type?
//   3. Write open_file(path, mode) — a factory that returns a FilePtr.
//      QUESTION: why must we pass `closer` as the second constructor argument?
//   4. Use it: open a file, write to it, let the FilePtr go out of scope.
//   5. Print sizeof(FilePtr).
//      PREDICT before running: how does it compare to sizeof(FILE*)?
// =============================================================================

// TODO Part 1.1: define the lambda. It should be a non-capturing lambda that
//                accepts a FILE* and calls std::fclose if the pointer is non-null.
//                Log something like "fclose called on " + the FILE* address to cerr.
// auto closer = [] (FILE* f) { ... };

// TODO Part 1.2: define the using-alias.
// using FilePtr = std::unique_ptr<FILE, decltype(closer)>;

// TODO Part 1.3: implement the factory.
// FilePtr open_file(const char* path, const char* mode) {
//     FILE* raw = std::fopen(path, mode);
//     if (!raw) throw std::runtime_error("fopen failed");
//     return FilePtr(raw, closer);
// }

// ─── Part 1 main ────────────────────────────────────────────────────────────
/*
int main() {
    std::cerr << "=== Part 1: stateless lambda deleter ===\n";

    // Predict: how many "fclose called on ..." lines will print?
    {
        FilePtr f = open_file("/tmp/raii_demo.txt", "w");
        std::fputs("hello, RAII\n", f.get());
        // f goes out of scope here — fclose should run automatically.
    }

    // Exception path: confirm the file still gets closed.
    std::cerr << "\n-- exception path --\n";
    try {
        FilePtr f = open_file("/tmp/raii_demo.txt", "w");
        std::fputs("about to throw\n", f.get());
        throw std::runtime_error("simulated failure");
    } catch (const std::exception& e) {
        std::cerr << "caught: " << e.what() << "\n";
    }

    // Size check — what do you predict?
    std::cerr << "\nsizeof(FILE*)   = " << sizeof(FILE*) << "\n";
    std::cerr << "sizeof(FilePtr) = " << sizeof(FilePtr) << "\n";
    // EXPECTED: equal. Stateless lambdas are empty types; the compiler uses
    // empty-base-optimization so the deleter takes 0 bytes inside the unique_ptr.

    return 0;
}
*/

// =============================================================================
// PART 2 — Stateful counting functor deleter
//
// Background:
//   A deleter is just a callable. So far we've used a lambda. But the deleter
//   can also be a struct with an operator() — which means it can carry STATE.
//
// You'll build a deleter that increments a shared counter every time it runs.
// This is contrived (you wouldn't really count fcloses) but it makes the cost
// of statefulness visible.
//
// Your tasks:
//   1. Define struct CountingFileCloser with an int* member and operator()(FILE*).
//      QUESTION: why an int* rather than an int? (Hint: we want multiple
//                unique_ptrs to update the SAME counter.)
//   2. Create a using-alias CountedFilePtr = std::unique_ptr<FILE, CountingFileCloser>.
//   3. Write open_counted(path, mode, counter) — a factory that takes the counter
//      by reference and returns a CountedFilePtr.
//   4. Open three files using the same counter, let them go out of scope at
//      staggered points (inner blocks), and confirm counter == 3 at the end.
//   5. Print sizeof(CountedFilePtr).
//      PREDICT before running: how does it compare to sizeof(FilePtr) from Part 1?
// =============================================================================

// TODO Part 2.1: define the functor.
// struct CountingFileCloser {
//     int* close_count;
//     void operator()(FILE* f) const {
//         if (f) {
//             std::fclose(f);
//             ++*close_count;
//             std::cerr << "fclose #" << *close_count << " on " << f << "\n";
//         }
//     }
// };

// TODO Part 2.2: define the using-alias.
// using CountedFilePtr = std::unique_ptr<FILE, CountingFileCloser>;

// TODO Part 2.3: implement the factory. The deleter instance must be constructed
//                with a pointer to the caller's counter.
// CountedFilePtr open_counted(const char* path, const char* mode, int& counter) {
//     FILE* raw = std::fopen(path, mode);
//     if (!raw) throw std::runtime_error("fopen failed");
//     return CountedFilePtr(raw, CountingFileCloser{&counter});
// }

// ─── Part 2 main ────────────────────────────────────────────────────────────
/*
int main() {
    std::cerr << "=== Part 2: stateful counting deleter ===\n";

    int closes = 0;

    {
        auto a = open_counted("/tmp/raii_a.txt", "w", closes);
        {
            auto b = open_counted("/tmp/raii_b.txt", "w", closes);
            std::cerr << "two files open, closes=" << closes << "\n";
        } // b closes here — closes should become 1
        std::cerr << "after inner block, closes=" << closes << "\n";

        auto c = open_counted("/tmp/raii_c.txt", "w", closes);
        std::cerr << "two files open again, closes=" << closes << "\n";
    } // a and c close here — closes should become 3

    std::cerr << "final closes=" << closes << "  (expected 3)\n\n";

    // Size check — the surprise.
    std::cerr << "sizeof(FILE*)          = " << sizeof(FILE*) << "\n";
    std::cerr << "sizeof(CountedFilePtr) = " << sizeof(CountedFilePtr) << "\n";
    // EXPECTED: roughly 2x sizeof(FILE*). The deleter has state (an int*) so
    // it must be stored inside the unique_ptr — empty-base-optimization no
    // longer helps. This is the cost of stateful deleters, and it's why the
    // "unique_ptr is the same size as a raw pointer" invariant from Stage 2
    // is only true for stateless deleters.

    return 0;
}
*/

// ─── Placeholder main — uncomment Part 1 or Part 2 above ────────────────────
int main() {
    std::cerr << "Uncomment Part 1 or Part 2 main() and recompile.\n";
    return 0;
}

// =============================================================================
// EXPECTED OUTPUT (after both parts are uncommented and the TODOs are filled in)
//
// Part 1:
//   === Part 1: stateless lambda deleter ===
//   fclose called on 0x...
//
//   -- exception path --
//   fclose called on 0x...
//   caught: simulated failure
//
//   sizeof(FILE*)   = 8
//   sizeof(FilePtr) = 8                          <-- same as raw pointer
//
// Part 2:
//   === Part 2: stateful counting deleter ===
//   two files open, closes=0
//   fclose #1 on 0x...
//   after inner block, closes=1
//   two files open again, closes=1
//   fclose #2 on 0x...
//   fclose #3 on 0x...
//   final closes=3  (expected 3)
//
//   sizeof(FILE*)          = 8
//   sizeof(CountedFilePtr) = 16                  <-- doubled by the stateful deleter
//
// On a 64-bit machine, pointers are 8 bytes; on 32-bit, halve all the numbers.
// The ratio is the point, not the absolute size.
// =============================================================================

// =============================================================================
// DISCUSSION (after both parts)
//
// 1. Other C APIs you could wrap exactly this way:
//      - sqlite3_open / sqlite3_close
//      - pthread_mutex_init / pthread_mutex_destroy
//      - socket() / close()
//      - OpenSSL's SSL_new / SSL_free
//      - cudaMalloc / cudaFree
//    Any C library with paired create/destroy calls is a candidate.
//
// 2. shared_ptr also accepts a custom deleter, but the mechanism is different:
//
//      std::shared_ptr<FILE> sp(std::fopen("...", "r"), &std::fclose);
//
//    The deleter is type-erased — stored in the control block, not in the
//    type. That means every shared_ptr<FILE> has the same type regardless of
//    its deleter. Trade-off: more flexible (you can store mixed deleters in
//    a single container) but has a runtime indirection cost. unique_ptr's
//    approach is zero-overhead but the type changes when the deleter does.
//
// 3. C++20 also has std::unique_ptr<T, std::default_delete<T>> in a special
//    array form: std::unique_ptr<T[]> calls `delete[]` instead of `delete`.
//    Same idea — the deleter is a customization point.
// =============================================================================

// =============================================================================
// SWIFT BRIDGE (instructor note — not for students)
//
// The closest Swift analogue: wrapping a C resource (CFType, UnsafeMutablePointer,
// or any pointer from a C API) inside a Swift `class` and releasing it in
// `deinit`. Swift FORCES you to write the wrapper class. C++ lets you bolt
// the cleanup onto a smart pointer directly with no class — that's the win
// custom deleters provide.
//
// Part 1 (stateless lambda) ~ a Swift class whose deinit calls a free function;
//                              the class itself carries no extra state.
// Part 2 (stateful functor) ~ a Swift class whose deinit touches shared mutable
//                              state; the wrapper now carries that state.
//
// In both languages, the cost story is identical: stateful cleanup costs storage.
// =============================================================================
