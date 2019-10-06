/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "caf/atom.hpp"
#include "caf/detail/bounds_checker.hpp"
#include "caf/detail/move_if_not_ptr.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/dictionary.hpp"
#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/raise_error.hpp"
#include "caf/string_algorithms.hpp"
#include "caf/string_view.hpp"
#include "caf/sum_type.hpp"
#include "caf/sum_type_access.hpp"
#include "caf/sum_type_token.hpp"
#include "caf/timestamp.hpp"
#include "caf/uri.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A type for config parameters with similar interface to a `variant`. This
/// type is not implemented as a simple variant alias because variants cannot
/// contain lists of themselves.
class config_value {
public:
  // -- member types -----------------------------------------------------------

  using integer = int64_t;

  using boolean = bool;

  using real = double;

  using atom = atom_value;

  using timespan = caf::timespan;

  using string = std::string;

  using list = std::vector<config_value>;

  using dictionary = caf::dictionary<config_value>;

  using types = detail::type_list<integer, boolean, real, atom, timespan, uri,
                                  string, list, dictionary>;

  using variant_type = detail::tl_apply_t<types, variant>;

  // -- constructors, destructors, and assignment operators --------------------

  config_value() = default;

  config_value(config_value&& other) = default;

  config_value(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  explicit config_value(T&& x) {
    set(std::forward<T>(x));
  }

  config_value& operator=(config_value&& other) = default;

  config_value& operator=(const config_value& other) = default;

  template <class T, class E = detail::enable_if_t<
                       !std::is_same<detail::decay_t<T>, config_value>::value>>
  config_value& operator=(T&& x) {
    set(std::forward<T>(x));
    return *this;
  }

  ~config_value();

  // -- parsing ----------------------------------------------------------------

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(string_view::iterator first,
                                      string_view::iterator last);

  /// Tries to parse a value from `str`.
  static expected<config_value> parse(string_view str);

  // -- properties -------------------------------------------------------------

  /// Converts the value to a list with one element. Does nothing if the value
  /// already is a list.
  void convert_to_list();

  /// Returns the value as a list, converting it to one if needed.
  list& as_list();

  /// Returns the value as a dictionary, converting it to one if needed.
  dictionary& as_dictionary();

  /// Appends `x` to a list. Converts this config value to a list first by
  /// calling `convert_to_list` if needed.
  void append(config_value x);

  /// Returns a human-readable type name of the current value.
  const char* type_name() const noexcept;

  /// Returns the underlying variant.
  variant_type& get_data() {
    return data_;
  }

  /// Returns the underlying variant.
  const variant_type& get_data() const {
    return data_;
  }

  /// Returns a pointer to the underlying variant.
  variant_type* get_data_ptr() {
    return &data_;
  }

  /// Returns a pointer to the underlying variant.
  const variant_type* get_data_ptr() const {
    return &data_;
  }

private:
  // -- properties -------------------------------------------------------------

  static const char* type_name_at_index(size_t index) noexcept;

  // -- auto conversion of related types ---------------------------------------

  void set(bool x) {
    data_ = x;
  }

  void set(float x) {
    data_ = static_cast<double>(x);
  }

  void set(const char* x) {
    data_ = std::string{x};
  }

  void set(string_view x) {
    data_ = std::string{x.begin(), x.end()};
  }

  template <class T>
  detail::enable_if_t<detail::is_one_of<T, real, atom, timespan, uri, string,
                                        list, dictionary>::value>
  set(T x) {
    data_ = std::move(x);
  }

  template <class T>
  void set_range(T& xs, std::true_type) {
    auto& dict = as_dictionary();
    dict.clear();
    for (auto& kvp : xs)
      dict.emplace(kvp.first, std::move(kvp.second));
  }

  template <class T>
  void set_range(T& xs, std::false_type) {
    auto& ls = as_list();
    ls.clear();
    ls.insert(ls.end(), std::make_move_iterator(xs.begin()),
              std::make_move_iterator(xs.end()));
  }

  template <class T>
  detail::enable_if_t<detail::is_iterable<T>::value
                      && !detail::is_one_of<T, string, list, dictionary>::value>
  set(T xs) {
    using value_type = typename T::value_type;
    detail::bool_token<detail::is_pair<value_type>::value> is_map_type;
    set_range(xs, is_map_type);
  }

  template <class T>
  detail::enable_if_t<std::is_integral<T>::value> set(T x) {
    data_ = static_cast<int64_t>(x);
  }

  // -- member variables -------------------------------------------------------

  variant_type data_;
};

// -- SumType-like access ------------------------------------------------------

template <class T>
struct default_config_value_access {
  static bool is(const config_value& x) {
    return holds_alternative<T>(x.get_data());
  }

