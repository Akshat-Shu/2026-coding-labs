#pragma once

#include <cstddef>
#include <utility>

namespace cs106l {

/**
 * @brief A smart pointer that owns an object and deletes it when it goes out of scope.
 * @tparam T The type of the object to manage.
 * @note This class is a simpler version of `std::unique_ptr`.
 */
template <typename T> class unique_ptr {
private:
  /* STUDENT TODO: What data must a unique_ptr keep track of? */
  T* ptr_;

public:
  /**
   * @brief Constructs a new `unique_ptr` from the given pointer.
   * @param ptr The pointer to manage.
   * @note You should avoid using this constructor directly and instead use `make_unique()`.
   */
  unique_ptr(T* ptr) : ptr_(ptr) {
    /* STUDENT TODO: Implement the constructor */
    // throw std::runtime_error("Not implemented: unique_ptr(T* ptr)");
  }

  /**
   * @brief Constructs a new `unique_ptr` from `nullptr`.
   */
  unique_ptr(std::nullptr_t) : ptr_(nullptr) {
    /* STUDENT TODO: Implement the nullptr constructor */
    // throw std::runtime_error("Not implemented: unique_ptr(std::nullptr_t)");
  }

  /**
   * @brief Constructs an empty `unique_ptr`.
   * @note By default, a `unique_ptr` points to `nullptr`.
   */
  unique_ptr() : unique_ptr(nullptr) {}

  /**
   * @brief Dereferences a `unique_ptr` and returns a reference to the object.
   * @return A reference to the object.
   */
  T& operator*() {
    /* STUDENT TODO: Implement the dereference operator */
    // throw std::runtime_error("Not implemented: operator*()");
    if(ptr_ == nullptr)
        throw std::runtime_error("Dereferencing a null unique_ptr");
    return *ptr_;
  }

  /**
   * @brief Dereferences a `unique_ptr` and returns a const reference to the object.
   * @return A const reference to the object.
   */
  const T& operator*() const {
    /* STUDENT TODO: Implement the dereference operator (const) */
    // throw std::runtime_error("Not implemented: operator*() const");
    if(ptr_ == nullptr)
        throw std::runtime_error("Dereferencing a null unique_ptr");
    return *ptr_;
  }

  /**
   * @brief Returns a pointer to the object managed by the `unique_ptr`.
   * @note This allows for accessing the members of the managed object through the `->` operator.
   * @return A pointer to the object.
   */
  T* operator->() {
    /* STUDENT TODO: Implement the arrow operator */
    // throw std::runtime_error("Not implemented: operator->()");
    return ptr_;
  }

  /**
   * @brief Returns a const pointer to the object managed by the `unique_ptr`.
   * @note This allows for accessing the members of the managed object through the `->` operator.
   * @return A const pointer to the object.
   */
  const T* operator->() const {
    /* STUDENT TODO: Implement the arrow operator */
    // throw std::runtime_error("Not implemented: operator->() const");
    return ptr_;
  }

  /**
   * @brief Returns whether or not the `unique_ptr` is non-null.
   * @note Marked `explicit` so this conversion only fires in
   *       boolean contexts (e.g. inside an `if`) and does not
   *       silently participate as an implicit conversion to `int`.
   * @return `true` if the `unique_ptr` is non-null, `false` otherwise.
   */
  explicit operator bool() const {
    /* STUDENT TODO: Implement the boolean conversion operator */
    // throw std::runtime_error("Not implemented: operator bool() const");
    return ptr_ != nullptr;
  }

  /** STUDENT TODO: In the space below, do the following:
   * - Implement a destructor
   * - Delete the copy constructor
   * - Delete the copy assignment operator
   * - Implement the move constructor
   * - Implement the move assignment operator
   */

  ~unique_ptr() {
    if(ptr_ != nullptr) {
        delete ptr_;
        ptr_ = nullptr;
    }
  }

  unique_ptr(const unique_ptr& other) = delete;
  unique_ptr& operator=(const unique_ptr& other) = delete;

  unique_ptr(unique_ptr&& other) noexcept : ptr_(other.ptr_) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }

  unique_ptr& operator=(unique_ptr&& other) noexcept {
    if (this == &other)
        return *this;

    if(ptr_ != nullptr) 
        delete ptr_;

    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  T* get() const {
    return ptr_;
  }

  /* STUDENT TODO (Part 3): Implement equality comparisons.
   *
   * Add two friend operators here so that the following compile
   * and behave correctly:
   *
   *   unique_ptr<int> a = make_unique<int>(1);
   *   unique_ptr<int> b = make_unique<int>(2);
   *   bool x = (a == b);        // compare two unique_ptrs
   *   bool y = (a == nullptr);  // compare against nullptr
   *   bool z = (nullptr == a);  // and the reverse
   *
   * Two `unique_ptr`s compare equal iff they hold the same raw
   * pointer (which, given uniqueness, only happens when both are
   * null).
   *
   * Hint: declare them as `friend` inside this class so they can
   * see the private pointer, and define them inline here.
   */

  bool operator==(const unique_ptr& other) const {
    return ptr_ == other.ptr_;
  }

  bool operator==(std::nullptr_t) const {
    return ptr_ == nullptr;
  }

  friend bool operator==(std::nullptr_t, const unique_ptr& uptr) {
    return uptr.ptr_ == nullptr;
  }
};

template <typename T>
bool operator==(std::nullptr_t, const unique_ptr<T>& uptr) {
  return uptr == nullptr;
}

/**
 * @brief Creates a new unique_ptr for a type with the given arguments.
 * @example auto ptr = make_unique<int>(5);
 * @tparam T The type to create a unique_ptr for.
 * @tparam Args The types of the arguments to pass to the constructor of T.
 * @param args The arguments to pass to the constructor of T.
 */
template <typename T, typename... Args> 
unique_ptr<T> make_unique(Args&&... args) {
  return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

}