/**
 * @file oneData.h
 * @author Rui Azevedo (neu-rah) (ruihfazevedo@gmail.com)
 * @brief Data API - Lightweight data components for HAPI and embedded systems
 *
 * USAGE EXAMPLES:
 * ============================================================================
 * 1. Local Owned Data with Change Tracking (Watch)
 *  auto power = DataDef<Watch, Int>{60};
 *  power.set(80);
 *  if (power.changed()) {
 *    power.sync();
 *  }
 *
 * 2. External Hardware/Variable Reference via Pointer (0 Bytes RAM)
 *  inline volatile int fake_hw{};
 *  using PinPort = DataRef<volatile int*, &fake_hw>;
 *  DataDef<PinPort> led_pin;
 *  led_pin.set(0xFF); // Writes directly to fake_hw
 *
 * 3. Static range with deferred type resolution
 *  DataDef<StaticNumRange<StaticRange<0,100>>, Int> pct{50};
 *  pct.up();   // 51
 *  pct.down(); // 50
 * ============================================================================
 */

#pragma once

// #include <type_traits>
// #include <utility>

#include <hapi/hapi.h>
using hapi::APIOf;

namespace oneData {
  using CText = const char *;

  // VALUE DESCRIPTORS ----------------------------------------------------------
  // Carry compile-time values without committing to a Unit type.
  // Type is resolved by the chain at the call site via template method.

  /// @brief compile-time value descriptor — deferred type resolution
  template<auto _value>
  struct Val {
    template<typename T>
    static constexpr T value() noexcept { return static_cast<T>(_value); }
  };

  /// @brief compile-time range descriptor — deferred type resolution
  template<auto _low, auto _high, bool _wraps=false>
  struct StaticRange {
    template<typename T>
    static constexpr T low() noexcept { return static_cast<T>(_low); }
    template<typename T>
    static constexpr T high() noexcept { return static_cast<T>(_high); }
    static constexpr bool wraps() noexcept { return _wraps; }

    template<typename T>
    static constexpr bool valid(T v) noexcept {
      return v >= low<T>() && v <= high<T>();
    }
    template<typename T>
    static constexpr T clamp(T v) noexcept {
      return v < low<T>() ? low<T>() : v > high<T>() ? high<T>() : v;
    }
    template<typename T>
    static constexpr T stepUp(T o, T s) noexcept {
      return high<T>() - o >= s ? o + s : _wraps ? low<T>() : high<T>();
    }
    template<typename T>
    static constexpr T stepDown(T o, T s) noexcept {
      return o - low<T>() >= s ? o - s : _wraps ? high<T>() : low<T>();
    }
  };

  // BASE DATA API --------------------------------------------------------------
  template <typename O = hapi::Nil>
  struct DataAPI : O {
    using Base = O;
    using Base::Base;

    static constexpr bool changed() noexcept { return false; }
    static constexpr void sync() noexcept {}
    template<typename Out> void print(Out& out) const noexcept {}
  };

  template <typename... OO> using DataDef = APIOf<DataAPI<>, OO...>;

  // STATIC DATA (Compile-Time Constant - Flash/Immediate - 0 Bytes RAM) --------
  /// @brief StaticData<Val<42>> or StaticData<Val<42>> — type from chain
  template<typename ValDesc>
  struct StaticData {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;
      using Type = typename O::Type;

