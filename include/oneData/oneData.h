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
 * ============================================================================
 */

#pragma once

#include <type_traits>
#include <utility>

#include <hapi/hapi.h>
using hapi::APIOf;

namespace oneData {
  using CText = const char *;

  // ==========================================================================
  // BASE DATA API
  // ==========================================================================
  template <typename O = hapi::Nil>
  struct DataAPI : O {
    using Base = O;
    using Base::Base;

    static constexpr bool changed() noexcept { return false; }
    static constexpr void sync() noexcept {}
    template<typename Out> void print(Out& out) const noexcept {}
  };

  template <typename... OO>
  struct DefaultDataDef : APIOf<DataAPI<>, OO...> {
    using Base = APIOf<DataAPI<>, OO...>;
    using Base::Base;
  };

  template <typename... OO> using DataDef = DefaultDataDef<OO...>;

  // ==========================================================================
  // STATIC DATA (Compile-Time Constant - Flash/Immediate - 0 Bytes RAM)
  // ==========================================================================
  template <typename T, T value>
  struct StaticData {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = T;
      using Base::Base;

      static constexpr const Type& get() noexcept { return value; }
      
      template<typename Out> 
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      operator const Type&() const noexcept { return get(); }
    };
  };

  // ==========================================================================
  // DATA (Owned RAM Storage)
  // ==========================================================================
  template <typename T>
  struct Data {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = T;
      using Base::Base;

      Type data{};

      template <typename V, typename... OO>
      constexpr Part(V&& val, OO&&... oo) noexcept
          : Base{std::forward<OO>(oo)...}, data{std::forward<V>(val)} {}

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

  // ==========================================================================
  // DATA REF (Unified External Pointer/Reference - 0 Bytes RAM)
  // ==========================================================================
  template <typename T, T address>
  struct DataRef {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = std::remove_pointer_t<T>;
      using Base::Base;

      static auto& get() noexcept {
        if constexpr (std::is_same_v<std::remove_cv_t<Type>, char>) {
          return address;
        } else {
          return *address;
        }
      }

      static void set(Type v) noexcept {*address = v;}
      
      template<typename Out> 
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }

      // Both operators present and safe for PC/Embedded environments
      operator auto&() noexcept { return get(); }
      operator std::remove_cv_t<Type>() const noexcept { return get(); }    };
  };

  // ==========================================================================
  // WATCH (Change Tracking Modifier)
  // ==========================================================================
  template <typename W>
  struct Watch {
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

  // ==========================================================================
  // NUMBER RANGE (Dynamic Boundaries)
  // ==========================================================================
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
      constexpr NRP clamp(NRP v) const noexcept { return v < m_low ? m_low : v > m_high ? m_high : v; }

      constexpr NRP stepUp(NRP o, NRP s) noexcept { return m_high - o >= s ? o + s : wraps ? m_low : m_high; }
      constexpr NRP stepDown(NRP s, NRP o) noexcept { return o - m_low >= s ? o - s : wraps ? m_high : m_low; }

      void up(NRP s = 1) noexcept { set(stepUp(get(), s)); }
      void down(NRP s = 1) noexcept { set(stepDown(s, get())); }
    };
  };

  // ==========================================================================
  // STATIC NUMBER RANGE (Compile-Time Boundaries - 0 Bytes RAM)
  // ==========================================================================
  template <typename N, N low, N high, bool wraps = false>
  struct StaticNumRange {
    template <typename O>
    struct Part : O {
      using Type = N;
      using Base = O;
      using Base::Base;
      using Base::get;
      using Base::set;

      static constexpr bool valid(N v) noexcept { return v >= low && v <= high; }
      static constexpr N clamp(N v) noexcept { return v < low ? low : v > high ? high : v; }

      void up(N step = 1) noexcept { set(clamp(get() + step)); }
      void down(N step = 1) noexcept { set(clamp(get() - step)); }
    };
  };

  // ==========================================================================
  // DEFAULT (Default Value Injection Modifier)
  // ==========================================================================
  template <typename T, T defaultValue>
  struct Default {
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

      template <typename... OO>
      constexpr Part(OO&&... oo) noexcept
          : Base{defaultValue, std::forward<OO>(oo)...} {}

      template <typename... OO>
      constexpr Part(T val, OO&&... oo) noexcept
          : Base{val, std::forward<OO>(oo)...} {}
    };
  };

  // ==========================================================================
  // SUGAR ALIASES
  // ==========================================================================
  using Text = Data<const char *>;
  using Bool = Data<bool>;
  using Int  = Data<int>;

  template <int v>  using StaticInt  = StaticData<int, v>;
  template <bool v> using StaticBool = StaticData<bool, v>;
  template <char v> using StaticChar = StaticData<char, v>;

  template <int* p>  using IntRef  = DataRef<int*, p>;
  template <bool* p> using BoolRef = DataRef<bool*, p>;
  template <char* p> using CharRef = DataRef<char*, p>;
  
  template <const CText& text> using StaticText = DataRef<const CText, text>;

}; // namespace oneData