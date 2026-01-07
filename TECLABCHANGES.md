# TECLAB Changes: master → wip

This document describes all changes made in the `wip` branch compared to `master`.

## Commit History

```
5a1a51b WIP
17b92f7 WIP
9bb174f beautify
a476851 adding autoarray tests
fa88e9d readding libtommath submodule
0776812 fix compile errors
66d3a82 Merge branch 'master' of https://github.com/RubyLane/rl_json into wip
5217388 added nopadding test
9ccf3f8 Merge pull request #3 from teclabat/autoarray
321b159 Merge branch 'wip' into autoarray
a07c3f1 Merge pull request #2 from teclabat/pretty
c3588aa pretty command now accepts the -nopadding switch cleanup the pretty.test
23fc406 enhancements for the pretty command: -compact -indent -arrays
ff30d6f new autoarray method
1b9c8e3 Merge pull request #1 from teclabat/noparseargs
10f2db0 parse_args fails to load during tests, simplify as it seems it's anyhow overkill for this
```

## Summary Statistics

```
 16 files changed, 891 insertions(+), 79 deletions(-)
```

## Major Features Added

### 1. New `json autoarray` Command

A new command that creates JSON arrays with automatic type detection, eliminating the need for explicit type specification.

**Syntax:**
```tcl
json autoarray ?value ...?
```

**Features:**
- Automatically detects JSON booleans (exact "true" or "false")
- Automatically detects valid JSON numbers
- Defaults to JSON strings for all other values

**Example:**
```tcl
json autoarray 1 2.5 true false "hello world" 42
# Returns: [1,2.5,true,false,"hello world",42]
```

**Files:**
- `generic/rl_json.c`: Implementation of `jsonAutoArray()` function
- `doc/json.n`: Documentation added
- `tests/autoarray.test`: Comprehensive test suite (322 new lines)

### 2. Enhanced `json pretty` Command

Significant enhancements to the pretty-printing functionality with three new options.

**New Options:**

#### `-compact`
Returns a compact, single-line representation with no extra whitespace (equivalent to `json normalize`).

**Example:**
```tcl
json pretty -compact $jsonValue
```

#### `-nopadding`
Removes the automatic padding/alignment of object keys, resulting in more condensed output.

**Example:**
```tcl
json pretty -nopadding $jsonValue
```

#### `-arrays <mode>`
Controls array formatting with two modes:
- `inline`: All arrays formatted on a single line `[1,2,3]`
- `multiline`: All arrays formatted with one element per line

**Default behavior:** Arrays with ≤3 elements are inline, larger arrays are multiline.

**Example:**
```tcl
json pretty -arrays inline $jsonValue
json pretty -arrays multiline $jsonValue
```

**Files Modified:**
- `generic/rl_json.c`: Core implementation
- `generic/api.c`: API updates
- `generic/rl_json.decls`: Declaration updates
- `generic/rl_jsonDecls.h`: Header updates
- `generic/rl_jsonInt.h`: Internal header updates
- `doc/json.n`: Extended documentation
- `tests/pretty.test`: Expanded test coverage (+180 lines)

## Bug Fixes and Improvements

### Parser Error Reporting
**File:** `generic/parser.c`

Fixed format specifiers for cross-platform compatibility:
- Added Tcl version-specific format strings for error messages
- Proper handling of `size_t` values in error reporting
- Fixed warnings on 64-bit systems

### Memory Leak Debugging
**File:** `generic/rl_json.c`

Updated leak detection tools:
- Changed address handling from `unsigned long` to `Tcl_WideInt`
- Proper casting to `uintptr_t` for pointer conversions
- Better cross-platform compatibility

### TIP445 Support
**Files:**
- `generic/tip445.h`: Changed from symlink to regular file
- `generic/tip445.fix.h`: New file with TIP445 compatibility shims

Added compatibility layer for building on Tcl 8.6.

### Submodule Updates
**File:** `teabase`

Updated teabase submodule:
- From: `b58eac68cc3d07c4f3155a3d390e31fc29fb255b`
- To: `b293fee95e97cbe6ce1583807e4d2aed8404e2e3`

### libtommath Integration
**File:** `generic/rl_jsonInt.h`

Added proper configuration header support and reorganized includes for CBOR support.

## Configuration and Build Changes

### .gitignore
Added `.gse` to ignored files.

### Version Update
**File:** `doc/json.n`

Updated package version:
- From: `0.14.0`
- To: `0.15.0`

## Test Improvements

### New Test Files
- `tests/autoarray.test`: Complete test suite for autoarray functionality (322 lines)

### Enhanced Test Files
- `tests/pretty.test`: Added 180+ lines of new tests for enhanced pretty options
- `tests/helpers.tcl`: Minor updates for test infrastructure
- `tests/set.test`: Minor updates
- `tests/unset.test`: Minor updates

## API Changes

### Modified C API Functions

**`JSON_Pretty()`** signature changed:
```c
// Old:
int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* indent,
                Tcl_Obj** prettyString)

// New:
int JSON_Pretty(Tcl_Interp* interp, Tcl_Obj* obj, Tcl_Obj* indent,
                int nopadding, int compact, int arrays_inline,
                Tcl_Obj** prettyString)
```

**`json_pretty()`** internal function signature changed:
```c
// Old:
int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent,
                Tcl_Obj* pad, Tcl_DString* ds)

// New:
int json_pretty(Tcl_Interp* interp, Tcl_Obj* json, Tcl_Obj* indent,
                int nopadding, Tcl_Obj* pad, int arrays_inline,
                Tcl_DString* ds)
```

### New Tcl Commands
- `json autoarray ?value ...?`

### Enhanced Tcl Commands
- `json pretty ?-indent indent? ?-compact? ?-nopadding? ?-arrays inline|multiline? jsonValue ?key ...?`

## Documentation Updates

**File:** `doc/json.n`

Comprehensive documentation added/updated for:
1. `json autoarray` - Complete new section with examples
2. `json pretty` - Expanded with detailed descriptions of all new options
3. Version number updated throughout

## Backward Compatibility

### Breaking Changes
- The C API function `JSON_Pretty()` has a changed signature
- Code calling this C function directly will need to be updated

### Non-Breaking Changes
- All Tcl-level commands maintain backward compatibility
- New options are optional and default to previous behavior
- Existing scripts will continue to work unchanged

## Statistics by Component

### Code Changes
- **C source files:** 6 files modified, ~180 insertions
- **Header files:** 4 files modified, ~30 insertions
- **Documentation:** 1 file modified, ~70 insertions
- **Tests:** 4 files modified/added, ~500 insertions

### Lines of Code
- **Total additions:** 891 lines
- **Total deletions:** 79 lines
- **Net change:** +812 lines
