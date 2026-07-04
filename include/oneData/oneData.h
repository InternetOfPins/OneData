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
 *  using PinPort = DataRef<&fake_hw>;
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

#ifdef __AVR__
  #include <string.h>
  #include <stdlib.h>
  #include <stdint.h>
#else
  #include <cstring>
  #include <cstdlib>
  #include <cstdint>
#endif
#include <hapi/hapi.h>
using hapi::APIOf;

namespace oneData {
  using CText = const char *;

  // BASE DATA API --------------------------------------------------------------
  template <typename O = hapi::Nil>
  struct DataAPI : O {
    using Base = O;
    using Base::Base;

    static constexpr bool changed() noexcept { return false; }
    static constexpr void sync() noexcept {}
    template<typename Out> static constexpr void print(Out&) noexcept {}
    template<typename Out,typename Ctx> static constexpr void printItem(Out&,Ctx&) noexcept {}
    template<typename T> T&       operator[](std::size_t) noexcept       { __builtin_unreachable(); }
    template<typename T> const T& operator[](std::size_t) const noexcept { __builtin_unreachable(); }
  };

  template <typename... OO> using DataDef = APIOf<DataAPI<>, OO...>;

  // STATIC DATA (Compile-Time Constant - Flash/Immediate - 0 Bytes RAM) --------
  /// @brief StaticData<42> / StaticData<true> / StaticData<'A'> — 0 bytes RAM, Type from value
  template<auto _value>
  struct StaticData {
    using Type = decltype(_value);
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;
      using Type = decltype(_value);

      static constexpr Type get() noexcept { return _value; }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }

