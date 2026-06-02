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
using namespace oneData;

int value = 42;

Text label = "Status";
auto volume = DataDef<Watch,Int&>{value};//will detect changes (watch) on external var
auto power = DataDef<Watch,Int>{60};//own data, watch for changes

if (volume.changed()) {
    // ...
    volume.sync();//clear changed, prepare for next change
}
```