/**
 * @file test.cpp
 * @brief OneData unit tests
 *
 * Tests:
 *   Data<T>           — owned RAM storage, get/set
 *   StaticData        — compile-time constant
 *   Watch             — change tracking (changed/sync)
 *   StaticNumRange    — compile-time range up/down/clamp
 *   NumRange          — runtime range up/down
 *   Default           — default value injection
 *   DataRef           — external variable reference
 *   DataDef           — composition closes chain
 */

#include <iostream>
#include <cassert>
#include <oneData/oneData.h>

#ifdef ARDUINO
  #define cout Serial
  #define assert(x) do { if(!(x)) { Serial.print("FAIL: "); Serial.println(#x); } } while(0)
#else
  using namespace std;
#endif

using namespace oneData;

void test_data_int() {
  DataDef<Int> d;
  assert(d.get() == 0);
  d.set(42);
  assert(d.get() == 42);
  DataDef<Int> d2{7};
  assert(d2.get() == 7);
  cout << "Data<int>: ok" << endl;
}

void test_data_bool() {
  DataDef<Bool> b;
  assert(!b.get());
  b.set(true);
  assert(b.get());
  cout << "Data<bool>: ok" << endl;
}

void test_static_val() {
  DataDef<StaticData<99>> d;
  assert(d.get() == 99);
  static_assert(DataDef<StaticData<99>>::get() == 99);
  DataDef<StaticData<true>> b;
  assert(b.get());
  cout << "StaticData: ok" << endl;
}

void test_watch() {
  DataDef<Watch<Int>> d;        // data=0, watched=0
  assert(!d.changed());
  d.set(10);
  assert(d.changed());          // data=10, watched=0
  d.sync();
  assert(!d.changed());         // data=10, watched=10
  d.set(10);
  assert(!d.changed());         // same value, no change
  d.set(5);
  assert(d.changed());
  cout << "Watch<Int>: ok" << endl;
}

void test_static_num_range() {
  DataDef<StaticNumRange<StaticRange<0,100>>, Int> pct{50};
  assert(pct.get() == 50);
  pct.up();
  assert(pct.get() == 51);
  pct.down();
  pct.down();
  assert(pct.get() == 49);
  pct.up(10);
  assert(pct.get() == 59);

  DataDef<StaticNumRange<StaticRange<0,100>>, Int> lo{0};
  lo.down();
  assert(lo.get() == 0);         // clamped at low

  DataDef<StaticNumRange<StaticRange<0,100>>, Int> hi{100};
  hi.up();
  assert(hi.get() == 100);       // clamped at high

  // wrapping range
  DataDef<StaticNumRange<StaticRange<0,3,true>>, Int> w{3};
  w.up();
  assert(w.get() == 0);          // wraps to low
  w.down();
  assert(w.get() == 3);          // wraps to high
  cout << "StaticNumRange<0,100> + wrap: ok" << endl;
}

void test_num_range() {
  DataDef<NumRange<int>, Int> r{0, 10, false, 5};
  assert(r.get() == 5);
  r.up();
  assert(r.get() == 6);
  r.down();
  r.down();
  assert(r.get() == 4);
  cout << "NumRange<int>: ok" << endl;
}

void test_default_value() {
  DataDef<Default<Int, 42>> d;
  assert(d.get() == 42);
  d.set(7);
  assert(d.get() == 7);
  DataDef<Default<Int, 42>> d2{99};
  assert(d2.get() == 99);
  cout << "Default<Int, 42>: ok" << endl;
}

inline int ext_var = 0;

void test_data_ref() {
  ext_var = 99;
  DataDef<DataRef<&ext_var>> d;
  assert(d.get() == 99);
  d.set(123);
  assert(ext_var  == 123);
  assert(d.get()  == 123);
  cout << "DataRef: ok" << endl;
}

void doTests() {
  test_data_int();
  test_data_bool();
  test_static_val();
  test_watch();
  test_static_num_range();
  test_num_range();
  test_default_value();
  test_data_ref();
  cout << "all OneData tests passed" << endl;
}

#ifdef ARDUINO
  void setup() { Serial.begin(115200); while(!Serial); doTests(); }
  void loop() {}
#else
  int main() { doTests(); return 0; }
#endif
