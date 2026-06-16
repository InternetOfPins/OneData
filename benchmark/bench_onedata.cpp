/**
 * OneData standalone benchmark — no external dependencies beyond HAPI + OneData
 *
 * Compile-time probes  (-fsyntax-only -O0 -DTEST_SIZE=N -DTEST_XXX):
 *   TEST_BASELINE      oneData.h + HAPI include parse, no instantiation
 *   TEST_FLAT_CHAIN    DataDef<Tagged<0>,...,Tagged<N-1>> — N tagged int fields
 *   TEST_WATCH_STACK   DataDef<Watch<Watch<...<Data<int>>...>>> depth N (modifier stress)
 *   TEST_FIND_FIRST    FindFirst<Tag<0>> in flat chain of N
 *   TEST_FIND_LAST     FindFirst<Tag<N-1>> in flat chain of N
 *   TEST_FOREACH       forEach<DataTag> over flat chain of N
 *
 * Runtime probe  (-O2 -DTEST_RUNTIME -DTEST_SIZE=N [-DREPS=K]):
 *   sizeof for flat chain and watch stack, get/set, Watch change detection,
 *   StaticNumRange up/down, find latency, forEach throughput
 *
 * Note: baseline excludes chain instantiation.
 *   flat_chain - baseline  ≈ pure DataDef<N> collapse cost
 *   watch_stack vs flat    ≈ per-level modifier overhead
 *   find_last  - find_first ≈ FindFirst traversal cost per step
 */

#ifdef TEST_RUNTIME
  #include <chrono>
  #include <iostream>
#endif

#include <oneData/oneData.h>
using namespace oneData;

#ifndef TEST_SIZE
  #define TEST_SIZE 20
#endif
#ifndef REPS
  #define REPS 1000000
#endif

// ── shared types ─────────────────────────────────────────────────────────────

struct DataTag {};  // forEach<TagIs<DataTag>> visits all Tagged<I>

template<std::size_t I>
struct Tag {};      // find<TagIs<Tag<I>>> targets component I exactly

// Tagged<I>: Data<int> component with two tags — DataTag (group) + Tag<I> (identity)
template<std::size_t I>
struct Tagged : DataTag, Tag<I> {
    template<typename O>
    struct Part : Data<int>::template Part<O> {
        using Base = typename Data<int>::template Part<O>;
        using Base::Base;
    };
};

// DataDef<Tagged<0>, ..., Tagged<N-1>>
template<typename Seq> struct GenFlat;
template<std::size_t... Is>
struct GenFlat<std::index_sequence<Is...>> {
    using Type = DataDef<Tagged<Is>...>;
};

// Watch<Watch<...<Data<int>>...>> depth N — each level adds one 'watched' field
// sizeof should be (N+1)*sizeof(int)
template<int N, typename W>
struct WatchStack { using Type = typename WatchStack<N-1, Watch<W>>::Type; };
template<typename W>
struct WatchStack<0, W> { using Type = W; };

#ifdef TEST_RUNTIME
volatile int g_sink = 0;
#endif

// ── main ─────────────────────────────────────────────────────────────────────