  static const T* get_if(const config_value* x) {
    return caf::get_if<T>(&(x->get_data()));
  }

  static T get(const config_value& x) {
    return caf::get<T>(x.get_data());
  }
};

struct config_value_access_unspecialized {};

/// @relates config_value
template <class T>
struct config_value_access : config_value_access_unspecialized {};

#define CAF_DEFAULT_CONFIG_VALUE_ACCESS(type)                                  \
  template <>                                                                  \
  struct config_value_access<type> : default_config_value_access<type> {}

CAF_DEFAULT_CONFIG_VALUE_ACCESS(bool);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(double);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(atom_value);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(timespan);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(std::string);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(config_value::list);
CAF_DEFAULT_CONFIG_VALUE_ACCESS(config_value::dictionary);

#undef CAF_DEFAULT_CONFIG_VALUE_ACCESS

enum class select_config_value_hint {
  is_custom,
  is_integral,
  is_list,
  is_map,
  is_missing,
};

template <class T>
constexpr select_config_value_hint select_config_value_oracle() {
  return !std::is_base_of<config_value_access_unspecialized,
                          config_value_access<T>>::value
           ? select_config_value_hint::is_custom
           : (std::is_integral<T>::value
                ? select_config_value_hint::is_integral
                : (detail::is_map_like<T>::value
                     ? select_config_value_hint::is_map
                     : (detail::is_list_like<T>::value
                          ? select_config_value_hint::is_list
                          : select_config_value_hint::is_missing)));
}

/// Delegates to config_value_access for all specialized versions.
template <class T,
          select_config_value_hint Hint = select_config_value_oracle<T>()>
struct select_config_value_access {
  static_assert(Hint == select_config_value_hint::is_custom,
                "no default or specialization for config_value_access found");
  using type = config_value_access<T>;
};

template <class T>
using select_config_value_access_t =
  typename select_config_value_access<T>::type;

template <>
struct sum_type_access<config_value> {
  using types = typename config_value::types;

  using type0 = typename detail::tl_head<types>::type;

  static constexpr bool specialized = true;

  template <class U, int Pos>
  static bool is(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().is(pos(token));
  }

  template <class U>
  static bool is(const config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::is(x);
  }

