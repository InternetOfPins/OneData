/**
 * @file data.h
 * @author Rui Azevedo (neu-rah)
 * @brief Data API - HAPI data components
 * @date 2026-05-17
 * @contributor Grok (xAI) - architecture, cleanup & modern C++ patterns
*/

#pragma once

#include "menu/sys/base.h"
#include <type_traits>
#include <utility>

using CText = const char *;

// ====================== Base ======================
template <typename O = Nil>
struct DataAPI : O {
  using Base = O;
  using Base::Base;

  static constexpr bool changed() { return false; }
  static constexpr void sync() {}
};

template <typename... OO>
struct DataDef : APIOf<DataAPI<>, OO...> {
  using Base = APIOf<DataAPI<>, OO...>;
  using Base::Base;
};

// ====================== Owned Data ======================
template <typename T>
struct Data {
  template <typename O>
  struct Part : O {
    using Base = O;
    using Type = T;

    Type data{};

    template <typename V, typename... OO>
    constexpr Part(V&& value, OO &&...oo)
        : Base{std::forward<OO>(oo)...}
        , data{std::forward<V>(value)} {}

    template <typename... OO>
    constexpr Part(OO &&...oo)
        : Base{std::forward<OO>(oo)...} {}

    const Type& get() const { return data; }

    template <typename V>
    void set(V&& v) {data = std::forward<V>(v);}

    operator Type&()             { return data; }
    operator const Type&() const { return data; }
  };
};

// ====================== Data Reference ======================
template <typename T, T &Ref>
struct DataRef {
  template <typename O>
  struct Part : O {
    using Base = O;
    using Type = T;
    using Base::Base;
    static T &get() { return Ref; }
    static void set(const T &v) { Ref = v; }

    operator Type &() { return get(); }
    operator const Type &() const { return get(); }

    template <typename Out, typename Ctx>
    void print(Out &out, Ctx &ctx) {
      out.template fmtStart<Fmt::Data>(ctx);
      out.put(get());
      out.template fmtStop<Fmt::Data>(ctx);
      Base::print(out, ctx);
    }
  };
};

template <const CText &Text>
using TextRef = DataRef<const CText, Text>;

// ====================== Watch ======================
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

    constexpr bool changed() const { return get() != watched; }
    void sync() { watched = get(); }
  };
};

// ====================== Number Range ======================
template <typename N>
struct NumRange {
  template <typename O>
  struct Part : O {
    using Base = O;
    using Type = N;
    using NRP = std::decay_t<Type>;

    using Base::get;
    using Base::set;

    NRP m_low, m_high;
    bool wraps;

    template <typename... OO>
    constexpr Part(NRP low, NRP high, bool w, OO &&...oo)
        : Base{std::forward<OO>(oo)...}, m_low{low}, m_high{high}, wraps{w} {}

    constexpr bool valid(NRP v) const { return v >= m_low && v <= m_high; }
    constexpr NRP clamp(NRP v) const { return v < m_low ? m_low : v > m_high ? m_high : v; }

    constexpr NRP stepUp(NRP o,NRP s) {return m_high-o>=s?o+s:wraps?m_low:m_high;}
    constexpr NRP stepDown(NRP s,NRP o) {return o-m_low>=s?o-s:wraps?m_high:m_low;}

    void up(NRP s=1) {get()=stepUp(s,get());}
    void down(NRP s=1) {get()=stepDown(s,get());}

  };
};

// ====================== Static Number Range ======================
template <typename N, N Low, N High, bool Wraps = false>
struct StaticNumRange {
  template <typename O>
  struct Part : O {
    using Type = N;
    using Base = O;
    using Base::Base;
    using Base::get;
    using Base::set;

    static constexpr bool valid(N v) { return v >= Low && v <= High; }
    static constexpr N clamp(N v) { return v < Low ? Low : v > High ? High : v; }

    void up(N step = 1) { set(clamp(get() + step)); }
    void down(N step = 1) { set(clamp(get() - step)); }
  };
};

// ====================== Default ======================
template <typename T, T DefaultValue>
struct Default {
  template <typename O>
  struct Part : O {
    using Base = O;
    using Base::Base;

    template <typename... OO>
    constexpr Part(OO &&...oo)
        : Base{DefaultValue, std::forward<OO>(oo)...} {}

    template <typename... OO>
    constexpr Part(T value, OO &&...oo)
        : Base{value, std::forward<OO>(oo)...} {}
  };
};

// ====================== Aliases ======================
using Text = Data<const char *>;
using Bool = Data<bool>;
using Int  = Data<int>;

template <const CText &text> using StaticText=TextRef<text>;