      operator Type() const noexcept { return get(); }
    };
  };

  // static text --
  /// @brief compile-time string literal bound to a const char* const* pointer; get() returns *text_ptr
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
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }

      static constexpr Type get() noexcept { return *text_ptr; }
    };
  };

  // Multi-language static text.
  //
  // Language is selected at the outer type level — no plumbing needed at each use site:
  //   MultiLangText::current = 2;           // switch language globally
  //
  // Text data is bound one level in:
  //   static const char* const spd[] = {"Speed", "Vitesse", "Velocidade"};
  //   using SpeedLabel = MultiLangText::Of<spd>;  // HAPI component
  /// @brief selectable multi-language string array; current language set via MultiLangText::current
  struct MultiLangText {
    static inline uint8_t current = 0;

    template<const char* const* texts>
    struct Of {
      using Type = const char*;
      template<typename O>
      struct Part : O {
        using Base = O;
        using Type = const char*;
        using Base::Base;

        static const char* get() noexcept { return texts[current]; }

        template<typename Out>
        void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
        template<typename Out,typename Ctx>
        void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }
      };
    };
  };

  // DATA (Owned RAM Storage) ---------------------------------------------------
  /// @brief mutable value with RAM storage; get() returns reference, set() writes, print() emits to output
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
          : Base{std::forward<OO>(oo)...}, data(static_cast<std::decay_t<Type>>(v)) {}

      template <typename... OO>
      constexpr Part(OO&&... oo) noexcept : Base{std::forward<OO>(oo)...} {}

      const std::decay_t<Type>& get() const noexcept { return data; }

      template <typename V>
      void set(V&& v) noexcept { data = std::forward<V>(v); }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }

      operator std::decay_t<Type>&() noexcept             { return data; }
      operator const std::decay_t<Type>&() const noexcept { return data; }
    };
  };

  // DATA specialization for char arrays — supports set(const char*) via strncpy.
  // Use Data<char[N]> wherever a fixed-length string buffer is needed with
  // uniform get()/set() access (e.g. TextField buffer, web form fields).
  template <size_t N>
  struct Data<char[N]> {
    using Type = char[N];
    template <typename O>
    struct Part : O {
      using Base = O;
      using Base::Base;

      char data[N]{};

      const char* get() const noexcept { return data; }
      char*       get()       noexcept { return data; }

      void set(const char* s) noexcept {
        strncpy(data, s, N - 1);
        data[N - 1] = '\0';
      }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(data); Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(data); Base::printItem(out,ctx); }

      operator const char*() const noexcept { return data; }
      operator       char*()       noexcept { return data; }

      template<typename Nav,typename P>
      bool setStr(Nav&,const char* s,P p) noexcept {
        if(p.len==0) { set(s); return true; }
        return false;
      }
    };
  };

  // DATA REF (Unified External Pointer/Reference - 0 Bytes RAM) ---------------
  /// @brief reference to an external variable; get/set forward directly to *address (0 bytes RAM)
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
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }

      operator auto&() noexcept { return get(); }
      operator std::remove_cv_t<Type>() const noexcept { return get(); }
    };
  };

  // DATA FN (External get/set Functions - Pins, ISR-shared vars, sensors...) ---
  /// @brief storage backed by user get()/set() static functions instead of an owned value.
  /// Src just needs `static Type get()` (and `static void set(Type)` if writable) — a pin
  /// terminal (OnePin already exposes both, via Mask<>) works directly as Src with no
  /// adapter; a hand-written struct works the same way for volatile globals, registers,
  /// sensor reads, etc.
  template <typename Src>
  struct DataFn {
    using Type = decltype(Src::get());
    template <typename O>
    struct Part : O {
      using Base = O;
      using Type = decltype(Src::get());
      using Base::Base;

      static Type get() noexcept { return Src::get(); }
      static void set(Type v) noexcept { Src::set(v); }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }

      operator Type() const noexcept { return get(); }
    };
  };

  // SUGAR ALIASES --------------------------------------------------------------
  using Text = Data<const char *>;
  using Bool = Data<bool>;
  using Int  = Data<int>;

  //================================================================================--

  /// @brief change-tracking modifier: changed() returns true when value differs from last sync()
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

  /// @brief bidirectional value conversion between an underlying raw storage W and a
  /// displayed/edited Type (e.g. a 0-1023 ADC reading shown/edited as 0.0-5.0 volts).
  /// Policy needs `static Display toDisplay(Raw)`; `static Raw toRaw(Display)` is only
  /// required if the field is actually edited (set() called) — a read-only translated
  /// field (no NumField/EditField above it) never instantiates set(), so a Policy with
  /// only toDisplay is enough for a monitor-only field.
  template <typename W, typename Policy>
  struct Translated {
    using Type = decltype(Policy::toDisplay(std::declval<typename W::Type>()));
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = decltype(Policy::toDisplay(std::declval<typename Base::Type>()));
      using Base::Base;

      Type get() const noexcept { return Policy::toDisplay(Base::get()); }
      void set(Type v) noexcept { Base::set(Policy::toRaw(v)); }

      // NOTE: deliberately does NOT forward to Base::print()/printItem() — Base
      // eventually reaches a Data<T>/DataFn/DataRef terminal that would print the
      // untranslated raw value too, double-printing (same class of bug fixed in
      // TextField::PartEnd, see project history 2026-06-24). This is the sole
      // print source for a translated field.
      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { out.put(get()); }
    };
  };

  /// @brief fixed N-decimal-place print formatting for a floating-point W — first of what
  /// should be a family of small, single-purpose numeric print preferences (width/padding,
  /// sign display, etc. can each be their own sibling component later, same shape).
  /// Same idiom as Translated: hijacks print()/printItem() instead of forwarding to Base's,
  /// since it's replacing (not adding to) how the value is rendered. Digit extraction is done
  /// manually (no snprintf/%f) so it works on AVR without needing printf float-support linked
  /// in (-Wl,-u,vfprintf -lprintf_flt -lm), which isn't enabled by default.
  template <unsigned N, typename W>
  struct Decimals {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::set;

      template<typename Out>
      void print(Out& out) const noexcept { put(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { put(out); }

    private:
      template<typename Out>
      void put(Out& out) const noexcept {
        Type v = get();
        if (v < 0) { out.put('-'); v = -v; }
        long scale = 1;
        for (unsigned i=0; i<N; i++) scale *= 10;
        long whole = (long)v;
        long frac  = (long)((v - (Type)whole) * (Type)scale + (Type)0.5);
        if (frac >= scale) { whole++; frac -= scale; }  // carry from rounding up
        out.put(whole);
        if constexpr (N > 0) {
          out.put('.');
          long div = scale/10;
          for (unsigned i=0; i<N; i++) {
            out.put((char)('0' + (frac/div)%10));
            div /= 10;
          }
        }
      }
    };
  };

  /// @brief erases set() from W — read-only view. Private-inherits W::Part<O> and re-exposes
  /// only get()/print()/printItem(); set() (and any mutable access) is simply not brought back
  /// into scope, so it's not just unused but genuinely inaccessible — anything above ReadOnly
  /// that tries to call set() (e.g. StaticNumRange::up()/down(), which call set() internally)
  /// fails to compile, catching "editable UI wired to a read-only value" at compile time.
  template <typename W>
  struct ReadOnly {
    using Type = typename W::Type;
    template <typename O>
    struct Part : private W::template Part<O> {
      using Base = typename W::template Part<O>;
    public:
      using Base::Base;
      using Type = typename Base::Type;
      using Base::get;
      template<typename Out>
      void print(Out& out) const noexcept { Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { Base::printItem(out,ctx); }
    };
  };

  /// @brief compile-time range descriptor — deferred type resolution --
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

  /// @brief runtime-configurable numeric range; clamps get/set to [low, high] with optional step
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

      template<typename Nav,typename P>
      bool setStr(Nav&,const char* s,P p) noexcept {
        if(p.len==0) {
          if constexpr(std::is_floating_point<NRP>::value)
            set(clamp(static_cast<NRP>(strtod(s,nullptr))));
          else
            set(clamp(static_cast<NRP>(strtol(s,nullptr,10))));
          return true;
        }
        return Base::template setStr(*this,s,p);
      }
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

      template<typename Nav,typename P>
      bool setStr(Nav&,const char* s,P p) noexcept {
        if(p.len==0) {
          if constexpr(std::is_floating_point<Type>::value)
            set(clamp(static_cast<Type>(strtod(s,nullptr))));
          else
            set(clamp(static_cast<Type>(strtol(s,nullptr,10))));
          return true;
        }
        return Base::template setStr(*this,s,p);
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

  // BTREC (BT/GATT record tag) --------------------------------------------------
  /// @brief tags a data component with a BT record id; same composition shape as
  /// Watch/Default (wraps W, doesn't touch nav). Mirrors the value out through
  /// Out::btWrite<Id>() — picked up by a matching oneOutput::BtOut<Ble,Id> if one is
  /// composed into the Out chain, a no-op (see OutAPI::btWrite) otherwise, so an
  /// untagged/unwired field costs nothing.
  ///
  /// If W wraps Watch (i.e. has changed()/sync()), printItem() only mirrors+syncs when
  /// changed() — avoids pushing/notifying on every redraw. Without Watch it mirrors
  /// unconditionally. print() (the const, non-nav path) always mirrors unconditionally,
  /// since it can't call a mutating sync().
  ///
  /// BTRec<Watch<Default<Data<int>,0>>, 3>  — composable freely, same as Watch/Default.
  /// Only matters to keep W reference-backed (DataRef/DataFn) instead of owned (Data<T>)
  /// when BT runs its own independent nav alongside another nav over the same field tree
  /// — two navs both owning the same Data<T> would duplicate it. If BT is the only
  /// output/nav, owned Data<T> is fine too; the constraint is about multi-nav sharing,
  /// not BTRec itself. See project_bt_menu_output memory for the fuller nav discussion.
  ///
  /// Inbound direction (peer write -> set()) is NOT wired yet — reading back
  /// Ble::char_written(Id)/char_read(Id,...) into set() needs a text/binary parse-back
  /// story (see setStr() used elsewhere) and is deferred; see project_bt_menu_output memory.
  template<typename W, uint16_t Id>
  struct BTRec {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::set;
      static constexpr uint16_t btId = Id;

    private:
      template<typename T, typename = void>
      struct _HasChanged : std::false_type {};
      template<typename T>
      struct _HasChanged<T, std::void_t<decltype(std::declval<T&>().changed())>> : std::true_type {};

    public:
      template<typename Out>
      void print(Out& out) const noexcept {
        out.template btWrite<Id>(get());
        Base::print(out);
      }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept {
        if constexpr (_HasChanged<Base>::value) {
          if (Base::changed()) { out.template btWrite<Id>(get()); Base::sync(); }
        } else {
          out.template btWrite<Id>(get());
        }
        Base::printItem(out,ctx);
      }
    };
  };

}; // namespace oneData
