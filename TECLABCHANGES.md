# TECLAB Changes: upstream rl_json v0.17

This document describes all changes made by TERMA/TECLAB on top of the upstream
[RubyLane/rl_json](https://github.com/RubyLane/rl_json) release v0.17.

## Commit History (TECLAB additions)

```
(pending) add TECLABCHANGES.md and update README.md
b16828a  adding autoarray, autoobject and pretty enhancements
85fd158  clean tests for tcl86 builds on msys2
```

---

## 1. Windows / MinGW64 Build Fix

### Compile error: `srandom()` / `random()` not available on MinGW64

**File:** `teabase/names.c`

MinGW64 with GCC 15 uses the Windows UCRT which does not expose `srandom()` and
`random()` (POSIX BSD extensions) even when `_POSIX_C_SOURCE` is defined.

**Fix:** Added an `#ifdef __MINGW32__` compatibility block after the includes:

```c
#ifdef __MINGW32__
/* MinGW64 UCRT does not expose random()/srandom() even with _POSIX_C_SOURCE */
#define srandom(seed) srand((unsigned int)(seed))
#define random()      ((long)rand())
#endif
```

Linux builds continue using `random()`/`srandom()` unchanged.

---

## 2. Windows Test Suite Fix

### Test failures due to cp1252 encoding on Windows

**File:** `tests/all.tcl`

On Windows, `tclsh` defaults to the system encoding (`cp1252`). When tcltest
spawns subprocess instances for each test file, those subprocesses also start
with `cp1252`. Test files contain non-ASCII literals (Japanese characters etc.)
encoded as UTF-8. Reading them as cp1252 causes encoding mismatches between the
expected and actual test results.

**Symptoms (before fix):**
- `\u` escape decoding tests failing (expected `は` read as 3 cp1252 bytes)
- `?length` returning byte count instead of character count
- Character offset in parse errors off by 6 (bytes vs chars)

**Fix:** Added encoding setup and forced single-process mode in `all.tcl` for
non-UTF-8 systems, placed before the `runAllTests` call:

```tcl
# On Windows, tcltest's subprocess mode spawns fresh tclsh processes that
# inherit the system encoding (cp1252) rather than utf-8.  Force single-process
# mode on non-utf-8 systems so test files are sourced in-process after the
# encoding is switched.
if {[encoding system] ne "utf-8"} {
    encoding system utf-8
    fconfigure stdout -encoding utf-8
    fconfigure stderr -encoding utf-8
}
...
configure ... {*}[expr {$_singleproc_for_encoding ? {-singleproc 1} : {}}] ...
```

Linux/Mac builds (already UTF-8) are unaffected.

---

## 3. New `json autoarray` Command

Creates a JSON array from Tcl values with automatic type detection, eliminating
the need for explicit type specification.

**Syntax:**
```tcl
json autoarray ?value ...?
```

**Type detection (applied to each value):**
1. Exact `"true"` (case-sensitive) → JSON boolean `true`
2. Exact `"false"` (case-sensitive) → JSON boolean `false`
3. Valid JSON number (via `force_json_number`) → JSON number
4. Anything else → JSON string

**Examples:**
```tcl
json autoarray 1 2.5 true false "hello" 42
# → [1,2.5,true,false,"hello",42]

json autoarray 0x1F 007 0b1010
# → [31,7,10]   (Tcl number formats converted to canonical JSON)

json autoarray True FALSE
# → ["True","FALSE"]  (not exact match → strings)
```

**Files:**
- `generic/rl_json.c`: `jsonAutoArray()` implementation; dispatch table wiring
- `tests/autoarray.test`: 33 new tests
- `README.md`: synopsis entry and full command description added

---

## 4. New `json autoobject` Command

Creates a JSON object from key-value pairs with automatic type detection for
values, complementing `json autoarray`.

**Syntax:**
```tcl
json autoobject ?key value ...?
```

**Features:**
- Requires an even number of arguments (key-value pairs); odd count → error
- Keys are always JSON strings
- Values undergo the same automatic type detection as `json autoarray`
- Duplicate keys: last value wins (standard Tcl dict behaviour)

**Examples:**
```tcl
json autoobject name "Alice" age 30 active true score 95.5
# → {"name":"Alice","age":30,"active":true,"score":95.5}

json autoobject key        ;# error: wrong # args
```

**Files:**
- `generic/rl_json.c`: `jsonAutoObject()` implementation; dispatch table wiring
- `tests/autoobject.test`: 31 new tests
- `README.md`: synopsis entry and full command description added

---

## 5. Enhanced `json pretty` Command

Upstream v0.17 removed three options that existed in the TECLAB fork. They have
been restored.

**Full syntax:**
```tcl
json pretty ?-indent indent? ?-compact? ?-nopadding? ?-arrays inline|multiline? json_val ?key ...?
```

### Option: `-compact`

Returns a single-line compact representation with no extra whitespace (equivalent
to `json normalize`).

```tcl
json pretty -compact {{"foo":"bar","arr":[1,2,3]}}
# → {"foo":"bar","arr":[1,2,3]}
```

### Option: `-nopadding`

Suppresses the automatic alignment padding of object keys.

```tcl
# Default (keys aligned):
json pretty {{"foo":1,"longerkey":2}}
# → {
#       "foo":       1,
#       "longerkey": 2
#   }

# With -nopadding:
json pretty -nopadding {{"foo":1,"longerkey":2}}
# → {
#       "foo": 1,
#       "longerkey": 2
#   }
```

### Option: `-arrays inline|multiline`

Controls array formatting:

| Mode | Behaviour |
|------|-----------|
| `inline` | All arrays on one line: `[1,2,3,4,5]` |
| `multiline` | Each element on its own line |
| *(default)* | Arrays with ≤3 elements inline; larger arrays multiline |

```tcl
json pretty -arrays inline    {[1,2,3,4,5]}  ;# → [1,2,3,4,5]
json pretty -arrays multiline {[1,2,3]}       ;# → each on its own line
```

**Files:**
- `generic/rl_json.c`: `jsonPretty()` option parsing; `json_pretty()` and
  `json_pretty_dbg()` body updated (nopadding gate, inline/multiline array logic)
- `generic/api.c`: `JSON_Pretty()` — signature extended to 7 params; compact
  fast-path added
- `generic/rl_jsonInt.h`: `json_pretty()` forward declaration updated
- `generic/rl_jsonDecls.h`: `JSON_Pretty` stub declaration and struct field updated
- `generic/rl_json.decls`: Stubs source declaration updated
- `tests/pretty.test`: 18 new tests; 2 existing tests updated for new option set
- `README.md`: synopsis and per-option documentation updated

---

## API Changes

### Modified C API

**`JSON_Pretty()`** — signature extended:

```c
/* Upstream v0.17: */
int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* indent,
                Tcl_Obj** prettyString)

/* TECLAB: */
int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* indent,
                int nopadding, int compact, int arrays_inline,
                Tcl_Obj** prettyString)
```

**`json_pretty()`** — internal function signature extended:

```c
/* Upstream v0.17: */
int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent,
                Tcl_Obj* pad, Tcl_DString* ds)

/* TECLAB: */
int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent,
                int nopadding, Tcl_Obj* pad, int arrays_inline,
                Tcl_DString* ds)
```

### New Tcl Commands

```tcl
json autoarray ?value ...?
json autoobject ?key value ...?
```

### Enhanced Tcl Commands

```tcl
json pretty ?-indent indent? ?-compact? ?-nopadding? ?-arrays inline|multiline? json_val ?key ...?
```

---

## Backward Compatibility

| Change | Impact |
|--------|--------|
| `JSON_Pretty()` C signature | **Breaking** for C extension code calling this directly |
| `json_pretty()` internal signature | Internal only; no external impact |
| New Tcl commands `autoarray`/`autoobject` | Additive; no impact on existing scripts |
| Pretty new options | Additive; defaults preserve previous behaviour |
| Test suite encoding fix | No user-visible change; fixes false failures on Windows |

---

## Documentation and Test Summary

| File | Status | Notes |
|------|--------|-------|
| `README.md` | Updated | autoarray, autoobject sections added; pretty options documented |
| `TECLABCHANGES.md` | New | This file |
| `tests/autoarray.test` | New | 33 tests |
| `tests/autoobject.test` | New | 31 tests |
| `tests/pretty.test` | Updated | +18 new tests, 2 updated for new option set |
| `tests/all.tcl` | Updated | UTF-8 encoding + singleproc fix for Windows |
| `teabase/names.c` | Updated | MinGW64 compile fix |
