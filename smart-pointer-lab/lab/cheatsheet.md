# RAII & Smart Pointers — Quick Reference

## The ownership decision tree

```
Do you need to manage a heap-allocated resource?
│
├── Does exactly ONE thing own it?
│       └── std::unique_ptr<T>          (default choice)
│
├── Is ownership genuinely shared?
│       └── std::shared_ptr<T>          (use make_shared)
│
├── Do you observe a shared_ptr but must NOT keep it alive?
│       └── std::weak_ptr<T>            (call .lock() to use)
│
├── Do you just need access, lifetime guaranteed by caller?
│       └── T& or const T&              (non-owning reference)
│
├── Do you need nullable non-owning access?
│       └── T*                          (raw pointer, borrow only)
│
└── Is the resource not memory (file, lock, socket...)?
        └── RAII class with destructor
```

## Smart pointer cheat sheet

| Type | Copyable | Movable | Overhead | Use when |
|---|---|---|---|---|
| `unique_ptr<T>` | No | Yes | None (zero-cost) | One clear owner |
| `shared_ptr<T>` | Yes | Yes | Atomic ref-count + control block | Genuinely shared lifetime |
| `weak_ptr<T>` | Yes | Yes | Minimal | Observer; breaks cycles |

## Creation

```cpp
// unique_ptr — prefer make_unique
auto p = std::make_unique<Foo>(args...);

// shared_ptr — prefer make_shared (single allocation)
auto s = std::make_shared<Foo>(args...);

// weak_ptr — created from a shared_ptr
std::weak_ptr<Foo> w = s;

// Safely using a weak_ptr
if (auto locked = w.lock()) {
    locked->do_something();   // 'locked' is a shared_ptr, valid here
} else {
    // object is gone
}
```

## Ownership in function signatures

```cpp
// Takes ownership (caller's pointer becomes null after the call)
void consume(std::unique_ptr<Foo> p);
consume(std::move(myFoo));

// Borrows — caller keeps ownership, function must not outlive caller
void inspect(const Foo& f);
void inspect(Foo* f);         // nullable borrow

// Joins shared ownership (ref count goes up for the duration)
void share(std::shared_ptr<Foo> s);
```

## RAII pattern

```cpp
class FileHandle {
    FILE* f_;
public:
    explicit FileHandle(const char* path)
        : f_(std::fopen(path, "r")) {}

    ~FileHandle() { if (f_) std::fclose(f_); }

    // Rule of Three/Five or Rule of Zero — pick one.
    // Easiest: delete copy, allow move (or just delete both).
    FileHandle(const FileHandle&)            = delete;
    FileHandle& operator=(const FileHandle&) = delete;
};
```

## Rule of Three / Five / Zero

| Rule | When | What to do |
|---|---|---|
| **Rule of Zero** | Member types already handle cleanup (`unique_ptr`, `shared_ptr`) | Write no special members — let the compiler synthesize |
| **Rule of Three** | You write a destructor that manages a raw resource | Also write copy constructor and copy-assignment operator |
| **Rule of Five** | Same as above, and performance matters | Also write move constructor and move-assignment operator |

## The ref-cycle trap

```cpp
// BROKEN: A and B keep each other alive forever
struct A { std::shared_ptr<B> b; };
struct B { std::shared_ptr<A> a; };  // cycle — neither ever destroyed

// FIXED: back-reference is weak (observer, not owner)
struct B { std::weak_ptr<A> a; };    // A can die independently
```

## Swift ↔ C++ mental map

| Swift | C++ equivalent |
|---|---|
| `class Foo` (reference type) | `std::shared_ptr<Foo>` |
| `struct Foo` (value type) | `Foo` (stack / value semantics) |
| `weak var x: Foo?` | `std::weak_ptr<Foo>` |
| `deinit` | `~Foo()` destructor |
| `defer { cleanup() }` | RAII class destructor (scoped) |
| ARC / ref counting | `shared_ptr` control block |
| `~Copyable` struct (Swift 5.9+) | `std::unique_ptr<T>` (move-only) |
| `[weak self]` capture | `weak_ptr` member / capture |

## Compile command for the lab

```bash
g++ -std=c++20 -Wall -Wextra -fsanitize=address,undefined -g FILE.cpp -o FILE
```

AddressSanitizer (`-fsanitize=address`) will:
- Report heap-use-after-free
- Report double-free
- Report memory leaks (at program exit)

Combined with the `Tracker<T>` log, you have two independent ways to
detect every bug in the lab.
