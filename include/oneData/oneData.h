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

  /// @brief suppresses II...'s own print()/printItem() from the normal output chain,
  /// redirecting both straight to whatever comes next (I) instead — e.g. wrap a
  /// description/tooltip Data component in Hidden<...> to keep it out of the item's
  /// normal visible rendering. Ctx is a template parameter (not any concrete type) —
  /// print()/printItem() here are pure blind forwarding (I::print(out)/I::printItem(out,ctx)),
  /// never inspecting ctx's value, so this compiles and behaves correctly for *any* Ctx,
  /// not just one specific caller's notion of it.
  ///
  /// Deliberately narrow: this is only the data-chain half. A caller that also wants a
  /// pull-based "render II... on demand" escape hatch (used by oneMenu for secondary/
  /// footer-device content) or needs to route item-navigation calls (nav()) needs a
  /// richer wrapper on top — those genuinely interpret their Ctx/Nav arguments (e.g.
  /// oneMenu::Ctx::operator bool(), real path/selection semantics), unlike print/
  /// printItem here, so they don't belong at this generic a level. See
  /// oneMenu::Hidden<II...> (item.h), which derives its own Part<I> from this one and
  /// adds exactly those two oneMenu-specific pieces on top — not a duplicate
  /// implementation, the real split between what's genuinely data-generic and what
  /// isn't.
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
      // untranslated raw value too, double-printing. This is the sole print
      // source for a translated field.
      template<typename Out>
      void print(Out& out) const noexcept { out.put(get()); }
      template<typename Out,typename Ctx>
      void printItem(Out& out,Ctx&) noexcept { out.put(get()); }
    };
  };

  /// @brief linear-remap Policy for Translated — e.g. a 0-100 percent field driving a
  /// 0-255 PWM duty cycle, replacing a runtime `analogWrite(pin, map(v,0,100,0,255))`
  /// call at the write site with the transform living on the field itself. Same integer
  /// truncation behavior as Arduino's map() (division on whatever Type the ranges are —
  /// pass floating-point NTTPs if fractional precision is wanted).
  template<auto inLo, auto inHi, auto outLo, auto outHi>
  struct MapPolicy {
    static constexpr auto toDisplay(decltype(inLo) v) noexcept {
      return (v-inLo)*(outHi-outLo)/(inHi-inLo)+outLo;
    }
    static constexpr auto toRaw(decltype(outLo) v) noexcept {
      return (v-outLo)*(inHi-inLo)/(outHi-outLo)+inLo;
    }
  };

  /// @brief Translated<W, MapPolicy<...>> sugar — the ranges are compile-time NTTPs
  /// ("static"), not runtime-configurable; that's deliberate, same static-only rule as
  /// Row/Rows's own partition parameters.
  template<auto inLo, auto inHi, auto outLo, auto outHi, typename W>
  using MapRange = Translated<W, MapPolicy<inLo,inHi,outLo,outHi>>;

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

  /// @brief like Decimals<N,W> but N is a *runtime* value, read live from a pointer
  /// to an external integral variable (nPtr) at print time — not pushed via a
  /// setter/event. Lets one field's edited value (e.g. a "decimal places" field)
  /// control how a *different* field renders, without needing a value-changed event
  /// dispatch (OneMenu's EventDispatch only fires Enter/Exit/Focus/Blur — no
  /// "value changed" event exists to push through). Same AVR-safe manual
  /// digit-extraction as Decimals<N,W>, N substituted at runtime via *nPtr.
  /// changed()/sync() additionally track *nPtr's own last-seen value, so a redraw
  /// fires even when only the *formatting* source changed, not the wrapped value
  /// itself (mirrors Watch<W>'s own get()!=watched idiom, widened to a second
  /// tracked value). Requires W to already compose Watch<> (for Base::changed()/
  /// sync() to exist) — same implicit precondition Decimals<N,W> already has.
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

  /// @brief printf-style print formatting for a wrapped Data-chain W (e.g. "%03d" for
  /// leading-zero integers) — sibling of Decimals<N,W> in the same "numeric print
  /// preference" family, for the case where a fixed decimal-place count isn't the
  /// right tool and an arbitrary C format string is. fmt is a compile-time NTTP, not
  /// a runtime argument: a string literal can't be a template parameter directly
  /// (pre-C++20), so this uses the same pointer-to-pointer indirection StaticText<>/
  /// MultiLangText::Of<> already use elsewhere in this file — declare a named
  /// `static constexpr CText myFmt{"%03d"};` and pass `&myFmt`.
  ///
  /// Same idiom as Decimals/Translated: hijacks print()/printItem() instead of
  /// forwarding to Base's, since it's replacing (not adding to) how the value is
  /// rendered — Base would otherwise reach a Data<T>/DataFn/DataRef terminal that
  /// prints the unformatted raw value too, double-printing.
  ///
  /// AVR CAVEAT: integer format specifiers (%d/%u/%x/%03d/...) work out of the box —
  /// avr-libc's default (non-float) vfprintf handles them with no extra linking.
  /// Floating-point specifiers (%f/%e/%g) additionally need
  /// `-Wl,-u,vfprintf -lprintf_flt -lm` linked, which most AVR toolchain setups don't
  /// enable by default (same reasoning Decimals<N,W> exists to sidestep entirely, via
  /// manual digit extraction instead of snprintf/%f) — prefer Decimals<N,W> over
  /// Printf<"%f",W> for AVR-safe fixed-decimal float printing.
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

  /// @brief like Printf<fmt_ptr,W,sz> but fmt is a runtime const char*, fixed at
  /// construction — for the case where the format string itself is a real runtime
  /// value (e.g. supplied by the sketch author as a plain string literal argument),
  /// not requiring the fmt_ptr pointer-to-pointer indirection Printf needs to work
  /// around the "string literal isn't a legal NTTP" restriction. Same snprintf-
  /// into-a-local-buffer rendering as Printf — same AVR caveat applies unchanged
  /// (integer specifiers work with avr-libc's default vfprintf; float specifiers
  /// need -Wl,-u,vfprintf -lprintf_flt -lm linked, prefer Decimals<N,W> instead for
  /// AVR-safe float printing). Not live-reread like RuntimeDecimals<nPtr,W> — the
  /// format string doesn't change while the program runs, only the wrapped value
  /// does, so there's no separate "format source changed" case to track.
  ///
  /// The forwarding constructor below matters beyond this component alone: any
  /// component composed *before* this one in an outer chain (e.g. `ItemDef<
  /// AsLabel<Text>, ..., NumField<Range,AsField<RuntimePrintf<W>>>, AsUnit<Text>,
  /// ...>`) reaches its own real constructor by threading remaining brace-init
  /// arguments hand-to-hand down through every intermediate `using Base::Base;`
  /// level — hapi::Chain's constructor inheritance is flat across the WHOLE
  /// composed chain, not "peel one arg per level" on its own. A component with
  /// its OWN extra runtime arg (this one's `fmt`) MUST consume exactly its own
  /// arg and forward the rest via `Base{...}`, or anything declared *later* in
  /// the outer chain (like that `AsUnit<Text>`) loses its own path to its real
  /// constructor entirely — confirmed via a real compile error the first time
  /// this used a fixed single-arg constructor instead (no forwarding path),
  /// composing this through `NumFieldDef` for `fieldFormat.ino`'s port.
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

  /// @brief swaps which physical key increases vs decreases a wrapped
  /// range's value — wraps anything exposing up(step)/down(step) (e.g.
  /// StaticNumRange<...>/NumRange<T>) and forwards to the OTHER one when
  /// `Inverted` is true. Default `false` (not inverted) is a deliberate
  /// choice, not an arbitrary one: it matches AM4's own real shipped
  /// default (`config::invertFieldKeys = false`, confirmed against AM4's
  /// actual source — `menuBase.cpp`'s `Menu::defaultOptions` — the
  /// `config` struct's own constructor default of `true` is overridden
  /// there) — natural direction, consistent with plain (non-edit-mode)
  /// Up/Down semantics elsewhere in this codebase. AM4's own comment on
  /// the equivalent code ("by default they are inverted.. now buttons and
  /// joystick have to flip them") suggests the inverted case exists to
  /// compensate for specific input devices (e.g. a rotary encoder whose
  /// physical rotation sense doesn't match logical up/down), not as a
  /// universal default. Compose `InvDir<Range,true>` in place of a plain
  /// `Range` (e.g. `NumField<InvDir<StaticNumRange<...>,true>,
  /// AsField<...>>`) to opt into the flipped direction for a specific
  /// field.
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
