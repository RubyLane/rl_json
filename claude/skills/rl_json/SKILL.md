---
name: using-rl-json
description: Parse, manipulate, and generate JSON in Tcl using the rl_json package. Use when working with JSON data in Tcl, parsing JSON documents, building JSON with templates, or when the user mentions json commands, JSON manipulation, or rl_json.
---

## Package Overview

The rl_json package provides high-performance JSON manipulation for Tcl. It defines a custom Tcl_Obj type
for efficient parsing and manipulation, with performance similar to the `dict` command. The package uses a
custom-built parser (older versions used yajl, but that dependency has been removed).

## Loading the Package

```tcl
package require rl_json
# Optionally import the json command into the current namespace:
if {[llength [info commands json]] == 0} {namespace import ::rl_json::json}
```

## Core Concept: JSON as Strings

The `json` commands operate on string values in JSON format, similar to how `dict` operates on strings
in dictionary format. Values have an internal cached representation for performance, but semantically
behave as if manipulating JSON strings directly.

**Critical distinction:**
- **`json get`** - Converts JSON to native Tcl types (objects→dicts, arrays→lists, null→empty string)
- **`json extract`** - Returns JSON as-is, preserving the JSON format for further manipulation

## Command Reference

### Reading JSON Data

**json get** ?**-default** *defaultValue*? *jsonValue* ?*key ...*?
:   Extract a value, returning native Tcl type. Cannot be queried further with `json get`.

    ```tcl
    set data {{"name": "Alice", "age": 30, "tags": ["dev", "admin"]}}
    json get $data name         ;# Returns: Alice (Tcl string)
    json get $data tags         ;# Returns: {"dev" "admin"} (Tcl list)
    json get $data missing      ;# Error
    json get -default {} $data missing  ;# Returns: {}
    ```

**json extract** ?**-default** *defaultValue*? *jsonValue* ?*key ...*?
:   Extract a value, returning JSON format. Can be queried with subsequent `json` commands.

    ```tcl
    set item [json extract $data tags]   ;# Returns: ["dev","admin"] (JSON)
    json get $item 0                      ;# Returns: dev

    # Performance note: Direct deep indexing is preferred
    json get $data tags 0                 ;# Preferred - equivalent but clearer
    ```

**json exists** *jsonValue* ?*key ...*?
:   Test if path exists and is not null. Returns boolean.

    ```tcl
    json exists $data name       ;# true
    json exists $data missing    ;# false
    json exists {{"x": null}} x  ;# false (null is treated as non-existent)
    ```

**json type** *jsonValue* ?*key ...*?
:   Get type: "object", "array", "string", "number", "boolean", or "null".

    ```tcl
    json type $data              ;# object
    json type $data tags         ;# array
    json type $data age          ;# number
    ```

**json isnull** *jsonValue* ?*key ...*?
:   Test if value is JSON null.

**json length** *jsonValue* ?*key ...*?
:   Get length (arrays: element count, objects: key count, strings: character count).

**json keys** *jsonValue* ?*key ...*?
:   Get object keys as Tcl list.

### Modifying JSON Data

**json set** *jsonVariableName* ?*key ...*? *value*
:   Modify JSON in-place (like `dict set`). Auto-creates nested objects if path doesn't exist.
    Performs automatic type coercion on *value*.

    ```tcl
    set doc {{}}
    json set doc name Alice      ;# "Alice" - not valid JSON, becomes string
    json set doc age 30          ;# 30 - valid JSON number
    json set doc active true     ;# true - valid JSON boolean
    json set doc flag yes        ;# "yes" - not valid JSON, becomes string
    json set doc tags {[]}       ;# [] - valid JSON array
    json set doc count "42"      ;# 42 - string rep is valid JSON number
    json set doc data null       ;# null - valid JSON null

    # Explicit type control when string rep could be ambiguous:
    json set doc name [json string Alice]    ;# Force string type
    json set doc count [json number 42]      ;# Force number type
    json set doc flag [json boolean yes]     ;# Force boolean (yes→true)

    # IMPORTANT: json set modifies the variable directly, don't reassign:
    json set doc key value               ;# ✅ Correct
    set doc [json set doc key value]     ;# ❌ Wrong - redundant
    ```

