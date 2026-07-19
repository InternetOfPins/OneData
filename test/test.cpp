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
 *   DataFn            — external get()/set() functions (pins, ISR-shared vars, sensors)
 *   Translated        — bidirectional raw<->display value conversion
 *   ReadOnly          — erases set(), compile-time-enforced read-only view
 *   Decimals          — fixed N-decimal-place print formatting
 *   Printf            — printf-style print formatting via a compile-time format string
 *   DataDef           — composition closes chain
 */

#include <iostream>
#include <cassert>
#include <cstring>
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

struct FakePinSrc {
  static inline int value = 0;
  static int  get()      { return value; }
  static void set(int v) { value = v; }
};

void test_data_fn() {
  FakePinSrc::value = 42;
  DataDef<DataFn<FakePinSrc>> d;
  assert(d.get() == 42);
  d.set(7);
  assert(FakePinSrc::value == 7);
  assert(d.get() == 7);
  cout << "DataFn: ok" << endl;
}

struct AdcToVolts {
  static constexpr float toDisplay(int raw) { return raw * (5.0f/1023.0f); }
  static constexpr int   toRaw(float v)     { return (int)(v * 1023.0f/5.0f); }
};

void test_translated() {
  DataDef<Translated<Watch<Default<Int,0>>,AdcToVolts>> d;
  assert(d.get() == 0.0f);
  assert(!d.changed());
  d.set(2.5f);
  assert(d.get() > 2.4f && d.get() < 2.6f);
  assert(d.changed());           // Watch tracks the raw int below Translated; set() moved it off default
  d.sync();
  assert(!d.changed());
  cout << "Translated: ok" << endl;
}

void test_translated_readonly() {
  FakePinSrc::value = 512;
  DataDef<Translated<DataFn<FakePinSrc>,AdcToVolts>> d;
  assert(d.get() > 2.4f && d.get() < 2.6f);   // 512/1023*5 ~= 2.502
  cout << "Translated (read-only, DataFn-backed): ok" << endl;
}

void test_read_only() {
  DataDef<ReadOnly<Default<Int,42>>> d;
  assert(d.get() == 42);
  // d.set(7);  // would fail to compile: ReadOnly erases set()
  cout << "ReadOnly<Default<Int,42>>: ok" << endl;
}

void test_translated_read_only_wrapper() {
  FakePinSrc::value = 512;
  DataDef<Translated<ReadOnly<DataFn<FakePinSrc>>,AdcToVolts>> d;
  assert(d.get() > 2.4f && d.get() < 2.6f);
  // d.set(2.5f);  // would fail to compile: erasure propagates through Translated
  cout << "Translated<ReadOnly<DataFn<...>>>: ok" << endl;
}

struct StrOut {
  char buf[64]{};
  int len=0;
  void put(char c) { if(len<63) { buf[len++]=c; buf[len]=0; } }
  void put(long v) {
    if(v<0) { put('-'); v=-v; }
    char tmp[16]; int n=0;
    if(v==0) tmp[n++]='0';
    while(v>0) { tmp[n++]=(char)('0'+(v%10)); v/=10; }
    while(n>0) put(tmp[--n]);
  }
  void put(int v) { put((long)v); }
};

void test_decimals() {
  DataDef<Decimals<2,Data<float>>> d;
  d.set(3.14159f);
  StrOut out; int ctx=0;
  d.printItem(out, ctx);
  assert(strcmp(out.buf,"3.14")==0);

  DataDef<Decimals<0,Data<float>>> d0;
  d0.set(7.8f);
  StrOut out0;
  d0.printItem(out0, ctx);
  assert(strcmp(out0.buf,"8")==0);   // rounds to nearest whole, carry handled at N=0 too

  DataDef<Decimals<3,Data<float>>> d3;
  d3.set(-1.5f);
  StrOut out3;
  d3.print(out3);
  assert(strcmp(out3.buf,"-1.500")==0);

  cout << "Decimals: ok" << endl;
}

static constexpr CText fmt03{"%03d"};
int printf_target=7; // DataRef<&...>'s NTTP needs static storage duration

void test_printf() {
  DataDef<Printf<&fmt03, DataRef<&printf_target>>> d;
  StrOut out; int ctx=0;
  d.printItem(out, ctx);
  assert(strcmp(out.buf,"007")==0);

  printf_target=42;
  StrOut out2;
  d.print(out2);
  assert(strcmp(out2.buf,"042")==0);

  // get()/set() must still forward to the wrapped DataRef unchanged
  d.set(123);
  assert(printf_target==123);
  assert(d.get()==123);
  StrOut out3;
  d.print(out3);
  assert(strcmp(out3.buf,"123")==0);   // 3-digit value, no leading zeros to show

  cout << "Printf: ok" << endl;
}

int runtime_printf_target=7; // separate storage from printf_target, same NTTP reason
int other_runtime_printf_target=5; // DataRef<&...>'s NTTP needs static storage duration

void test_runtime_printf() {
  DataDef<RuntimePrintf<DataRef<&runtime_printf_target>>> d{"%03d"};
  StrOut out; int ctx=0;
  d.printItem(out, ctx);
  assert(strcmp(out.buf,"007")==0);

  runtime_printf_target=42;
  StrOut out2;
  d.print(out2);
  assert(strcmp(out2.buf,"042")==0);

  // get()/set() must still forward to the wrapped DataRef unchanged
  d.set(123);
  assert(runtime_printf_target==123);
  assert(d.get()==123);
  StrOut out3;
  d.print(out3);
  assert(strcmp(out3.buf,"123")==0);   // 3-digit value, no leading zeros to show

  // a second instance with a DIFFERENT runtime fmt, same wrapped type — proves
  // fmt is a real per-instance runtime value, not baked into the type
  DataDef<RuntimePrintf<DataRef<&other_runtime_printf_target>>> d2{"%5d"};
  StrOut out4;
  d2.print(out4);
  assert(strcmp(out4.buf,"    5")==0);

  cout << "RuntimePrintf: ok" << endl;
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
  test_data_fn();
  test_translated();
  test_translated_readonly();
  test_read_only();
  test_translated_read_only_wrapper();
  test_decimals();
  test_printf();
  test_runtime_printf();
  cout << "all OneData tests passed" << endl;
}

#ifdef ARDUINO
  void setup() { Serial.begin(115200); while(!Serial); doTests(); }
  void loop() {}
#else
  int main() { doTests(); return 0; }
#endif
