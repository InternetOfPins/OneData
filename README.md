# <img src="logo.png" alt="OneData logo" width="32" height="32"> OneData

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Lightweight data components for **HAPI** and embedded systems.

**HAPI Compatibility:** Updated for new Check/Apply/ApplyPack API (2026-Q2)

Owned values, external references, compile-time constants, change tracking, value ranges, and default injection — all composable via `DataDef<>` with zero runtime overhead where the hardware allows it.

---

## Features

| Component | Description | RAM cost |
|---|---|---|
| `Data<T>` | Owned runtime value | `sizeof(T)` |
| `StaticData<T, v>` | Compile-time immutable constant | 0 |
| `DataRef<T*, &var>` | Reference to external variable or hardware register | 0 |
| `Watch<W>` | Change-detection modifier wrapping any data component (value-diff) | `sizeof(T)` |
| `Dirty<W>` | Change-detection modifier wrapping any data component (write-flag) | `sizeof(bool)` |
| `NumRange<N>` | Dynamic value range with step/wrap | 3 × `sizeof(N)` + 1 |
| `StaticNumRange<N, low, high>` | Compile-time range, clamp on step | 0 |
| `Default<T, v>` | Default-value injection modifier | 0 |
| `DataFn<Src>` | External `get()`/`set()` functions as storage (pins, ISR-shared vars, sensors) | 0 |
| `Translated<W, Policy>` | Bidirectional raw↔display value conversion (e.g. ADC counts ↔ volts) | 0 |
| `MapRange<inLo,inHi,outLo,outHi, W>` | Compile-time linear remap sugar over `Translated` (e.g. 0-100 percent ↔ 0-255 PWM duty) | 0 |
| `ReadOnly<W>` | Erases `set()` at compile time — genuine read-only view, not a silent no-op | 0 |
| `Decimals<N, W>` | Fixed N-decimal-place print formatting, no libc float-printf needed | 0 |

String aliases: `Text` (`Data<const char*>`), `Bool` (`Data<bool>`), `Int` (`Data<int>`).

Pointer aliases: `IntRef<p>`, `BoolRef<p>`, `CharRef<p>`, `StaticText<ref>`.

---

## Quick Usage

### Owned data with change tracking

```cpp
#include "oneData.h"
using namespace oneData;

auto power = DataDef<Watch<Int>, Int>{60};
power.set(80);
if (power.changed()) {
    // react to change
    power.sync(); // clear flag, arm for next change
}
```

### External variable or hardware register (0 bytes RAM)

```cpp
inline volatile int fake_hw{};
using PinPort = DataRef<volatile int*, &fake_hw>;
DataDef<PinPort> led_pin;
led_pin.set(0xFF); // writes directly to fake_hw
```

### Compile-time constant (0 bytes RAM, value lives in Flash/immediate)

```cpp
DataDef<StaticInt<42>> answer;
int v = answer.get(); // v == 42
```

### Static string reference (0 bytes RAM)

```cpp
inline const char* label = "Status";
DataDef<StaticText<label>> status_label;
// status_label.get() returns "Status" with no local storage
```

### Value range with step and wrap

```cpp
// Dynamic range
auto brightness = DataDef<NumRange<int>, Int>{};
// construct with: low, high, wraps, initial_value
auto vol = DataDef<NumRange<int>, Int>(0, 100, true, 50);
vol.up();   // 51
vol.down(); // 50

// Static range (compile-time, 0 bytes overhead)
DataDef<StaticNumRange<int, 0, 100>, Int> gain{50};
gain.up(10);   // 60
gain.down(20); // 40
```

### Default value injection

```cpp
DataDef<Default<int, 128>, Int> mid; // initialises to 128 without passing a value
```

### Translated value (raw ADC counts shown as volts)

```cpp
struct MyAdcPin { static int get(); static void set(int); };  // or any OnePin terminal — get()/set() already match

struct AdcToVolts {
    static constexpr float toDisplay(int raw) { return raw * (5.0f/1023.0f); }
    static constexpr int   toRaw(float v)     { return (int)(v * 1023.0f/5.0f); }  // omit if read-only
};

DataDef<Decimals<2, Translated<Watch<DataFn<MyAdcPin>>, AdcToVolts>>> sensor;
sensor.get();     // "2.50" -style raw->volts conversion, formatted to 2 decimals when printed
sensor.changed(); // tracks the raw int underneath Translated, not the lossy float
```

