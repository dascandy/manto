#pragma once

#include "source_location.h"
#include <string>

struct error {
  error(std::string err = "", source_location loc = source_location())
  : err(std::move(err))
  , loc(std::move(loc))
  {}
  std::string err;
  source_location loc;
};

template <typename T>
struct expected {
  bool failed = false;
  union {
    error err;
    T t;
  };
  expected(T t = T())
  : failed(false)
  , t(t)
  {}
  expected(error e)
  : failed(true)
  , err(e)
  {}
  ~expected() {
    if (!failed)
      t.~T();
    else
      err.~error();
  }
  operator error() {
    if (!failed) {
      std::terminate();
    }
    return err;
  }
  explicit operator bool() {
    return not failed;
  }
  T& value() { 
    if (failed) {
      std::terminate();
    }
    return t; 
  }
  
};


