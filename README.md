RL_JSON
=======

This package adds a command [json] to the interpreter, and defines a new
Tcl_Obj type to store the parsed JSON document.  The [json] command directly
manipulates values whose string representation is valid JSON, in a similar
way to how the [dict] command directly manipulates values whose string
representaion is a valid dictionary.  It is similar to [dict] in performance.

Also provided is a command [json from_template] which generates JSON documents
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
* [json get_typed *json_val* ?*key* ... ?*modifier*??]  - Extract the value of a portion of the *json_val*, returns a two element list: the first being the value that would be returned by [json get] and the second being the JSON type of the extracted portion.
* [json extract *json_val* ?*key* ... ?*modifier*??]  - Extract the value of a portion of the *json_val*, returns the JSON fragment.
* [json exists *json_val* ?*key* ... ?*modifier*??]  - Tests whether the supplied key path and modifier resolve to something that exists in *json_val*
* [json normalize *json_val*]  - Return a "normalized" version of the input *json_val* - all optional whitespace trimmed.
* [json from_template *json_val* ?*dictionary*?]  - Return a JSON value by interpolating the values from *dictionary* into the template, or from variables in the current scope if *dictionary* is not supplied, in the manner described above.
* [json new *type* *value*]  - Return a JSON fragment of type *type* and value *value*.
* [json foreach *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - Evaluate *script* in a loop in a similar way to the [foreach] command.  In each iteration, the values stored in the iterator variables in *varlist* are the JSON fragments from *json_val*.  Supports iterating over JSON arrays and JSON objects.  In the JSON object case, *varlist* must be a two element list, with the first specifiying the variable to hold the key and the second the value.  In the JSON array case, the rules are the same as the [foreach] command.
* [json lmap *varlist1* *json_val1* ?*varlist2* *json_val2* ...? *script*]  - As for [json foreach], except that it is collecting - the result from each evaluation of *script* is added to a list and returned as the result of the [json lmap] command.  If the *script* results in a TCL_CONTINUE code, that iteration is skipped and no element is added to the result list.  If it results in TCL_BREAK the iterations are stopped and the results accumulated so far are returned.
