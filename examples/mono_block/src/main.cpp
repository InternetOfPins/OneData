// OneData mono_block tests
//
// Verifies OneData components under the mono_block HAPI chain:
//   1. Data<T>         — owned RAM storage, get/set
//   2. DataRef<T*,p>   — zero-RAM external pointer, get/set
//   3. Watch<Data<T>>  — change tracking composable over Data
//   4. StaticNumRange  — compile-time boundaries, up/down
//   5. Default<W,val>  — default value injection
//   6. find<Q>         — locate a tagged component in DataDef
//   7. query<TagIs<Q>> — tag detection in DataDef::Types

#include <cassert>
#include <iostream>
using namespace std;

#include <hapi/hapi.h>
#include <oneData/oneData.h>
using namespace hapi;
using namespace oneData;

// ─── Test 1: Data<int> owned storage ─────────────────────────────────────────

void test_data_owned() {
  DataDef<Data<int>> d{42};
  assert(d.get() == 42);
  d.set(7);
  assert(d.get() == 7);
  cout << "PASS test_data_owned\n";
}

// ─── Test 2: DataRef — zero-RAM external pointer ─────────────────────────────

static volatile int hw_reg{};

void test_data_ref() {
  using PinPort = DataRef<volatile int*, &hw_reg>;
  DataDef<PinPort> pin;
  pin.set(0xFF);
  assert(hw_reg == 0xFF);
  assert(pin.get() == 0xFF);
  cout << "PASS test_data_ref\n";
}

// ─── Test 3: Watch<Data<int>> change tracking ────────────────────────────────

void test_watch() {
  // Watch::watched is zero-init; sync() aligns it with current data.
  DataDef<Watch<Data<int>>> w{10};
  assert(w.changed());   // data=10, watched=0 → dirty immediately after non-zero init
  w.sync();
  assert(!w.changed());  // now aligned
  w.set(20);
  assert(w.changed());
  w.sync();
  assert(!w.changed());
  cout << "PASS test_watch\n";
}

// ─── Test 4: StaticNumRange compile-time range ────────────────────────────────

void test_static_num_range() {
  DataDef<StaticNumRange<StaticRange<0, 100>>, Data<int>> pct{50};
  pct.up();
  assert(pct.get() == 51);
  pct.down();
  assert(pct.get() == 50);
  // clamp at boundaries
  DataDef<StaticNumRange<StaticRange<0, 100>>, Data<int>> top{100};
  top.up();
  assert(top.get() == 100);
  cout << "PASS test_static_num_range\n";
}

// ─── Test 5: Default<Data<int>, val> ─────────────────────────────────────────

void test_default_value() {
  DataDef<Default<Data<int>, 99>> d;
  assert(d.get() == 99);
  d.set(1);
  assert(d.get() == 1);
  cout << "PASS test_default_value\n";
}

// ─── Test 6: find<TagIs<Q>> on a tagged DataDef ───────────────────────────────

struct DirtyTag {};

struct DirtyTracker : DirtyTag {
  template<typename O>
  struct Part : O {
    using Base = O; using Base::Base;
    bool dirty = false;
    template<typename V> void set(V&& v) { dirty = true; Base::set(std::forward<V>(v)); }
  };
};

void test_find_tagged() {
  DataDef<DirtyTracker, Data<int>> d{0};
  auto& dt = hapi::find<TagIs<DirtyTag>>(d);
  assert(!dt.dirty);
  d.set(5);
  assert(dt.dirty);
  cout << "PASS test_find_tagged\n";
}

// ─── Test 7: query tag detection ─────────────────────────────────────────────

void test_query_tag() {
  static_assert(query<TagIs<DirtyTag>, DataDef<DirtyTracker, Data<int>>::Types>,
    "DirtyTag should be detectable in DataDef types");
  static_assert(!query<TagIs<DirtyTag>, DataDef<Data<int>>::Types>,
    "DirtyTag should not appear without DirtyTracker");
  cout << "PASS test_query_tag\n";
}

// ─────────────────────────────────────────────────────────────────────────────

#ifdef ARDUINO
  void setup() {
    Serial.begin(115200);
    while (!Serial);
    test_data_owned();
    test_data_ref();
    test_watch();
    test_static_num_range();
    test_default_value();
    test_find_tagged();
    test_query_tag();
  }
  void loop() {}
#else
  int main() {
    test_data_owned();
    test_data_ref();
    test_watch();
    test_static_num_range();
    test_default_value();
    test_find_tagged();
    test_query_tag();
    cout << "\nAll tests passed.\n";
    return 0;
  }
#endif