A read-only monitor (no editing) just needs a `Policy` with `toDisplay()` — `toRaw()` is never instantiated unless `set()` is actually called. Wrap with `ReadOnly<...>` for a compile-time-enforced guarantee instead of relying on that:

```cpp
DataDef<Translated<ReadOnly<DataFn<MyAdcPin>>, AdcToVolts>> sensor; // sensor.set(x) fails to compile
```

### Linear remap (percent field driving a PWM duty cycle)

```cpp
// field is edited/displayed 0-100 (percent); underlying storage is 0-255 (PWM duty) —
// replaces a runtime analogWrite(pin, map(v,0,100,0,255)) call at the write site.
DataDef<MapRange<0,100,0,255, Watch<Default<Int,55>>>> ch1{};
ch1.get();     // 140 (55 mapped from 0-100 into 0-255)
ch1.set(255);  // writes 100 to the underlying Int
```

Ranges are compile-time NTTPs, not runtime-configurable — same integer-truncation behavior as Arduino's `map()`.

---

## API Reference

### DataDef

```cpp
template<typename... OO>
using DataDef = DefaultDataDef<OO...>;
```

Composes a chain of data components via `APIOf`. The first matching `Part` in the chain provides each method; components that do not provide a method fall through to the next layer. The base `DataAPI<>` supplies no-op defaults for `changed()`, `sync()`, and `print()`.

### Data\<T\>

Owns a value of type `T`.

```cpp
const T& get() const noexcept;
void     set(V&&)  noexcept;   // forwarding
```

Constructor forwarding: the first argument is treated as the initial value when `V` is convertible to `T`; remaining arguments are forwarded to the base.

> **Note:** `Data<const char*>` copies the pointer, not the string. Ownership semantics for string literals are caller-managed.

### StaticData\<T, v\>

Stores nothing; `get()` returns a `constexpr` reference to `v`.

```cpp
static constexpr const T& get() noexcept;
```

### DataRef\<T*, address\>

Zero-RAM reference to an externally owned variable. `T` must be a pointer type; `address` must be a valid non-null pointer.

```cpp
static auto& get()    noexcept;  // returns *address (or address for char*)
static void  set(T v) noexcept;  // *address = v
```

### Watch\<W\>

Wraps another data component `W` and tracks whether `get()` has changed since the last `sync()`.

```cpp
bool changed() const noexcept; // get() != watched
void sync()          noexcept; // watched = get()
```

`Watch` delegates `get()`, `set()`, and constructors to `W`.

### Dirty\<W\>

Wraps another data component `W` and tracks whether `set()` has been called since the last `sync()` — a literal write-flag, not a value comparison. Writing the same value back still counts as changed (unlike `Watch<W>`), and it costs one `bool` regardless of `W`'s `Type` size instead of a full extra copy. Matches upstream AM4's real `prompt::dirty` semantics; `compat/am4.h`'s `FIELD()` macro uses this instead of `Watch<W>` for that reason. Starts dirty (`true`) — a field needs its first draw regardless of what its initial value happens to be.

```cpp
bool changed() const noexcept; // true if set() has run since the last sync()
void sync()          noexcept; // clears the flag
```

`Dirty` delegates `get()` and constructors to `W`; unlike `Watch`, it intercepts `set()` itself (forwarding to `W::set()`) rather than re-exposing it, since it has to flag the write.

### NumRange\<N\>

Adds a dynamic range `[m_low, m_high]` with optional wrap. Requires a backing `Data<N>` lower in the chain.

```cpp
// Constructor arguments (prepended to the chain):
Part(NRP low, NRP high, bool wraps, ...rest);

bool valid(NRP v) const noexcept;
NRP  clamp(NRP v) const noexcept;
void up  (NRP step = 1) noexcept;
void down(NRP step = 1) noexcept;
```