**Type coercion rule for `json set`:**

If the string representation of *value* (what `puts` would display) is valid JSON, it is inserted
as-is. Otherwise, it is wrapped as a JSON string.

**Examples (Tcl script literals → JSON output):**

These show what happens when you write these values literally in Tcl code:

- `true` → `true` (valid JSON boolean)
- `yes` → `"yes"` (Tcl boolean, but not valid JSON - wrapped as string)
- `123` → `123` (valid JSON number)
- `hello` → `"hello"` (not valid JSON - wrapped as string)
- `null` → `null` (valid JSON null)
- `{}` → `""` (empty string in Tcl - wrapped as empty JSON string)
- `{{}}` → `{}` (Tcl string containing "{}" - valid JSON empty object)
- `{[]}` → `[]` (Tcl string containing "[]" - valid JSON empty array)

**Examples (String values → JSON output):**

These show how rl_json interprets string values after Tcl quoting is resolved:

- String `true` → `true` (valid JSON boolean)
- String `yes` → `"yes"` (not valid JSON - wrapped as string)
- String `123` → `123` (valid JSON number)
- String `"123"` (with quote chars) → `"123"` (valid JSON string)
- String `hello` → `"hello"` (not valid JSON - wrapped as string)
- String `null` → `null` (valid JSON null)
- Empty string → `""` (empty JSON string)
- String `{}` → `{}` (valid JSON empty object)
- String `[]` → `[]` (valid JSON empty array)

**json unset** *jsonVariableName* ?*key ...*?
:   Remove a value. For objects: removes key. For arrays: removes element and shifts later elements.

    ```tcl
    json unset doc tags
    json unset doc items 0       ;# Remove first array element
    ```

### Creating JSON Values

**json string** *value*
:   Create JSON string.

**json number** *value*
:   Create JSON number. Accepts any Tcl numeric format.

**json boolean** *value*
:   Create JSON boolean. Accepts any Tcl boolean format (yes/no, true/false, 1/0, etc.).

**json object** ?*key value* ?*key value ...*??
:   Create JSON object. *value* is a 2-element list: `{type value}` where type is one of:
    string, number, boolean, null, object, array, json.

    ```tcl
    json object name {string Alice} age {number 30}
    # Returns: {"name":"Alice","age":30}

    # Alternate packed syntax:
    json object {name {string Alice} age {number 30}}
    ```

**json array** ?*elem ...*?
:   Create JSON array. *elem* is a 2-element list: `{type value}`.

    ```tcl
    json array {string foo} {number 42} {boolean true}
    # Returns: ["foo",42,true]
    ```

**Note:** For complex structures, **`json template`** is strongly preferred over `json object`/`json array`.

### JSON Templates

**json template** *jsonValue* ?*dictionary*?
:   Interpolate values into a JSON template. Templates are **valid JSON** with special string markers.

**Template markers (must be complete string values):**
- **~S:***name* - Insert as string
- **~N:***name* - Insert as number
- **~B:***name* - Insert as boolean
- **~J:***name* - Insert as JSON fragment (no additional parsing)
- **~T:***name* - Insert as template (recursively process)
- **~L:***remainder* - Literal (use remaining chars as-is, for escaping)

**Rules:**
1. Template markers **must be the entire string value** in the template
2. The template itself **must be valid JSON**
3. Values come from *dictionary* (if provided) **OR** current scope variables (never both)
   - With dictionary: ALL values sourced from dictionary keys only
   - Without dictionary: ALL values sourced from variables in current scope only
4. Missing variables/keys interpolate as `null`

