# OneData

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
| `Watch<W>` | Change-detection modifier wrapping any data component | `sizeof(T)` |
| `NumRange<N>` | Dynamic value range with step/wrap | 3 × `sizeof(N)` + 1 |
| `StaticNumRange<N, low, high>` | Compile-time range, clamp on step | 0 |
| `Default<T, v>` | Default-value injection modifier | 0 |

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