> **Known issue:** `stepDown` argument order is `(s, o)` where `s` is the step and `o` is the current value — the reverse of `stepUp(o, s)`. This is an internal inconsistency; `up()` and `down()` are the correct public interface and are unaffected.

### StaticNumRange\<N, low, high, wraps = false\>

Compile-time variant. No stored state; `up()`/`down()` clamp via `constexpr` expressions.

### Default\<T, defaultValue\>

Injects `defaultValue` as the first constructor argument to the layer below, so a `DataDef` can be constructed without explicitly providing an initial value.

### DataFn\<Src\>

Storage backed by external `get()`/`set()` static functions instead of an owned value. `Src` just needs `static Type get()` (and `static void set(Type)` if writable) — an OnePin terminal already exposes both (via `Mask<>`), so a pin works directly as `Src` with no adapter. A hand-written struct works the same way for volatile globals, hardware registers, sensor reads, etc.

```cpp
static Type get() noexcept; // Src::get()
static void set(Type) noexcept; // Src::set(v)
```

### Translated\<W, Policy\>

Bidirectional value conversion between an underlying raw storage `W` and a displayed/edited `Type` (e.g. a 0–1023 ADC reading shown/edited as 0.0–5.0 volts).

```cpp
static Type toDisplay(RawType) noexcept; // required on Policy
static RawType toRaw(Type)     noexcept; // required on Policy only if set() is ever called
```

`toRaw()` is only required if the field is actually edited — a non-template member function of a class template is only instantiated when called, so a read-only `Policy` (no `toRaw`) is enough for a monitor-only field. Defines its own `print()`/`printItem()` (does not forward to `Base`'s) — `Base` eventually reaches a `Data`/`DataFn`/`DataRef` terminal that would print the untranslated raw value too, double-printing the same logical value at two representations.

### MapRange\<inLo, inHi, outLo, outHi, W\>

```cpp
template<auto inLo, auto inHi, auto outLo, auto outHi, typename W>
using MapRange = Translated<W, MapPolicy<inLo,inHi,outLo,outHi>>;
```

Sugar over `Translated`: `MapPolicy<...>` computes `toDisplay()`/`toRaw()` as a linear remap between `[inLo,inHi]` (`W`'s own value range) and `[outLo,outHi]` (the displayed/edited range), instead of a hand-written `Policy` struct. All four bounds are compile-time NTTPs — not runtime-configurable, by design (the same static-only rule OneMenu's `Row`/`Rows` layout partition parameters follow). Integer NTTPs truncate the same way Arduino's `map()` does; pass floating-point NTTPs for fractional precision.

### ReadOnly\<W\>

Erases `set()` from `W` via private inheritance, re-exposing only `get()`/`print()`/`printItem()`. Calling `set()` on a `ReadOnly`-wrapped chain is a genuine compile error ("is inaccessible within this context"), not a silent no-op. Does not re-expose `changed()`/`sync()` — `ReadOnly<Watch<...>>` would lose those too; compose the other way (`Watch<ReadOnly<...>>`) if you need both.

### Decimals\<N, W\>

Fixed N-decimal-place print formatting for a floating-point `W`. Hijacks `print()`/`printItem()` the same way `Translated` does — it's replacing, not adding to, how the value is rendered. Digit extraction is done manually (no `snprintf`/`%f`), so it works on AVR without needing printf float-support linked in (`-Wl,-u,vfprintf -lprintf_flt -lm`), which isn't enabled by default.

---

## Composition notes

Components are composed left-to-right in the `DataDef<>` template argument list. A modifier like `Watch` or `Default` must appear before the data component it wraps:

```cpp
// Watch wrapping Int — correct
DataDef<Watch<Int>, Int> x{0};

// Default injecting into Int — correct
DataDef<Default<int, 0>, Int> y;
```

`print(out)` is defined on every component and chains down the stack; `out` must supply a `put()` method compatible with the stored type.

---

## Dependencies

- C++17 or later (required for `if constexpr`, fold expressions)
- `hapi/hapi.h` — provides `APIOf` and `hapi::Nil`
- No dynamic allocation; no exceptions; no RTTI
- Tested on AVR (avr-gcc 7+) and x86-64

---

## License

MIT — see [LICENSE](LICENSE).