int main() {

#if defined(TEST_BASELINE)
    (void)0;

#elif defined(TEST_FLAT_CHAIN)
    using Node = typename GenFlat<std::make_index_sequence<TEST_SIZE>>::Type;
    { Node node{}; (void)node; }

#elif defined(TEST_WATCH_STACK)
    using Stack = typename WatchStack<TEST_SIZE, Data<int>>::Type;
    using Node  = DataDef<Stack>;
    { Node node{}; (void)node; }

#elif defined(TEST_FIND_FIRST)
    using Node  = typename GenFlat<std::make_index_sequence<TEST_SIZE>>::Type;
    using API   = typename Node::Types::Head;
    using Comps = typename Node::Types::Tail;
    using Found = typename hapi::FindFirst<hapi::TagIs<Tag<0>>, Comps, API>::type;
    (void)static_cast<Found*>(nullptr);

#elif defined(TEST_FIND_LAST)
    using Node  = typename GenFlat<std::make_index_sequence<TEST_SIZE>>::Type;
    using API   = typename Node::Types::Head;
    using Comps = typename Node::Types::Tail;
    using Found = typename hapi::FindFirst<hapi::TagIs<Tag<TEST_SIZE-1>>, Comps, API>::type;
    (void)static_cast<Found*>(nullptr);

#elif defined(TEST_FOREACH)
    using Node = typename GenFlat<std::make_index_sequence<TEST_SIZE>>::Type;
    { Node node; hapi::forEach<hapi::TagIs<DataTag>>(node, [](auto&){}); }

#elif defined(TEST_RUNTIME)
    using SC = std::chrono::steady_clock;
    using NS = std::chrono::nanoseconds;
    constexpr int N    = TEST_SIZE;
    constexpr int reps = REPS;

    // ── sizeof metrics ────────────────────────────────────────────────────────
    using FlatNode  = typename GenFlat<std::make_index_sequence<N>>::Type;
    using WatchNode = DataDef<typename WatchStack<N, Data<int>>::Type>;

    std::cout << "sizeof(DataDef<Tagged<" << N << ">>):        "
              << sizeof(FlatNode)  << " bytes  (expected " << N*sizeof(int) << ")\n";
    std::cout << "sizeof(DataDef<Watch^" << N << "<Data<int>>>): "
              << sizeof(WatchNode) << " bytes  (expected " << (N+1)*sizeof(int) << ")\n";

    // ── basic Data<int> get/set ───────────────────────────────────────────────
    {
        DataDef<Data<int>> d{0};
        auto t0 = SC::now();
        for (int i = 0; i < reps; ++i) { d.set(i); g_sink += d.get(); }
        long long ns = std::chrono::duration_cast<NS>(SC::now() - t0).count();
        std::cout << "get+set  " << (double)ns / reps << " ns/op  [Data<int>]\n";
    }

    // ── Watch change detection ────────────────────────────────────────────────
    {
        DataDef<Watch<Data<int>>> w{0};
        auto t0 = SC::now();
        for (int i = 0; i < reps; ++i) {
            w.set(i);
            g_sink += w.changed();
            w.sync();
        }
        long long ns = std::chrono::duration_cast<NS>(SC::now() - t0).count();
        std::cout << "set+changed+sync  " << (double)ns / reps
                  << " ns/op  [Watch<Data<int>>]\n";
    }

    // ── StaticNumRange up/down ────────────────────────────────────────────────
    {
        DataDef<StaticNumRange<StaticRange<0, 10000>>, Data<int>> r{5000};
        auto t0 = SC::now();
        for (int i = 0; i < reps; ++i) {
            r.up(); r.down();
            g_sink += r.get();
        }
        long long ns = std::chrono::duration_cast<NS>(SC::now() - t0).count();
        std::cout << "up+down  " << (double)ns / reps
                  << " ns/op  [StaticNumRange<0,10000>, Data<int>]\n";
    }

    // ── forEach throughput on flat chain ─────────────────────────────────────
    {
        FlatNode node{};
        // warmup
        hapi::forEach<hapi::TagIs<DataTag>>(node, [](auto&){ ++g_sink; });
        auto t0 = SC::now();
        for (int i = 0; i < reps; ++i)
            hapi::forEach<hapi::TagIs<DataTag>>(node, [](auto&){ ++g_sink; });
        long long ns = std::chrono::duration_cast<NS>(SC::now() - t0).count();
        std::cout << "forEach  " << (double)ns / reps      << " ns/call"
                  << "  ("       << (double)ns / reps / N  << " ns/comp)"
                  << "  [N=" << N << "]\n";
    }

    // ── find<> latency (static_cast — should be ~0 ns) ───────────────────────
    {
        FlatNode node{};
        auto t0 = SC::now();
        for (int i = 0; i < reps; ++i)
            g_sink += hapi::find<hapi::TagIs<Tag<0>>>(node).get();
        long long ns = std::chrono::duration_cast<NS>(SC::now() - t0).count();
        std::cout << "find+get " << (double)ns / reps
                  << " ns/op  [find<Tag<0>> in N=" << N << "]\n";
    }

    return static_cast<int>(g_sink & 0);
#endif

    return 0;
}
