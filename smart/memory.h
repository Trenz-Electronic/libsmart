/// \file memory.h
/// \brief Provide subsitute for the std::make_unique, which is missing in C++11.
#pragma once

#include <memory>	// std::make_unique, etc.

#if !defined(_MSC_VER) && (__cplusplus < 201402L)
/**
 *  std::make_unique replacement for C++11
 *
 *  @param args the arguments to make_unique (forwarded)
 *
 *  @return the unique pointer
 */
namespace std {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}
#endif
