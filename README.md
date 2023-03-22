RL_JSON
=======

This package adds a command [json] to the interpreter, and defines a new
Tcl_Obj type to store the parsed JSON document.  The [json] command directly
manipulates values whose string representation is valid JSON, in a similar
way to how the [dict] command directly manipulates values whose string
representation is a valid dictionary.  It is similar to [dict] in performance.

Also provided is a command [json template] which generates JSON documents
by interpolating values into a template from a supplied dictionary or
variables in the current call frame.  The templates are valid JSON documents
containing string values which match the regex "^~[SNBJTL]:.+$".  The second
character determines what the resulting type of the substituted value will be:
* S: A string.
* N: A number.
* B: A boolean.
* J: A JSON fragment.
* T: A JSON template (substitutions are performed on the inserted fragment).
* L: A literal - the resulting string is simply everything from the forth character onwards (this allows literal strings to be included in the template that would otherwise be interpreted as the substitutions above).

None of the first three characters for a template may be escaped.

The value inserted is determined by the characters following the substitution
type prefix.  When interpolating values from a dictionary they name keys in the
dictionary which hold the values to interpolate.  When interpolating from
variables in the current scope, they name scalar or array variables which hold
the values to interpolate.  In either case if the named key or variable doesn't
exist, a JSON null is interpolated in its place.