  template <class U, int Pos>
  static U& get(config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U>
  static U get(config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static const U& get(const config_value& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U>
  static U get(const config_value& x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get(x);
  }

  template <class U, int Pos>
  static U* get_if(config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(config_value* x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get_if(x);
  }

  template <class U, int Pos>
  static const U* get_if(const config_value* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U>
  static optional<U> get_if(const config_value* x, sum_type_token<U, -1>) {
    return select_config_value_access_t<U>::get_if(x);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(const config_value& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }
};

/// Catches all non-specialized integer types.
template <class T>
struct select_config_value_access<T, select_config_value_hint::is_integral> {
  struct type {
    static bool is(const config_value& x) {
      auto ptr = caf::get_if<typename config_value::integer>(x.get_data_ptr());
      return ptr != nullptr && detail::bounds_checker<T>::check(*ptr);
    }

    static optional<T> get_if(const config_value* x) {
      auto ptr = caf::get_if<typename config_value::integer>(x->get_data_ptr());
      if (ptr != nullptr && detail::bounds_checker<T>::check(*ptr))
        return static_cast<T>(*ptr);
      return none;
    }

    static T get(const config_value& x) {
      auto res = get_if(&x);
      CAF_ASSERT(res != none);
      return *res;
    }
  };
};

/// Catches all non-specialized list types.
template <class T>
struct select_config_value_access<T, select_config_value_hint::is_list> {
  struct type {
    using list_type = T;

    using value_type = typename list_type::value_type;

    static bool is(const config_value& x) {
      auto lst = caf::get_if<config_value::list>(&x);
      if (lst != nullptr) {
        return std::all_of(lst->begin(), lst->end(), [](const config_value& y) {
          return caf::holds_alternative<value_type>(y);
        });
      }
      return false;
    }

    static optional<list_type> get_if(const config_value* x) {
      list_type result;
      auto out = std::inserter(result, result.end());
      auto extract = [&](const config_value& y) {
        auto opt = caf::get_if<value_type>(&y);
        if (opt) {
          *out++ = move_if_optional(opt);
          return true;
        }
        return false;
      };
      auto lst = caf::get_if<config_value::list>(x);
      if (lst != nullptr && std::all_of(lst->begin(), lst->end(), extract))
        return result;
      return none;
    }

    static list_type get(const config_value& x) {
      auto result = get_if(&x);
      if (!result)
        CAF_RAISE_ERROR("invalid type found");
      return std::move(*result);
    }
  };
};

/// Catches all non-specialized list types.
template <class T>
struct select_config_value_access<T, select_config_value_hint::is_map> {
  struct type {
    using map_type = T;

    using mapped_type = typename map_type::mapped_type;

    static bool is(const config_value& x) {
      using value_type = config_value::dictionary::value_type;
      auto dict = caf::get_if<config_value::dictionary>(&x);
      if (dict != nullptr) {
        return std::all_of(dict->begin(), dict->end(), [](const value_type& y) {
          return caf::holds_alternative<mapped_type>(y.second);
        });
      }
      return false;
    }

    static optional<map_type> get_if(const config_value* x) {
      using value_type = config_value::dictionary::value_type;
      map_type result;
      auto extract = [&](const value_type& y) {
        if (auto opt = caf::get_if<mapped_type>(&y.second)) {
          result.emplace(y.first, *opt);
          return true;
        }
        return false;
      };
      auto dict = caf::get_if<config_value::dictionary>(x);
      if (dict != nullptr && std::all_of(dict->begin(), dict->end(), extract))
        return result;
      return none;
    }

    static map_type get(const config_value& x) {
      auto result = get_if(&x);
      if (!result)
        CAF_RAISE_ERROR("invalid type found");
      return std::move(*result);
    }
  };
};

template <>
struct config_value_access<float> {
  static bool is(const config_value& x) {
    return holds_alternative<double>(x.get_data());
  }

  static optional<float> get_if(const config_value* x) {
    auto res = caf::get_if<double>(&(x->get_data()));
    if (res)
      return static_cast<float>(*res);
    return none;
  }

  static float get(const config_value& x) {
    return static_cast<float>(caf::get<double>(x.get_data()));
  }
};

/// Implements automagic unboxing of `std::tuple<Ts...>` from a heterogeneous
///`config_value::list`.
/// @relates config_value
template <class... Ts>
struct config_value_access<std::tuple<Ts...>> {
  using tuple_type = std::tuple<Ts...>;

  static bool is(const config_value& x) {
    if (auto lst = caf::get_if<config_value::list>(&x)) {
      if (lst->size() != sizeof...(Ts))
        return false;
      return rec_is(*lst, detail::int_token<0>(), detail::type_list<Ts...>());
    }
    return false;
  }

  static optional<tuple_type> get_if(const config_value* x) {
    if (auto lst = caf::get_if<config_value::list>(x)) {
      if (lst->size() != sizeof...(Ts))
        return none;
      tuple_type result;
      if (rec_get(*lst, result, detail::int_token<0>(),
                  detail::type_list<Ts...>()))
        return result;
    }
    return none;
  }

  static tuple_type get(const config_value& x) {
    if (auto result = get_if(&x))
      return std::move(*result);
    CAF_RAISE_ERROR("invalid type found");
  }

private:
  template <int Pos>
  static bool rec_is(const config_value::list&, detail::int_token<Pos>,
                     detail::type_list<>) {
    // End of recursion.
    return true;
  }

  template <int Pos, class U, class... Us>
  static bool rec_is(const config_value::list& xs, detail::int_token<Pos>,
                     detail::type_list<U, Us...>) {
    if (!holds_alternative<U>(xs[Pos]))
      return false;
    return rec_is(xs, detail::int_token<Pos + 1>(), detail::type_list<Us...>());
  }

  template <int Pos>
  static bool rec_get(const config_value::list&, tuple_type&,
                      detail::int_token<Pos>, detail::type_list<>) {
    // End of recursion.
    return true;
  }

  template <int Pos, class U, class... Us>
  static bool rec_get(const config_value::list& xs, tuple_type& result,
                      detail::int_token<Pos>, detail::type_list<U, Us...>) {
    if (auto value = caf::get_if<U>(&xs[Pos])) {
      std::get<Pos>(result) = detail::move_if_not_ptr(value);
      return rec_get(xs, result, detail::int_token<Pos + 1>(),
                     detail::type_list<Us...>());
    }
    return false;
  }
};

/// Implements automagic unboxing of `dictionary<V>` from a homogeneous
/// `config_value::dictionary`.
/// @relates config_value
template <class V>
struct config_value_access<dictionary<V>> {
  using map_type = dictionary<V>;

  using kvp = std::pair<const std::string, config_value>;

  static bool is(const config_value& x) {
    auto lst = caf::get_if<config_value::dictionary>(&x);
    if (lst != nullptr) {
      return std::all_of(lst->begin(), lst->end(), [](const kvp& y) {
        return holds_alternative<V>(y.second);
      });
    }
    return false;
  }

  static optional<map_type> get_if(const config_value* x) {
    map_type result;
    auto extract = [&](const kvp& y) {
      auto opt = caf::get_if<V>(&(y.second));
      if (opt) {
        result.emplace(y.first, std::move(*opt));
        return true;
      }
      return false;
    };
    auto lst = caf::get_if<config_value::dictionary>(x);
    if (lst != nullptr && std::all_of(lst->begin(), lst->end(), extract))
      return result;
    return none;
  }

  static map_type get(const config_value& x) {
    auto result = get_if(&x);
    if (!result)
      CAF_RAISE_ERROR("invalid type found");
    return std::move(*result);
  }
};

// -- SumType-like access of dictionary values ---------------------------------

/// @relates config_value
bool operator<(const config_value& x, const config_value& y);

/// @relates config_value
bool operator==(const config_value& x, const config_value& y);

/// @relates config_value
inline bool operator>=(const config_value& x, const config_value& y) {
  return !(x < y);
}

/// @relates config_value
inline bool operator!=(const config_value& x, const config_value& y) {
  return !(x == y);
}

/// @relates config_value
std::string to_string(const config_value& x);

/// @relates config_value
std::ostream& operator<<(std::ostream& out, const config_value& x);

template <class... Ts>
config_value make_config_value_list(Ts&&... xs) {
  std::vector<config_value> lst{config_value{std::forward<Ts>(xs)}...};
  return config_value{std::move(lst)};
}

/// @relates config_value
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, config_value& x) {
  return f(meta::type_name("config_value"), x.get_data());
}

} // namespace caf