```tcl
# ✅ Correct - using variables from current scope (no dictionary argument):
set name "Alice"
set age 30
json template {
    {
        "name": "~S:name",
        "age": "~N:age",
        "active": "~B:active"
    }
}
# Returns: {"name":"Alice","age":30,"active":null} (active is null - not defined)

# ✅ Correct - using dictionary (all values from dictionary):
json template {
    {
        "name": "~S:name",
        "age": "~N:age",
        "tags": ["~S:tag1", "~S:tag2"]
    }
} {name Alice age 30 tag1 dev tag2 admin}
# Returns: {"name":"Alice","age":30,"tags":["dev","admin"]}

# ❌ Wrong - can't mix dictionary values and variables:
set name "Alice"
json template {{"name": "~S:name"}} {age 30}
# name comes from dictionary (returns null), NOT from variable

# ❌ Wrong - can't interpolate inside string:
json template {{"url": "https://example.com/~S:path"}}  ;# Error: not valid JSON

# ✅ Correct - build the value first:
set url "https://example.com/$path"
json template {{"url": "~S:url"}}

# ❌ Wrong - template not valid JSON:
json template {{":val": ~J:data}}  ;# Missing quotes around template marker

# ✅ Correct - template is valid JSON:
json template {{":val": "~J:data"}}
```

**Real-world example:**
```tcl
set users {[]}
db eval {SELECT name, email, active FROM users} {
    json set users end+1 [json template {
        {
            "name": "~S:name",
            "email": "~S:email",
            "active": "~B:active"
        }
    }]
}
```

### Iteration

**json foreach** *varList1 jsonValue1* ?*varList2 jsonValue2 ...*? *script*
:   Iterate like `foreach`. For objects, *varList* must be 2 elements: `{key value}`.

    ```tcl
    # Arrays:
    json foreach item {["a","b","c"]} {
        puts $item    ;# Each $item is a JSON value
    }

    # Objects:
    json foreach {key val} {{"x":1,"y":2}} {
        puts "$key = $val"    ;# key is Tcl string, val is JSON value
    }
    ```

**json lmap** *varList1 jsonValue1* ?*varList2 jsonValue2 ...*? *script*
:   Collecting iteration returning Tcl list. Supports `continue` and `break`.

**json amap** *varList1 jsonValue1* ?*varList2 jsonValue2 ...*? *script*
:   Collecting iteration returning JSON array. Script results converted to JSON.

**json omap** *varList1 jsonValue1* ?*varList2 jsonValue2 ...*? *script*
:   Collecting iteration returning JSON object. Script must return dict/list of 2n elements.

### Utility Commands

**json normalize** *jsonValue*
:   Remove optional whitespace, return minimal JSON.

**json pretty** ?**-indent** *indent*? *jsonValue* ?*key ...*?
:   Format JSON for readability. Default indent is 4 spaces.

    ```tcl
    puts [json pretty $data]
    puts [json pretty -indent "\t" $data]
    ```

**json decode** *bytes* ?*encoding*?
:   Decode binary data to JSON string per RFC standards. Handles BOM detection and encodings:
    utf-8 (default), utf-16le, utf-16be, utf-32le, utf-32be.

    ```tcl
    proc readjson file {
        set h [open $file rb]
        try {
            json decode [read $h]
        } finally {
            close $h
        }
    }
    ```

**json valid** ?**-extensions** *extensionlist*? ?**-details** *detailsvar*? *jsonValue*
:   Validate JSON. About 3× faster than parsing with try/trap.

    ```tcl
    if {![json valid $input]} {
        error "Invalid JSON"
    }

    # With details:
    if {![json valid -details info $input]} {
        puts stderr [dict get $info errmsg]
        puts stderr "At offset: [dict get $info char_ofs]"
    }

    # Extensions (default: comments allowed):
    json valid -extensions {} $input       ;# Strict RFC, no comments
    json valid -extensions {comments} $input  ;# Allow // and /* */ comments
    ```

## Path Specifications

Commands like `json get`, `json set`, `json exists`, `json unset` accept path arguments:

- Object keys: Use key name
- Array indices: Use integer or "end", "end-1", etc.
- Mixed: Navigate through nested structures

**Path behavior:**
- **`json get`/`json extract`/`json exists`**: Out-of-bounds array access returns null/default
- **`json set`**: Creates nested objects if keys don't exist; extends arrays with nulls if needed
- **`json unset`**: Does nothing if path doesn't exist

