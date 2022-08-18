#pragma once

#include <coroutine>
#include <mutex>
#include <memory>
#include <cassert>
#include "manto/expected.hpp"

template <typename T>
struct blocking_shared_state {
  std::aligned_storage_t<sizeof(T), alignof(T)> storage;
  std::mutex m;
  std::condition_variable cv;
  enum class State {
    Empty,
    Value,
    Error,
    Finished
  } state = State::Empty;
  void set_value(T&& t) noexcept {
    std::unique_lock<std::mutex> l(m);
    assert(state == State::Empty);
    new (&storage) T(t);
    state = State::Value;
  }
  T get_value() noexcept {
    std::unique_lock<std::mutex> l(m);
    cv.wait(l, [this]{return state != State::Empty; });
    if (state != State::Value) {
      assert(state == State::Value);
      __builtin_unreachable();
    }
    state = State::Finished;
    return std::move((T&)storage);
  }
  expected<T> get() noexcept {
    if (state == State::Value) {
      state = State::Finished;
      return std::move((T&)storage);
    } else if (state == State::Error) {
      return {};
    } else {
      assert(state == State::Value || state == State::Error);
      std::terminate();
    }
  }
};

template <>
struct blocking_shared_state<void> {
  std::mutex m;
  std::condition_variable cv;
  enum class State {
    Empty,
    Value,
    Error,
    Finished
  } state = State::Empty;
  void set_value() noexcept {
    std::unique_lock<std::mutex> l(m);
    assert(state == State::Empty);
    state = State::Value;
  }
  void get_value() noexcept {
    std::unique_lock<std::mutex> l(m);
    cv.wait(l, [this]{return state != State::Empty; });
    if (state != State::Value) {
      assert(state == State::Value);
      __builtin_unreachable();
    }
    state = State::Finished;
  }
  expected<T> get() noexcept {
    if (state == State::Value) {
      state = State::Finished;
      return std::move((T&)storage);
    } else if (state == State::Error) {
      return {};
    } else {
      assert(state == State::Value || state == State::Error);
      std::terminate();
    }
  }
};

template <typename T>
struct blocking_promise;

template <typename T>
struct blocking_future {
    using promise_type = blocking_promise<T>;
    std::shared_ptr<blocking_shared_state<T>> st;
    blocking_future(std::shared_ptr<blocking_shared_state<T>>& p) noexcept
    : st(p)
    {
    }
    T get_value() { return st->get_value(); }
    expected<T> get() noexcept { return st->get(); }
};

template <>
struct blocking_promise<void> {
  blocking_promise() 
  : st(std::make_shared<blocking_shared_state<void>>())
  {
  }
  blocking_future<void> get_future() noexcept {
    return st;
  }
  std::shared_ptr<blocking_shared_state<void>> st;
  void set_value() noexcept {
    st->set_value();
  }
  auto get_return_object() {
    return get_future();
  }
  auto initial_suspend() {
    return std::suspend_never{};
  }
  auto return_void() {
    set_value();
    return std::suspend_never{};
  }
  auto final_suspend() {
    return std::suspend_always{};
  }
  void unhandled_exception() {
    std::exit(1);
  }
//  void set_error(...) {}
};

template <typename T>
struct blocking_promise {
  blocking_promise() 
  : st(std::make_shared<blocking_shared_state<T>>())
  {
  }
  blocking_future<T> get_future() noexcept {
    return st;
  }
  std::shared_ptr<blocking_shared_state<T>> st;
  void set_value(T&& value) noexcept {
    st->set_value(std::move(value));
  }
  auto get_return_object() {
    return get_future();
  }
  auto initial_suspend() {
    return std::suspend_never{};
  }
  auto return_value(T v) {
    set_value(std::move(v));
    return std::suspend_never{};
  }
  auto final_suspend() {
    return std::suspend_always{};
  }
  void unhandled_exception() {
    std::exit(1);
  }
//  void set_error(...) {}
};

