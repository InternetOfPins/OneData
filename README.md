# OneData

Lightweight data components for **HAPI** and embedded systems.

Provides owned values, references, ranges, defaults and change tracking with zero runtime overhead.

---

## Features

- `Data<T>` — owned runtime data
- `DataRef<T&>` — reference to external variable
- `Text` / `TextRef` — string handling
- `Watch<>` — automatic change detection
- `NumRange<>` / `StaticNumRange<>` — value limiting and stepping
- `Default<T>` — default value injection
- Full CRTP / `APIOf` / `Chain` compatibility

---

## Quick Usage

```cpp
#include "oneData.h"

using namespace hapi::data;

Text label = "Status";
Int  value = 42;

auto volume = MenuData<int>{75};        // menu-extended version

if (volume.changed()) {
    // ...
}
```