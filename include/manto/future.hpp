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

template <typename T> struct future;
template <typename T> struct promise;

template <typename T>
struct promise {
    using handle_type = std::experimental::coroutine_handle<promise<T>>;
    std::experimental::coroutine_handle<> awaiting = {};
    future<T>* f = nullptr;
    ~promise();
    void set_future(future<T>* f);
    auto get_return_object();
    auto initial_suspend();
    auto return_value(T v);
    auto return_value(Error e);
    auto final_suspend() noexcept;
    void unhandled_exception();
};

template<typename T>
struct future {
    using promise_type = promise<T>;
    using handle_type = std::experimental::coroutine_handle<promise_type>;

    std::variant<Empty, T, Error> v;
    bool detached = false;
    handle_type coro;
    future(T t);
    future();
    future(handle_type h);
    future(future && rhs);
    const future& operator=(future&& rhs);
    ~future();
    bool await_ready();
    void await_suspend(std::experimental::coroutine_handle<> awaiting);
    auto await_resume();
    auto get_value();
};

template <typename T>
future<T> make_ready_future(T t) {
    return future<T> {std::move(t)};
}

template <typename T>
promise<T>::~promise() {
    if (f) f->detached = true;
}
template <typename T>
void promise<T>::set_future(future<T>* f) {
    this->f = f;
}
template <typename T>
auto promise<T>::get_return_object() {
    return future<T>{handle_type::from_promise(*this)};
}
template <typename T>
auto promise<T>::initial_suspend() {
    return std::experimental::suspend_never{};
}
template <typename T>
auto promise<T>::return_value(T v) {
    if (f) {
        f->v = std::move(v);
        if (awaiting) {
          std::exchange(awaiting, {}).resume();
        }
    }
    return std::experimental::suspend_never{};
}
template <typename T>
auto promise<T>::return_value(Error e) {
    if (f) {
        f->v = std::move(e);
        if (awaiting) {
          std::exchange(awaiting, {}).resume();
        }
    }
    return std::experimental::suspend_never{};
}
template <typename T>
auto promise<T>::final_suspend() noexcept {
    return std::experimental::suspend_never{};
}
template <typename T>
void promise<T>::unhandled_exception() {
    try { throw; } catch (std::exception& e) {
        printf("Exception: %s\n", e.what());
    }
    std::exit(1);
}

template <typename T>
future<T>::future() 
: detached(true)
{}

template <typename T>
future<T>::future(T t) 
: v(std::move(t))
, detached(true)
{
}

template <typename T>
future<T>::future(handle_type h)
: coro(h) {
  coro.promise().set_future(this);
}
template <typename T>
future<T>::future(future && rhs) {
    v = std::move(rhs.v);
    coro = std::exchange(rhs.coro, {});
    detached = rhs.detached;
    rhs.detached = true;
    if (!detached) 
        coro.promise().set_future(this);
}

template <typename T>
const future<T>& future<T>::operator=(future&& rhs) {
    coro = std::exchange(rhs.coro, {});
    v = std::move(rhs.v);
    detached = rhs.detached;
    rhs.detached = true;
    if (!detached) 
        coro.promise().set_future(this);
    return *this;
}
template <typename T>
future<T>::~future() {
    if (!detached) 
      coro.promise().set_future(nullptr);
}
template <typename T>
bool future<T>::await_ready() {
    return v.index() > 0;
}
template <typename T>
void future<T>::await_suspend(std::experimental::coroutine_handle<> awaiting) {
    coro.promise().awaiting = awaiting;
}
template <typename T>
auto future<T>::await_resume() {
    return get_value();
}
template <typename T>
auto future<T>::get_value() {
    return std::move(std::get<T>(v));
}


