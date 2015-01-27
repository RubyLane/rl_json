RL_JSON
=======

This package adds a command [json] to the interpreter, and defines a new
Tcl_Obj type to store the parsed JSON document.  The [json] command directly
manipulates values whose string representation is valid JSON, in a similar
way to how the [dict] command directly manipulates values whose string
representaion is a valid dictionary.  It is similar to [dict] in performance.

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

The value inserted is determined by the characters following the substitution
type prefix.  When interpolating values from a dictionary they name keys in the
dictionary which hold the values to interpolate.  When interpolating from
variables in the current scope, they name scalar or array variables which hold
the values to interpolate.  In either case if the named key or variable doesn't
exist, a JSON null is interpolated in its place.

Quick Reference
---------------
* [json get *json_val* ?*key* ... ?*modifier*??]  - Extract the value of a portion of the *json_val*, returns the closest native Tcl type (other than JSON) for the extracted portion.
* [json parse *json_val*]  - A deprecated synonym for [json get *json_val*].
* [json get_typed *json_val* ?*key* ... ?*modifier*??]  - Extract the value of a portion of the *json_val*, returns a two element list: the first being the value that would be returned by [json get] and the second being the JSON type of the extracted portion.
* [json extract *json_val* ?*key* ... ?*modifier*??]  - Extract the value of a portion of the *json_val*, returns the JSON fragment.
* [json exists *json_val* ?*key* ... ?*modifier*??]  - Tests whether the supplied key path and modifier resolve to something that exists in *json_val*
* [json normalize *json_val*]  - Return a "normalized" version of the input *json_val* - all optional whitespace trimmed.
* [json template *json_val* ?*dictionary*?]  - Return a JSON value by interpolating the values from *dictionary* into the template, or from variables in the current scope if *dictionary* is not supplied, in the manner described above.
* [json new *type* *value*]  - Return a JSON fragment of type *type* and value *value*.
* [json fmt *type* *value*]  - A deprecated synonym for [json new *type* *value*].
* [json foreach *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - Evaluate *script* in a loop in a similar way to the [foreach] command.  In each iteration, the values stored in the iterator variables in *varlist* are the JSON fragments from *json_val*.  Supports iterating over JSON arrays and JSON objects.  In the JSON object case, *varlist* must be a two element list, with the first specifiying the variable to hold the key and the second the value.  In the JSON array case, the rules are the same as the [foreach] command.
* [json lmap *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - As for [json foreach], except that it is collecting - the result from each evaluation of *script* is added to a list and returned as the result of the [json lmap] command.  If the *script* results in a TCL_CONTINUE code, that iteration is skipped and no element is added to the result list.  If it results in TCL_BREAK the iterations are stopped and the results accumulated so far are returned.

Paths
-----

The commands [json get], [json get_typed], [json extract] and [json exists]
accept a path specification that names some subset of the supplied *json_val*.
The rules are similar to the equivalent concept in the [dict] command, except
that the paths used by [json] allow indexing into JSON arrays by the integer
key (or a string matching the regex "^end(-[0-9]+)?$"), and that the last
element can be a modifier:
* **?type** - Returns the type of the named fragment.
* **?length** - When the path refers to an array, this returns the length of the array.  When the path refers to a string, this returns the number of characters in the string.  All other types throw an error.
* **?size** - Valid only for objects, returns the number of keys defined in the object.
* **?keys** - Valid only for objects, returns a list of the keys in the object.

A literal value that would match one of the above modifiers can be used as the last element in the path by doubling the ?:

~~~tcl
json get {
    {
        "foo": {
            "?size": "quite big"
        }
    }
} foo ??size
~~~

Returns "quite big"

Examples
--------

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

Under the Hood
--------------

The yajl c library is used to parse the JSON string, and to generate properly
quoted strings when serializing JSON values.  JSON values are parsed to an
internal format using Tcl_Objs and stored as the internal representation for
a new type of Tcl_Obj.  Subsequent manipulation of that value use the internal
representation directly.

License
-------

Copyright 2015 Ruby Lane.  Licensed under the same terms as the Tcl core.
