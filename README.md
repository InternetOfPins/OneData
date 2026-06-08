# OneData

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Lightweight data components for **HAPI** and embedded systems.

Provides owned values, references, ranges, defaults, serialization and change tracking with zero runtime overhead.

---

## Features

- `Data<T>` — owned runtime data
- `DataRef<T&>` — reference to external variable
- `StaticData<T, v>` — compile-time immutable values
- `Text` / `TextRef` — string handling
- `Watch<>` — automatic change detection
- `NumRange<>` / `StaticNumRange<>` — value limiting and stepping
- `Default<T>` — default value injection
- `print(out)` — direct streaming to physical sinks and HAPI layers

---

## Quick Usage

```cpp
#include "oneData.h"
using namespace oneData;
using namespace oneData;

int value = 42;
int value = 42;

Text label = "Status";
auto volume = DataDef<Watch,Int&>{value};//will detect changes (watch) on external var
auto power = DataDef<Watch,Int>{60};//own data, watch for changes

if (volume.changed()) {
    // ...
    volume.sync();//clear changed, prepare for next change
}