      static constexpr Type get() noexcept {
        return ValDesc::template value<Type>();
      }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      operator Type() const noexcept { return get(); }
    };
  };

  // static text --
  template <const char* const* text_ptr>
  struct StaticText {
    using Type = const char*;
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = const char*;
      using Base::Base;

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      static constexpr Type get() noexcept { return *text_ptr; }
    };
  };

  // DATA (Owned RAM Storage) ---------------------------------------------------
  template <typename T>
  struct Data {
    using Type = T;
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = T;
      using Base::Base;

      Type data{};

      // template <typename V, typename... OO>
      // constexpr Part(V&& val, OO&&... oo) noexcept
      //     : Base{std::forward<OO>(oo)...}, data{std::forward<V>(val)} {}

      template <typename V, typename... OO>
      constexpr Part(V v, OO&&... oo) noexcept
          : Base{std::forward<OO>(oo)...}, data{v} {}

      template <typename... OO>
      constexpr Part(OO&&... oo) noexcept : Base{std::forward<OO>(oo)...} {}

      const std::decay_t<Type>& get() const noexcept { return data; }

      template <typename V>
      void set(V&& v) noexcept { data = std::forward<V>(v); }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      operator std::decay_t<Type>&() noexcept             { return data; }
      operator const std::decay_t<Type>&() const noexcept { return data; }
    };
  };

  // DATA REF (Unified External Pointer/Reference - 0 Bytes RAM) ---------------
  template <auto address>
  struct DataRef {
    using Type = std::remove_pointer_t<decltype(address)>;
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = std::remove_pointer_t<decltype(address)>;
      using Base::Base;

      static auto& get() noexcept {
        if constexpr (std::is_same_v<std::remove_cv_t<Type>, char>) {
          return address;
        } else {
          return *address;
        }
      }

      static void set(Type v) noexcept { *address = v; }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      operator auto&() noexcept { return get(); }
      operator std::remove_cv_t<Type>() const noexcept { return get(); }
    };
  };

  // WATCH (Change Tracking Modifier) ------------------------------------------
  template <typename W>
  struct Watch {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::set;

      std::remove_reference_t<Type> watched{};

      constexpr bool changed() const noexcept { return get() != watched; }
      void sync() noexcept { watched = get(); }
    };
  };

  // NUMBER RANGE (Dynamic Boundaries) -----------------------------------------
  template <typename N>
  struct NumRange {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;
      using Type = N;
      using NRP = std::decay_t<Type>;

      using Base::get;
      using Base::set;

      NRP m_low, m_high;
      bool wraps;

      template <typename... OO>
      constexpr Part(NRP low, NRP high, bool w, OO&&... oo) noexcept
          : Base{std::forward<OO>(oo)...}, m_low{low}, m_high{high}, wraps{w} {}

      constexpr bool valid(NRP v) const noexcept { return v >= m_low && v <= m_high; }
      constexpr NRP clamp(NRP v) const noexcept {
        return v < m_low ? m_low : v > m_high ? m_high : v;
      }
      constexpr NRP stepUp(NRP o, NRP s) noexcept {
        return m_high - o >= s ? o + s : wraps ? m_low : m_high;
      }
      constexpr NRP stepDown(NRP s, NRP o) noexcept {
        return o - m_low >= s ? o - s : wraps ? m_high : m_low;
      }

      void up(NRP s = 1) noexcept { set(stepUp(get(), s)); }
      void down(NRP s = 1) noexcept { set(stepDown(s, get())); }
    };
  };

  // STATIC NUMBER RANGE (Compile-Time Boundaries - 0 Bytes RAM) ---------------
  /// @brief StaticNumRange<StaticRange<0,100>> or StaticNumRange<StaticRange<0,255,true>>
  template<typename RangeDesc>
  struct StaticNumRange {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;
      using Type = typename O::Type;
      using Base::get;
      using Base::set;

      static constexpr bool valid(Type v) noexcept {
        return RangeDesc::template valid<Type>(v);
      }
      static constexpr Type clamp(Type v) noexcept {
        return RangeDesc::template clamp<Type>(v);
      }

      void up(Type step=1) noexcept {
        set(RangeDesc::template stepUp<Type>(get(), step));
      }
      void down(Type step=1) noexcept {
        set(RangeDesc::template stepDown<Type>(get(), step));
      }
    };
  };

  // DEFAULT (Default Value Injection Modifier) ---------------------------------
  /// @brief wraps a data component, injecting a compile-time default value
  /// Default<Data<int>, 0>
  /// Watch<Default<Data<int>, 0>>  — composable freely
  template<typename W, auto val>
  struct Default {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      // no using Base::Base — prevents zero-init constructor from winning

      constexpr Part() noexcept
          : Base{val} {}

      template <typename... OO>
      constexpr Part(OO&&... oo) noexcept
          : Base{val, std::forward<OO>(oo)...} {}

      constexpr Part(Type v) noexcept
          : Base{v} {}
    };
  };

  // SUGAR ALIASES --------------------------------------------------------------
  using Text = Data<const char *>;
  using Bool = Data<bool>;
  using Int  = Data<int>;

  // StaticData with explicit type — standalone use without chain Type
  template<auto v>
  struct StaticVal {
    using Type = decltype(v);
    template<typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;
      using Type = decltype(v);
      static constexpr Type get() noexcept { return v; }
      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
      operator Type() const noexcept { return get(); }
    };
  };

  template<int v>  using StaticInt  = StaticVal<v>;
  template<bool v> using StaticBool = StaticVal<v>;
  template<char v> using StaticChar = StaticVal<v>;

  template<auto p> using IntRef  = DataRef<p>;
  template<auto p> using BoolRef = DataRef<p>;
  template<auto p> using CharRef = DataRef<p>;

}; // namespace oneData