```tcl
set data {{"items": [{"name":"first"},{"name":"second"},{"name":"third"}]}}

json get $data items end-1 name      ;# "second"
json get $data items 1 name          ;# "second"
json get $data items 99 name         ;# Returns null (out of bounds)

# Auto-create nested structure:
set doc {{}}
json set doc a b c value             ;# Creates {"a":{"b":{"c":"value"}}}

# Array building:
set arr {[]}
json set arr end+1 [json string first]   ;# Append
json set arr end+1 [json string second]
json set arr -1 [json string zero]       ;# Prepend (index -1)
# Result: ["zero","first","second"]
```

## Common Patterns

### Building JSON arrays incrementally

```tcl
set result {[]}
foreach item $tcl_list {
    json set result end+1 [json string $item]
}
```

### Working with extracted JSON vs native Tcl

```tcl
# ❌ Wrong - json get returns dict, not JSON:
set item [json get $data Item]
set value [json get $item field]     ;# Error if Item was object

# ✅ Correct - json extract keeps JSON format:
set item [json extract $data Item]
set value [json get $item field]     ;# Works

# ✅ Best - direct deep access:
set value [json get $data Item field]  ;# Clearest and most efficient
```

### Type coercion examples

```tcl
set doc {{}}
set str "hello"
set num 42
set bool_tcl yes      ;# Tcl boolean
set bool_json true    ;# JSON-compatible boolean
set null null

json set doc a $str         ;# {"a":"hello"} - not valid JSON, becomes string
json set doc b $num         ;# {"b":42} - valid JSON number
json set doc c $bool_tcl    ;# {"c":"yes"} - not valid JSON, becomes string
json set doc d $bool_json   ;# {"d":true} - valid JSON boolean
json set doc e $null        ;# {"e":null} - valid JSON null

# Edge cases needing explicit typing:
json set doc f [json string "123"]     ;# Force "123" (string, not number)
json set doc g [json string "true"]    ;# Force "true" (string, not boolean)
json set doc h [json boolean "yes"]    ;# Force true (Tcl boolean → JSON boolean)
json set doc i [json number "42"]      ;# Ensure 42 (though string rep already valid)

# More examples showing string rep behavior:
set x 1.0
json set doc j $x           ;# 1.0 (string rep is valid JSON number)

set y "  123  "
json set doc k $y           ;# 123 (whitespace around tokens is valid JSON)

set z "123abc"
json set doc l $z           ;# "123abc" (text after number is invalid JSON)

set w [expr {1 + 1}]
json set doc m $w           ;# 2 (string rep is valid JSON number)
```

## Performance Notes

1. **Cached internal representation**: Repeated access to same JSON value is fast
2. **Validation**: `json valid` is ~3× faster than try/catch parse
3. **Direct indexing preferred**: Use `json get $data a b c` instead of multiple extracts
4. **Template performance**: ~2-4× faster than manually building with `json object`/`array` and much more readable

## Exception Handling

**RL JSON PARSE** *errormessage* *string* *charOfs*
:   Thrown when parsing invalid JSON. Contains string and character offset of error.

    ```tcl
    try {
        json get $invalid_data
    } trap {RL JSON PARSE} {errmsg options} {
        lassign [lrange [dict get $options -errorcode] 4 5] doc char_ofs
        puts stderr "$errmsg at position $char_ofs"
    }
    ```

**RL JSON BAD_PATH** *path*
:   Thrown when path is invalid for the JSON structure.

## Deprecated Features (v0.10.0+)

Avoid these deprecated commands:
- `json get_type` → Use `json type`
- `json parse` → Use `json get`
- `json fmt` → Use `json string`/`json number`/etc.
- `json new` → Use `json string`/`json number`/etc. or `json template`
- Path modifiers (`?type`, `?keys`, etc.) → Use dedicated commands

## Best Practices Summary

1. **Use `json template` for complex structures** - cleaner and faster than manual construction
2. **Prefer direct deep indexing** - `json get $data a b c` over multiple extracts
3. **Use `json extract` only when needed** - when you need to pass JSON fragment elsewhere
4. **Explicit types when ambiguous** - use `json string`/`json number`/etc. for edge cases
5. **Remember `json set` modifies in-place** - don't reassign the result
6. **Validate with `json valid`** - faster than try/catch for validation-only scenarios
7. **Template markers must be complete string values** - can't interpolate inside strings
8. **Templates must be valid JSON** - markers must be quoted as strings in template
