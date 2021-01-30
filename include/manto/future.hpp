#pragma once

#include <experimental/coroutine>
#include <mutex>
#include <memory>
#include <variant>

struct Empty {};
struct Void {}; // darn it, Matt Calabrese! I need regular void!
struct Error {
  Error(const char*f, int l) {
    fprintf(stderr, "FATAL %s %d\n", f, l);
    abort();
  }
};

#define ERROR() Error(__FILE__, __LINE__)

template<typename T>
struct future {
    struct promise {
        std::experimental::coroutine_handle<> awaiting = {};
        future<T>* f = nullptr;
        ~promise() {
            if (f) f->detached = true;
        }
        void set_future(future<T>* f) {
            this->f = f;
        }
        auto get_return_object() {
            return future<T>{handle_type::from_promise(*this)};
        }
        auto initial_suspend() {
            return std::experimental::suspend_never{};
        }
        auto return_value(T v) {
            f->v = std::move(v);
            if (awaiting) {
              std::exchange(awaiting, {}).resume();
            }
            return std::experimental::suspend_never{};
        }
        auto return_value(Error e) {
            f->v = std::move(e);
        }
        auto final_suspend() {
            return std::experimental::suspend_never{};
        }
        void unhandled_exception() {
          try { throw; } catch (std::exception& e) {
            printf("Exception: %s\n", e.what());
          }
            std::exit(1);
        }
    };
    using promise_type = promise;
    using handle_type = std::experimental::coroutine_handle<promise_type>;
    std::variant<Empty, T, Error> v;
    bool detached = false;
    future() {}
    future(handle_type h)
    : coro(h) {
      coro.promise().set_future(this);
    }
    future(future && rhs) {
        coro = std::exchange(rhs.coro, {});
    }
    const future& operator=(future&& rhs) {
        coro = std::exchange(rhs.coro, {});
        return *this;
    }
    ~future() {
        if (!detached) 
          coro.promise().set_future(nullptr);
    }
    bool await_ready() {
        return v.index() > 0;
    }
    void await_suspend(std::experimental::coroutine_handle<> awaiting) {
        coro.promise().awaiting = awaiting;
    }
    auto await_resume() {
        return get_value();
    }
    auto get_value() {
        return std::move(std::get<T>(v));
    }
    handle_type coro;
};


