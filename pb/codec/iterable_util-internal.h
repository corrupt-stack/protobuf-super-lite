// Copyright 2022 Yuri Wiitala. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <iterator>
#include <type_traits>

namespace pb::codec::internal {

template <typename T, typename Enable = void>
struct IterableDetector {
  constexpr static bool kIsIterable = false;
};

template <typename T>
struct IterableDetector<
    T,
    typename std::enable_if_t<std::is_integral_v<decltype(std::distance(
        std::begin(std::declval<T>()),
        std::end(std::declval<T>())))>>> {
  constexpr static bool kIsIterable = true;
};

// Removes the qualifiers from |T|.
template <typename T, typename Enable = void>
struct QualifierRemover {
  using Type = std::remove_cv_t<T>;
};

// Removes the qualifiers from |Pair|, and also from its |first_type| and
// |second_type|.
template <typename Pair>
struct QualifierRemover<
    Pair,
    typename std::enable_if_t<std::is_const_v<typename Pair::first_type> ||
                              std::is_const_v<typename Pair::second_type> ||
                              std::is_volatile_v<typename Pair::first_type> ||
                              std::is_volatile_v<typename Pair::second_type>>> {
 private:
  template <template <typename, typename> class PairT,
            typename First,
            typename Second>
  static PairT<std::remove_cv_t<First>, std::remove_cv_t<Second>>
      a_hypothetical_function(PairT<First, Second>);

 public:
  using Type = decltype(a_hypothetical_function(std::remove_cv_t<Pair>{}));
};

template <typename Iterable, typename Enable = void>
struct IterableValueTypeDetector {};

// When iterators provide lvalue or rvalue references, resolve the object type
// from them.
template <typename Iterable>
struct IterableValueTypeDetector<
    Iterable,
    typename std::enable_if_t<
        std::is_reference_v<decltype(*std::begin(std::declval<Iterable>()))>>>
    : public QualifierRemover<std::remove_reference_t<decltype(*std::begin(
          std::declval<Iterable>()))>> {};

// When iterators provide "proxy reference" objects (e.g., std::vector<bool>)
// instead, fall-back to using the Iterable::value_type member (hopefully, it is
// defined).
template <typename Iterable>
struct IterableValueTypeDetector<
    Iterable,
    typename std::enable_if_t<
        !std::is_reference_v<decltype(*std::begin(std::declval<Iterable>()))>>>
    : public QualifierRemover<typename Iterable::value_type> {};

}  // namespace pb::codec::internal
