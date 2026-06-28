# picojson Library Context

This workspace uses the **picojson** header-only C++ JSON parser/serializer:

- Location: `c:\Users\Daniel Sawitzki\Desktop\github\picojson\picojson.h`
- Include: `#include "../../picojson/picojson.h"` (adjust relative path as needed)
- License: BSD-2-Clause
- No build step required — drop the header in and include it.

---

## Core Types

```cpp
picojson::value          // variant holding any JSON value
picojson::value::array   // typedef for std::vector<picojson::value>
picojson::value::object  // typedef for std::map<std::string, picojson::value>

// Convenience typedefs at namespace scope:
picojson::array          // same as picojson::value::array
picojson::object         // same as picojson::value::object
picojson::null           // empty struct, used as template arg for null checks
```

All numbers are stored as `double` by default. Enable `int64_t` support by defining `PICOJSON_USE_INT64` before including the header.

---

## Parsing

### From a `std::string` (preferred for Resource_Manager use)

```cpp
picojson::value v;
std::string err = picojson::parse(v, json_string);
if (!err.empty()) {
    // err contains: "syntax error at line N near: <text>"
    SDL_Log("JSON parse error in %s: %s", filename, err.c_str());
    // fall back to defaults
}
```

### From a raw pointer + size (e.g. Win32 RCDATA resource)

```cpp
// After LockResource() gives you a void* ptr and DWORD size:
std::string json_str(static_cast<const char*>(ptr), static_cast<size_t>(size));
picojson::value v;
std::string err = picojson::parse(v, json_str);
```

### From iterators (e.g. file content already in a buffer)

```cpp
const char* s = raw_buffer;
std::string err;
const char* end = picojson::parse(v, s, s + length, &err);
// 'end' points to the first unconsumed character after the JSON value
```

### From `std::istream`

```cpp
std::ifstream f("data/tiles.json");
picojson::value v;
std::string err = picojson::parse(v, f);
```

---

## Type Checking and Access

**Always call `is<T>()` before `get<T>()`** — calling `get<T>()` on the wrong type throws `std::runtime_error`.

```cpp
// Check root type before doing anything
if (!v.is<picojson::object>()) {
    SDL_Log("Expected object at root of %s", filename);
    return false;
}
const picojson::object& obj = v.get<picojson::object>();
```

### Supported type template arguments

| JSON type   | C++ type          | `is<>` / `get<>` arg  |
|-------------|-------------------|-----------------------|
| null        | `picojson::null`  | `picojson::null`      |
| boolean     | `bool`            | `bool`                |
| number      | `double`          | `double`              |
| number (opt)| `int64_t`         | `int64_t` (needs `PICOJSON_USE_INT64`) |
| string      | `std::string`     | `std::string`         |
| array       | `picojson::array` | `picojson::array`     |
| object      | `picojson::object`| `picojson::object`    |

---

## Reading Object Fields

```cpp
const picojson::object& obj = v.get<picojson::object>();

// Safe field access — use contains() first
if (obj.count("level") && obj.at("level").is<double>()) {
    int level = static_cast<int>(obj.at("level").get<double>());
}

// Or use value::contains(key) on the value itself
if (v.contains("name") && v.get("name").is<std::string>()) {
    std::string name = v.get("name").get<std::string>();
}
```

`value::get(const std::string& key)` returns a static null value (not a crash) when the key is missing — but only if the value is already an object. Calling it on a non-object asserts.

---

## Reading Arrays

```cpp
if (!v.is<picojson::array>()) { /* handle error */ }
const picojson::array& arr = v.get<picojson::array>();

for (size_t i = 0; i < arr.size(); ++i) {
    if (arr[i].is<picojson::object>()) {
        const picojson::object& item = arr[i].get<picojson::object>();
        // process item
    }
}

// Range-based for (C++11)
for (const picojson::value& item : arr) {
    if (item.is<double>()) {
        double d = item.get<double>();
    }
}
```

Bounds-safe indexed access via `value::contains(size_t idx)`:

```cpp
if (v.contains(0) && v.get(0).is<double>()) {
    double first = v.get(0).get<double>();
}
```

---

## Iterating Object Keys

```cpp
const picojson::object& obj = v.get<picojson::object>();
for (const auto& kv : obj) {
    const std::string& key   = kv.first;
    const picojson::value& val = kv.second;
    // val.is<...>() / val.get<...>()
}
```

Note: `picojson::object` is a `std::map`, so keys are iterated in **alphabetical order**, not insertion order.

---

## Building JSON (Serialization)

```cpp
// Build an object
picojson::object obj;
obj["id"]    = picojson::value(std::string("tile_grass_01"));
obj["level"] = picojson::value(0.0);   // all numbers are double
obj["solid"] = picojson::value(true);

picojson::value root(obj);

// Compact output
std::string compact = root.serialize();

// Pretty-printed output (2-space indent)
std::string pretty = root.serialize(true);
```

```cpp
// Build an array
picojson::array arr;
arr.push_back(picojson::value(1.0));
arr.push_back(picojson::value(std::string("north")));
picojson::value root(arr);
```

---

## Mutating an Existing Value

```cpp
picojson::value v;
v.set<picojson::object>(picojson::object());          // reset to empty object
v.get<picojson::object>()["key"] = picojson::value(42.0);

// Nested mutation
picojson::value& nested = v.get<picojson::object>()["sub"];
nested.set<picojson::array>(picojson::array());
nested.get<picojson::array>().push_back(picojson::value(true));
```

---

## Depth Limit

The default maximum nesting depth is **100** (`DEFAULT_MAX_DEPTHS`). Game data files are unlikely to hit this, but if you use a custom parse context you can set it explicitly:

```cpp
picojson::value v;
picojson::default_parse_context ctx(&v, 50); // max 50 levels deep
std::string err;
picojson::_parse(ctx, json.begin(), json.end(), &err);
```

---

## Important Gotchas

**NaN and Infinity throw.** Constructing `picojson::value(NaN)` or `picojson::value(Inf)` throws `std::overflow_error`. Never store raw float computation results directly — clamp or validate first.

```cpp
// BAD — may throw if computation produces NaN/Inf
picojson::value bad(some_float_result);

// GOOD
if (std::isfinite(some_float_result)) {
    picojson::value ok(some_float_result);
}
```

**`get<T>()` on wrong type throws `std::runtime_error`.** Always guard with `is<T>()`.

**`get<double>()` on an `int64_t` value mutates the value** (converts it in-place to `number_type`). After calling `get<double>()` on an int64 value, `is<int64_t>()` returns false. Avoid mixing int64 and double access on the same value.

**Object key order is alphabetical** (std::map). Do not rely on insertion order when serializing and re-parsing.

**`>>` operator is not thread-safe** — it uses a global last-error string. Use the explicit `picojson::parse(v, str)` form instead.

**Locale affects number parsing.** picojson uses `localeconv()` to handle decimal separators. If the process locale uses `,` as decimal point, numbers in JSON will still parse correctly because picojson normalizes them. However, call `setlocale(LC_ALL, "")` before parsing if you want locale-aware behavior, or leave it at the default `"C"` locale for pure ASCII JSON.

---

## Recommended Pattern for Game Data Loading

This is the standard pattern used throughout Zeitgeist Evolved for loading any JSON_Data file via the Resource_Manager:

```cpp
// Returns false and logs on any error; caller falls back to defaults.
bool LoadJsonFile(const std::string& filename, picojson::value& out_root,
                  bool expect_object /* false = expect array */)
{
    // Resource_Manager returns the raw JSON text (from disk or RCDATA)
    std::string json;
    if (!g_ResourceManager.GetText(filename, json)) {
        SDL_Log("[JSON] Resource not found: %s", filename.c_str());
        return false;
    }

    std::string err = picojson::parse(out_root, json);
    if (!err.empty()) {
        SDL_Log("[JSON] Parse error in %s: %s", filename.c_str(), err.c_str());
        return false;
    }

    if (expect_object && !out_root.is<picojson::object>()) {
        SDL_Log("[JSON] Expected object root in %s", filename.c_str());
        return false;
    }
    if (!expect_object && !out_root.is<picojson::array>()) {
        SDL_Log("[JSON] Expected array root in %s", filename.c_str());
        return false;
    }

    return true;
}
```

Usage:

```cpp
picojson::value root;
if (LoadJsonFile("evolution_levels.json", root, true)) {
    const picojson::object& data = root.get<picojson::object>();
    // read fields...
}
// if LoadJsonFile returned false, use built-in defaults — do not abort
```

---

## Example: Tile Definition JSON Schema

```json
{
  "id": "grass_01",
  "evolution_level": 0,
  "image": "grass_01.png",
  "solid": false,
  "width_tiles": 1,
  "height_tiles": 1,
  "edges": {
    "north": "grass",
    "south": "grass",
    "east":  "grass",
    "west":  "grass"
  }
}
```

Reading it:

```cpp
picojson::value root;
if (!LoadJsonFile("tiles.json", root, false)) return; // array of tile defs

for (const picojson::value& tile_val : root.get<picojson::array>()) {
    if (!tile_val.is<picojson::object>()) continue;
    const picojson::object& t = tile_val.get<picojson::object>();

    std::string id    = t.count("id")    && t.at("id").is<std::string>()
                        ? t.at("id").get<std::string>() : "";
    int   evo_level   = t.count("evolution_level") && t.at("evolution_level").is<double>()
                        ? static_cast<int>(t.at("evolution_level").get<double>()) : 0;
    std::string image = t.count("image") && t.at("image").is<std::string>()
                        ? t.at("image").get<std::string>() : "";

    // Read edge compatibility
    if (t.count("edges") && t.at("edges").is<picojson::object>()) {
        const picojson::object& edges = t.at("edges").get<picojson::object>();
        std::string north = edges.count("north") && edges.at("north").is<std::string>()
                            ? edges.at("north").get<std::string>() : "";
        // ... east, south, west
    }
}
```

---

## Example: Evolution Level Parameters JSON Schema

```json
{
  "levels": [
    {
      "level": 0,
      "clearance_edge_size": 1,
      "npc_spawn_min": 1,
      "npc_spawn_max": 2,
      "creature_damage": 1,
      "dust_anim_min_sec": 1.0,
      "dust_anim_max_sec": 3.0
    },
    {
      "level": 1,
      "clearance_edge_size": 2,
      "npc_spawn_min": 1,
      "npc_spawn_max": 3,
      "creature_damage": 2,
      "dust_anim_min_sec": 1.0,
      "dust_anim_max_sec": 3.0
    }
  ],
  "alien_spawn_cap": 10,
  "alien_respawn_delay_sec": 5.0,
  "alien_damage_per_sec": 3.0,
  "launch_threshold_tiles": 100,
  "difficulty_scale_multiplier": 1.15,
  "difficulty_scale_cap": 3.0
}
```

---

## What NOT to Do

```cpp
// WRONG — no type check before get
std::string name = v.get("name").get<std::string>(); // throws if "name" is missing or not a string

// WRONG — storing NaN
picojson::value bad(0.0 / 0.0); // throws std::overflow_error

// WRONG — using >> operator in multi-threaded code
std::cin >> v; // not thread-safe

// WRONG — assuming key order matches insertion order
// picojson::object is std::map — always alphabetical
```