Quick Reference
---------------
* [json get ?-default *defaultValue*? *json_val* ?*key* ...?]  - Extract the value of a portion of the *json_val*, returns the closest native Tcl type (other than JSON) for the extracted portion.
* [json extract ?-default *defaultValue*? *json_val* ?*key* ...?]  - Extract the value of a portion of the *json_val*, returns the JSON fragment.
* [json exists *json_val* ?*key* ...?]  - Tests whether the supplied key path resolve to something that exists in *json_val*
* [json set *json_variable_name* ?*key* ...? *value*]  - Updates the JSON value stored in the variable *json_variable_name*, replacing the value referenced by *key* ... with the JSON value *value*.
* [json unset *json_variable_name* ?*key* ...?]  - Updates the JSON value stored in the variable *json_variable_name*, removing the value referenced by *key* ...
* [json normalize *json_val*]  - Return a "normalized" version of the input *json_val* - all optional whitespace trimmed.
* [json template *json_val* ?*dictionary*?]  - Return a JSON value by interpolating the values from *dictionary* into the template, or from variables in the current scope if *dictionary* is not supplied, in the manner described above.
* [json string *value*]  - Return a JSON string with the value *value*.
* [json number *value*]  - Return a JSON number with the value *value*.
* [json boolean *value*]  - Return a JSON boolean with the value *value*.  Any of the forms accepted by Tcl_GetBooleanFromObj are accepted and normalized.
* [json object ?*key* *value* ?*key* *value* ...??]  - Return a JSON object with the keys and values specified.  *value* is a list of two elements, the first being the type {string, number, boolean, null, object, array, json}, and the second being the value.
* [json object *packed_value*]  - An alternate syntax that takes the list of keys and values as a single arg instead of a list of args, but is otherwise the same.
* [json array ?*elem* ...?]  - Return a JSON array containing each of the elements given.  *elem* is a list of two elements, the first being the type {string, number, boolean, null, object, array, json}, and the second being the value.
* [json foreach *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - Evaluate *script* in a loop in a similar way to the [foreach] command.  In each iteration, the values stored in the iterator variables in *varlist* are the JSON fragments from *json_val*.  Supports iterating over JSON arrays and JSON objects.  In the JSON object case, *varlist* must be a two element list, with the first specifiying the variable to hold the key and the second the value.  In the JSON array case, the rules are the same as the [foreach] command.
* [json lmap *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - As for [json foreach], except that it is collecting - the result from each evaluation of *script* is added to a list and returned as the result of the [json lmap] command.  If the *script* results in a TCL_CONTINUE code, that iteration is skipped and no element is added to the result list.  If it results in TCL_BREAK the iterations are stopped and the results accumulated so far are returned.
* [json amap *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - As for [json lmap], but the result is a JSON array rather than a list.  If the result of each iteration is a JSON value it is added to the array as-is, otherwise it is converted to a JSON string.
* [json omap *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - As for [json lmap], but the result is a JSON object rather than a list.  The result of each iteration must be a dictionary (or a list of 2n elements, including n = 0).  Tcl_ObjType snooping is done to ensure that the iteration over the result is efficient for both dict and list cases.  Each entry in the dictionary will be added to the result object.  If the value for each key in the iteration result is a JSON value it is  added to the array as-is, otherwise it is converted to a JSON string.
* [json isnull *json_val* ?*key* ...?]  - Return a boolean indicating whether the named JSON value is null.
* [json type *json_val* ?*key* ...?]  - Return the type of the named JSON value, one of "object", "array", "string", "number", "boolean" or "null".
* [json length *json_val* ?*key* ...?]  - Return the length of the of the named JSON array, number of entries in the named JSON object, or number of characters in the named JSON string.  Other JSON value types are not supported.
* [json keys *json_val* ?*key* ...?]  - Return the keys in the of the named JSON object, found by following the path of *key*s.
* [json pretty ?-indent *indent*? *json_val* ?*key* ...?]  - Returns a pretty-printed string representation of *json_val*.  Useful for debugging or inspecting the structure of JSON data.
* [json decode *bytes* ?*encoding*?]  - Decode the binary *bytes* into a character string according to the JSON standards.  The optional *encoding* arg can be one of *utf-8*, *utf-16le*, *utf-16be*, *utf-32le*, *utf-32be*.  The encoding is guessed from the BOM (byte order mark) if one is present and *encoding* isn't specified.
* [json valid ?*-extensions* *extensionlist*? ?*-details* *detailsvar*?  *json_val*]  - Return true if *json_val* conforms to the JSON grammar with the extensions in *extensionlist*.  Currently only one extension is supported: *comments*, and is the default.  To reject comments, use *-extensions {}*.  If *-details detailsvar* is supplied and the validation fails, the variable *detailsvar* is set to a dictionary with the keys *errmsg*, *doc* and *char_ofs*.  *errmsg* contains the reason for the failure, *doc* contains the failing json value, and *char_ofs* is the character index into *doc* of the first invalid character.

Paths
-----

The commands [json get], [json extract], [json set], [json unset] and [json exists]
accept a path specification that names some subset of the supplied *json_val*.
The rules are similar to the equivalent concept in the [dict] command, except
that the paths used by [json] allow indexing into JSON arrays by the integer
key (or a string matching the regex "^end(-[0-9]+)?$").  If a path to [json set]
includes a key within an object that doesn't exist, it and all later elements of
the path are created as nested keys into (new) objects.  If a path element into
an array is outside the current bounds of the array, it resolves to a JSON null
(for [json get], [json extract], [json exists]), or appends or prepends null
elements to resolve the path (for [json set], or does nothing ([json unset]).

~~~tcl
json get {
    {
        "foo": [
            { "name": "first" },
            { "name": "second" },
            { "name": "third" }
        }
    }
} foo end-1 name
~~~

Returns "second"

Properly Interpreting JSON from Other Systems
---------------------------------------------

Rl_json operates on characters, not bytes, and so considerations of encoding
are strictly out of scope.  However, interoperating with other systems
properly in a way that conforms to the standards is a bit tricky, and requires
support for encodings Tcl currently doesn't natively support, like utf-32be.
To ease this burden and take care of things like replacing broken encoding
sequences, the [json decode] subcommand is provided.  Using it in an application
would look something like:

~~~tcl
proc readjson file {
    set h [open $file rb]    ;# Note that the file is opened in binary mode
    try {
        json decode [read $h]
    } finally {
        close $h
    }
}
~~~

If the encoding is known via some out-of-band channel (like headers in an
HTTP response), it can be supplied to override the BOM-based detection.
The supported encodings are those listed in the JSON standards: utf-8 (the
default), utf-16le, utf-16be, utf-32le and utf-32be.

Examples
--------

## Creating a document from a template

Produce a JSON value from a template:
~~~tcl
json template {
    {
        "thing1": "~S:val1",
        "thing2": ["a", "~N:val2", "~S:val2", "~B:val2", "~S:val3", "~L:~S:val1"],
        "subdoc1": "~J:subdoc",
        "subdoc2": "~T:subdoc"
    }
} {
    val1   hello
    val2   1e6
    subdoc {
        { "thing3": "~S:val1" }
    }
}
~~~
Result:
~~~json
{"thing1":"hello","thing2":["a",1000000.0,"1e6",true,null,"~S:val1"],"subdoc1":{"thing3":"~S:val1"},"subdoc2":{"thing3":"hello"}}
~~~

## Construct a JSON array from a SQL result set

~~~tcl
# Given:
# sqlite> select * from languages;
# 'Tcl',1,'http://core.tcl-lang.org/'
# 'Node.js',1,'https://nodejs.org/'
# 'Python',1,'https://www.python.org/'
# 'INTERCAL',0,'http://www.catb.org/~esr/intercal/'
# 'Unlambda',0,NULL

set langs {[]}
sqlite3 db languages.sqlite3
db eval {
    select
        rowid,
        name,
        active,
        url
    from
        languages
} {
    if {$url eq ""} {unset url}

    json set langs end+1 [json template {
        {
            "id":       "~N:rowid",
            "name":     "~S:name",
            "details": {
                "active":   "~B:active",  // Template values can be nested anywhere
                "url":      "~S:url"      /* Both types of comments are
                                             allowed but stripped at parse-time */
            }
        }
    }]
}

puts [json pretty $langs]
~~~
Result:
~~~json
[
    {
        "id":      1,
        "name":    "Tcl",
        "details": {
            "active": true,
            "url":    "http://core.tcl-lang.org/"
        }
    },
    {
        "id":      2,
        "name":    "Node.js",
        "details": {
            "active": true,
            "url":    "https://nodejs.org/"
        }
    },
    {
        "id":      3,
        "name":    "Python",
        "details": {
            "active": true,
            "url":    "https://www.python.org/"
        }
    },
    {
        "id":      4,
        "name":    "INTERCAL",
        "details": {
            "active": false,
            "url":    "http://www.catb.org/~esr/intercal/"
        }
    },
    {
        "id":      5,
        "name":    "Unlambda",
        "details": {
            "active": false,
            "url":    null
        }
    }
]
~~~

## Performance

Good performance was a requirement for rl_json, because it is used to handle
large volumes of data flowing to and from various JSON based REST apis.  It's
generally the fastest option for working with JSON values in Tcl from the
options I've tried, with the next closest being yajltcl.  These benchmarks
report the median times in microseconds, and produce quite stable results
between runs.  Benchmarking was done on a MacBook Air running Ubuntu
14.04 64bit, Tcl 8.6.3 built with -O3 optimization turned on, and using
an Intel i5 3427U CPU.

### Parsing

This benchmark compares the relative performance of extracting the field
containing the string "obj" from the JSON doc:

```json
{
	"foo": "bar",
	"baz": ["str", 123, 123.4, true, false, null, {"inner": "obj"}]
}
```

The compared methods are:

| Name | Notes | Code |
|------|-------|------|
| old_json_parse | Pure Tcl parser | `dict get [lindex [dict get [json_old parse [string trim $json]] baz] end] inner` |
| rl_json_parse | | `dict get [lindex [dict get [json parse [string trim $json]] baz] end] inner` |
| rl_json_get | Using the built-in accessor method | `json get [string trim $json] baz end inner` |
| yajltcl | | `dict get [lindex [dict get [yajl::json2dict [string trim $json]] baz] end] inner` |
| rl_json_get_native | | `json get $json baz end inner` |

The use of [string trim $json] is to defeat the caching of the parsed
representation, forcing it to reparse the string each time since we're
measuring the parse performance here.  The exception is the rl_json_get_native
test which demonstrates the performance of the cached case.

```
-- parse-1.1: "Parse a small JSON doc and extract a field" --------------------
                   | This run
    old_json_parse |  241.595
     rl_json_parse |    5.540
       rl_json_get |    4.950
           yajltcl |    8.800
rl_json_get_native |    0.800
```

### Validating

If the requirement is to validate a JSON value, the [json valid] command is a
light-weight version of the parsing engine that skips allocating values from
the document and only returns whether the parsing succeeded or failed, and
optionally a description of the failure.  It takes about a third of the time
to validate a document as parsing it, so the performance win is substantial.
On a relatively modern CPU validation takes about 11 cycles per byte, or around
200MB of JSON per second on a 2.3 GHz Intel i7.

### Generating

This benchmark compares the relative performance of various ways of
dynamically generating a JSON document.  Although all the methods produce the
same string, only the "template" and "template_dict" variants handle nulls in
the general case - the others manually test for null only for the one field
that is known to be null, so the performance of these variants would be worse
in a real-world scenario where all fields would need to be tested for null.

The JSON doc generated in each case is the one produced by the following JSON
template (where a(not_defined) does not exist and results in a null value in the produced document):

```json
{
	"foo": "~S:bar",
	"baz": [
		"~S:a(x)",
		"~N:a(y)",
		123.4,
		"~B:a(on)",
		"~B:a(off)",
		"~S:a(not_defined)",
		"~L:~S:not a subst",
		"~T:a(subdoc)",
		"~T:a(subdoc2)"
	]
}
```

The produced JSON doc is:

```json
{"foo":"Bar","baz":["str\"foo\nbar",123,123.4,true,false,null,"~S:not a subst",{"inner":"Bar"},{"inner2":"Bar"}]}
```

The code for these variants are too long to include in this table, refer to
bench/new.bench for the details.

| Name | Notes |
|------|-------|
| old_json_fmt | Pure Tcl implementation, builds JSON from type-annotated Tcl values |
| rl_json_new | rl_json's [json new], API compatible with the pure Tcl version used in old_json_fmt |
| template | rl_json's [json template] |
| yajltcl | yajltcl's type-annotated Tcl value approach |
| template_dict | As for template, but using a dict containing the values to substitute |
| yajltcl_dict | As for yajltcl, but extracting the values from the same dict used by template_dict |

```
-- new-1.1: "Various ways of dynamically assembling a JSON doc" ---------------
                 | This run
    old_json_fmt |   49.450
     rl_json_new |   10.240
        template |    4.520
         yajltcl |    7.700
   template_dict |    2.500
    yajltcl_dict |    7.530
```

Deprecations
------------

Version 0.10.0 deprecates various subcommands and features, which will be removed in a near future version:

* [json get_type *json_val* ?*key* ...?]  - Removed
    * lassign [json get_type *json_val* ?*key* ...?] val type  ->  set val [json get *json_val* ?*key* ...?]; set type [json type *json_val* ?*key* ...?]
* [json parse *json_val*]  - A deprecated synonym for [json get *json_val*].
* [json fmt *type* *value*]  - A deprecated synonym for [json new *type* *value*], which is itself deprecated, see below.
* [json new *type* *value*]  - Use direct subcommands of [json]:
    * [json new string *value*] -> [json string *value*]
    * [json new number *value*] -> [json number *value*]
    * [json new boolean *value*] -> [json boolean *value*]
    * [json new true] -> true
    * [json new false] -> false
    * [json new null] -> null
    * [json new json *value*] -> *value*
    * [json new object ...] -> [json object ...]   (but consider [json template])
    * [json new array ...] -> [json array ...]   (but consider [json template])
* modifiers at the end of a path  - Modifiers like [json get *json_val* foo ?type] are deprecated.  Replacements are:
    * ?type  - use [json type *json_val* ?*key* ...?]
    * ?length - use [json length *json_val* ?*key* ...?]
    * ?size - use [json length *json_val* ?*key* ...?]
    * ?keys - use [json keys *json_val* ?*key* ...?]

Under the Hood
--------------

Older versions used the yajl c library to parse the JSON string and properly
quote generated strings when serializing JSON values, but currently a custom
built parser and string quoter is used, removing the libyajl dependency.  JSON
values are parsed to an internal format using Tcl_Objs and stored as the
internal representation for a new type of Tcl_Obj.  Subsequent manipulation of
that value use the internal representation directly.

License
-------

Copyright 2015-2023 Ruby Lane.  Licensed under the same terms as the Tcl core.
