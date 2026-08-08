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
  #include <stdio.h>   // snprintf — Printf<fmt,W> below; avr-libc's default vfprintf
                        // handles integer specifiers fine, no extra linking needed
#else
  #include <cstring>
  #include <cstdlib>
  #include <cstdint>
  #include <cstdio>
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

  /// Suppresses II...'s own print()/printItem(), redirecting both to whatever comes next (I).
  template<typename... II>
  struct Hidden {
    struct End {
      template<typename O>
      struct Part:O {
        using Base=O;
        using Base::Base;
        template<typename Out> static void print(Out&) noexcept {}
        template<typename Out,typename Ctx> static void printItem(Out&,Ctx&) noexcept {}
      };
    };
    template<typename I>
    struct Part:hapi::Chain<II...,End>::template Part<I> {
      using Base=typename hapi::Chain<II...,End>::template Part<I>;
      using Base::Base;
      // skip II... in flat output chain
      template<typename Out>
      void print(Out& out) const noexcept {I::print(out);}
      // skip II... in printItem chain
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept {I::printItem(out,ctx);}
    };
  };

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

  /// Compile-time language table, id-indexed: table[lang][id]. Zero RAM, zero runtime cost.
  template<const char* const* const* table, int nLangs>
  struct FlashLangSrc {
    static const char* text(int id, uint8_t lang) noexcept {
      return table[lang < nLangs ? lang : 0][id];
    }
  };

  /// Id-indexed multilingual text, resolved via Src::text(id, lang); language shared
  /// with MultiLangText::current. Src requires `static const char* text(int id, uint8_t lang)`.
  template<int id, typename Src>
  struct IdText {
    using Type = const char*;
    template<typename O>
    struct Part : O {
      using Base = O;
      using Type = const char*;
      using Base::Base;

      static const char* get() noexcept { return Src::text(id, MultiLangText::current); }

      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); Base::print(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx& ctx) noexcept { out.put(get()); Base::printItem(out,ctx); }
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
  /// Storage backed by user get()/set() static functions instead of an owned value.
  /// Src requires `static Type get()` (and `static void set(Type)` if writable).
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
      using Base::sync;  // keep ItemAPI's inherited sync(Out&) template
                         // reachable — this Part's own sync() (0-arg) would
                         // otherwise hide it via ordinary C++ name hiding,
                         // breaking IItem's virtual sync(IOut&) override
                         // (item.h) for any chain built through IItemDef.

      std::remove_reference_t<Type> watched{};

      constexpr bool changed() const noexcept { return get() != watched; }
      void sync() noexcept { watched = get(); }
    };
  };

  /// Change-tracking modifier: changed() returns true after any set() since the last sync()
  /// (a dirty bit, not a value-diff — writing back the same value still counts as changed).
  template <typename W>
  struct Dirty {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::sync;  // keep ItemAPI's inherited sync(Out&) template reachable — same
                         // reason as Watch<>'s own identical line just above: this Part's
                         // own sync() (0-arg) would otherwise hide it via name hiding.

      // Starts dirty — matches AM4's prompt::dirty{true} "needs first draw", and unlike
      // Watch<>'s watched{} default-init, this is correct regardless of what the wrapped
      // value's own initial state happens to be (Watch<> only starts "changed" if the
      // real initial value happens to differ from Type's default-constructed value).
      bool m_dirty{true};

      // Must actually intercept set() (Watch<> doesn't — it just re-exposes Base::set and
      // compares lazily in changed()) — templated+forwarding so this one signature works
      // uniformly against every real set() shape in this file: Data<T>'s own templated
      // forwarding-ref set(V&&), and DataRef<>/DataFn<>'s static void set(Type). Can't be
      // static itself even when wrapping a static Base::set — it has to touch per-instance
      // m_dirty; ordinary name-hiding picks this non-static override over a static base one.
      template<typename V>
      void set(V&& v) noexcept { Base::set(std::forward<V>(v)); m_dirty=true; }

      constexpr bool changed() const noexcept { return m_dirty; }
      void sync() noexcept { m_dirty=false; }
    };
  };

  /// Bidirectional value conversion between raw storage W and a displayed/edited Type.
  /// Policy requires `static Display toDisplay(Raw)`; `static Raw toRaw(Display)` is only
  /// needed if the field is edited (set() called).
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
      // untranslated raw value too, double-printing. This is the sole print
      // source for a translated field.
      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { out.put(get()); }
    };
  };

  /// Bidirectional value conversion like Translated, but getFn/setFn are supplied directly
  /// as NTTPs instead of a named Policy type. Both are mandatory; getFn(Raw)->Display is
  /// applied on get(), setFn(Display)->Raw is applied before Base::set().
  template <auto getFn, auto setFn>
  struct Trans {
    template <typename W>
    struct Value {
      using Type = decltype(getFn(std::declval<typename W::Type>()));
      template <typename O>
      struct Part : W::template Part<O> {
        using Base = typename W::template Part<O>;
        using Type = decltype(getFn(std::declval<typename Base::Type>()));
        using Base::Base;

        Type get() const noexcept { return getFn(Base::get()); }
        void set(Type v) noexcept { Base::set(setFn(v)); }

        // NOTE: deliberately does NOT forward to Base::print()/printItem() — same
        // reasoning as Translated: Base eventually reaches a raw terminal that would
        // double-print the untranslated value.
        template<typename Out>
        void print(Out& out) const noexcept { out.put(get()); }
        template<typename Out,typename Ctx>
        void printItem(Out& out,Ctx&) noexcept { out.put(get()); }
      };
    };
  };

  /// Linear-remap Policy for Translated, mapping [inLo,inHi] to [outLo,outHi].
  template<auto inLo, auto inHi, auto outLo, auto outHi>
  struct MapPolicy {
    static constexpr auto toDisplay(decltype(inLo) v) noexcept {
      return (v-inLo)*(outHi-outLo)/(inHi-inLo)+outLo;
    }
    static constexpr auto toRaw(decltype(outLo) v) noexcept {
      return (v-outLo)*(inHi-inLo)/(outHi-outLo)+inLo;
    }
  };

  /// Translated<W, MapPolicy<...>> sugar; ranges are compile-time NTTPs, not runtime-configurable.
  template<auto inLo, auto inHi, auto outLo, auto outHi, typename W>
  using MapRange = Translated<W, MapPolicy<inLo,inHi,outLo,outHi>>;

  /// Fixed N-decimal-place print formatting for a floating-point W. Digit extraction is
  /// done manually (no snprintf/%f), so it works on AVR without float printf support linked.
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

  /// Like Decimals<N,W> but N is read live from a pointer (nPtr) at print time.
  /// changed()/sync() also track *nPtr so a redraw fires when only the format source
  /// changes. Requires W to already compose Watch<>.
  template <auto nPtr, typename W>
  struct RuntimeDecimals {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::set;

      std::remove_reference_t<decltype(*nPtr)> lastN{*nPtr};

      constexpr bool changed() const noexcept { return Base::changed() || *nPtr != lastN; }
      void sync() noexcept { Base::sync(); lastN = *nPtr; }

      template<typename Out>
      void print(Out& out) const noexcept { put(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { put(out); }

    private:
      template<typename Out>
      void put(Out& out) const noexcept {
        Type v = get();
        if (v < 0) { out.put('-'); v = -v; }
        unsigned n = (unsigned)*nPtr;
        long scale = 1;
        for (unsigned i=0; i<n; i++) scale *= 10;
        long whole = (long)v;
        long frac  = (long)((v - (Type)whole) * (Type)scale + (Type)0.5);
        if (frac >= scale) { whole++; frac -= scale; }  // carry from rounding up
        out.put(whole);
        if (n > 0) {
          out.put('.');
          long div = scale/10;
          for (unsigned i=0; i<n; i++) {
            out.put((char)('0' + (frac/div)%10));
            div /= 10;
          }
        }
      }
    };
  };

  /// Printf-style print formatting for a wrapped Data-chain W (e.g. "%03d"). fmt is a
  /// compile-time NTTP passed via pointer-to-pointer indirection: declare a named
  /// `static constexpr CText myFmt{"%03d"};` and pass `&myFmt`. On AVR, floating-point
  /// specifiers need printf float support linked; prefer Decimals<N,W> otherwise.
  template <const char* const* fmt_ptr, typename W, unsigned sz = 16>
  struct Printf {
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
        char buf[sz];
        snprintf(buf, sz, *fmt_ptr, get());
        for (const char* p = buf; *p; ++p) out.put(*p);
      }
    };
  };

  /// Like Printf<fmt_ptr,W,sz> but fmt is a runtime const char*, fixed at construction.
  /// The constructor must consume its own `fmt` arg and forward the rest via `Base{...}`
  /// so components declared later in the chain still reach their own constructors.
  template <typename W, unsigned sz = 16>
  struct RuntimePrintf {
    using Type = typename W::Type;
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::get;
      using Base::set;

      const char* fmt;

      template<typename... Args>
      constexpr Part(const char* f, Args&&... args)
        : Base{std::forward<Args>(args)...}, fmt(f) {}

      template<typename Out>
      void print(Out& out) const noexcept { put(out); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { put(out); }

    private:
      template<typename Out>
      void put(Out& out) const noexcept {
        char buf[sz];
        snprintf(buf, sz, fmt, get());
        for (const char* p = buf; *p; ++p) out.put(*p);
      }
    };
  };

  /// Swaps which physical key increases vs decreases a wrapped range's value; wraps
  /// anything exposing up(step)/down(step) and forwards to the other one when Inverted is true.
  template <typename W, bool Inverted = false>
  struct InvDir {
    // No outer `using Type = typename W::Type;` — W here is a Range-shaped
    // component (StaticNumRange<...>/NumRange<T>), which itself has no
    // outer Type either (only its own nested Part<O> does, matching
    // whatever it wraps); Type is only ever needed inside Part<O> below.
    template <typename O>
    struct Part : W::template Part<O> {
      using Base = typename W::template Part<O>;
      using Type = typename Base::Type;
      using Base::Base;
      using Base::get;
      using Base::set;
      using Base::valid;
      using Base::clamp;

      void up(Type step = 1) noexcept {
        if constexpr (Inverted) Base::down(step); else Base::up(step);
      }
      void down(Type step = 1) noexcept {
        if constexpr (Inverted) Base::up(step); else Base::down(step);
      }
    };
  };

  /// Erases set() from W — read-only view; only get()/print()/printItem() remain accessible.
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
      // Plain value accessors (no formatting/rendering concern — OneData
      // must never depend on OneMenu, see the dependency map memory) so a
      // higher layer (oneMenu::NumField) can expose the field's own range
      // for real client-facing rendering (e.g. a web slider's min/max).
      static constexpr Type low() noexcept { return RangeDesc::template low<Type>(); }
      static constexpr Type high() noexcept { return RangeDesc::template high<Type>(); }

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
  /// Wraps a data component, injecting a compile-time default value.
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
  /// Tags a data component with a BT record id; mirrors the value out through
  /// Out::btWrite<Id>(). If W wraps Watch<>, printItem() only mirrors+syncs when changed().
  /// Inbound direction (peer write -> set()) is not wired yet.
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
        // Only mirror+sync on a genuine draw pass (None=full, Update=partial) — Changed/Sync/
        // Measure are zero-side-effect probes (see nav.h's changed(out): a Gate-suppressed
        // printTo() dry-run just to ask "did anything change"). sync() isn't a hardware write,
        // so Gate can't protect it the way it protects put()/nl() — calling it unconditionally
        // here would consume/clear Watch's changed() flag during the probe itself, before the
        // real redraw pass ever runs, so the real pass then sees changed()==false and the
        // screen never visibly updates. Same class of bug as UseEditCursorFmt's own lockMode
        // guard (formats.h) and the "Gate-suppressed-writes" bug class (see project memory).
        auto lm = out.lockMode();
        bool isDrawPass = (lm==decltype(lm)::None || lm==decltype(lm)::Update);
        if constexpr (_HasChanged<Base>::value) {
          if (isDrawPass && Base::changed()) { out.template btWrite<Id>(get()); Base::sync(); }
        } else if (isDrawPass) {
          out.template btWrite<Id>(get());
        }
        Base::printItem(out,ctx);
      }
    };
  };

}; // namespace oneData
