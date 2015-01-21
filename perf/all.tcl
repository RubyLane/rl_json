#!/usr/bin/env cfkit8.6
# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4

if {[file system [info script]] eq "native"} {
	package require platform

	foreach platform [platform::patterns [platform::identify]] {
		set tm_path		[file join $env(HOME) .tbuild repo tm $platform]
		set pkg_path	[file join $env(HOME) .tbuild repo pkg $platform]
		if {[file exists $tm_path]} {
			tcl::tm::path add $tm_path
		}
		if {[file exists $pkg_path]} {
			lappend auto_path $pkg_path
		}
	}
}

set here	[file dirname [file normalize [info script]]]
set parent	[file dirname $here]
tcl::tm::path add $here
lappend auto_path $parent
lappend auto_path /home/cyan/local/lib

package require rl_json
package require benchmark
package require yajltcl
package require json_old

set json {
	{
		"foo": "bar",
		"baz": ["str", 123, 123.4, true, false, null, {"inner": "obj"}]
	}
}

set copy	"$json "
json parse $copy

set tmpl {
	{
		"foo": "~S:bar",
		"baz":  [
			"~S:a(x)",
			"~N:a(y)",
			123.4,
			"~B:a(on)",
			"~B:a(off)",
			"~S:a(not_defined)",
			"~L:~S:not a subst",
			"~J:a(subdoc)",
			"~T:a(subdoc2)"
		]
	}
}

set simple_tmpl {
	{
		"foo": "~S:bar",
		"baz": ["str", 123, 123.4, true, "~B:a(on)", null, {"inner": "obj"}]
	}
}

proc our_parse2dict_forced {} {
	global json
	dict get [lindex [dict get [json parse [string trim $json]] baz] end] inner
}

proc yajltcl_parse2dict_forced {} {
	global json
	dict get [lindex [dict get [yajl::json2dict [string trim $json]] baz] end] inner
}

proc our_parse2dict {} {
	global json
	dict get [lindex [dict get [json parse $json] baz] end] inner
}

proc yajltcl_parse2dict {} {
	global json
	dict get [lindex [dict get [yajl::json2dict $json] baz] end] inner
}


proc our_serialize {} {
	global json

	set copy2	$json
	string cat [json normalize $copy2]
}

proc our_normalize {} {
	global copy
	string cat [json normalize "$copy "]
}

array set a {
	x		str
	y		123
	on		yes
	off		0
	subdoc	{{"inner": "~S:bar"}}
	subdoc2	{{"inner2": "~S:bar"}}
}

proc our_simple_template_vars {} {
	global simple_tmpl a

	set bar	"Bar"

	json from_template $simple_tmpl
}

proc our_simple_template_dict {} {
	global simple_tmpl

	json from_template $simple_tmpl {
		foo		"Foo"
		bar		"Bar"
		baz		"Baz"
		a(x)		str
		a(y)		123
		a(on)		yes
		a(off)		0
		a(subdoc)	{{"inner" : "~S:bar"}}
		a(subdoc2)	{{"inner2" : "~S:bar"}}
	}
}


proc our_template_vars {} {
	global tmpl a

	set bar	"Bar"

	json from_template $tmpl
}

proc our_template_dict {} {
	global tmpl

	json from_template $tmpl {
		foo		"Foo"
		bar		"Bar"
		baz		"Baz"
		a(x)		str
		a(y)		123
		a(on)		yes
		a(off)		0
		a(subdoc)	{{"inner" : "~S:bar"}}
		a(subdoc2)	{{"inner2" : "~S:bar"}}
	}
}


proc naive_template {} {
	global a

	set bar	"Bar"

	subst {
		{
			"foo": "$bar",
			"baz":  \[
				"$a(x)",
				$a(y),
				123.4,
				[expr {$a(on) ? "true" : "false"}],
				[expr {$a(off) ? "true" : "false"}],
				[expr {[info exists a(not_defined)] ? "\"$a(not_defined)\"" : "null"}],
				"~S:not a subst",
				{"inner" : "$bar"},
				{"inner2" : "$bar"}
			\]
		}
	}
}

proc old_template {} {
	global a

	set bar	"Bar"

	json_old fmt obj [list \
		"foo" [list s $bar] \
		"baz" [list a \
			[list s $a(x)] \
			[list num $a(y)] \
			[list num 123.4] \
			[list bool $a(on)] \
			[list bool $a(off)] \
			[expr {[info exists a(not_defined)] ? [list s $a(not_defined)] : [list null]}] \
			[list s "~S:not a subst"] \
			[list o inner [list s $bar]] \
			[list o inner2 [list s $bar]] \
		] \
	]
}

proc new_fmt {} {
	global a

	set bar	"Bar"

	json fmt obj [list \
		"foo" [list s $bar] \
		"baz" [list a \
			[list s $a(x)] \
			[list num $a(y)] \
			[list num 123.4] \
			[list num 0x20] \
			[list bool $a(on)] \
			[list bool $a(off)] \
			[expr {[info exists a(not_defined)] ? [list s $a(not_defined)] : [list null]}] \
			[list s "~S:not a subst"] \
			[list o inner [list s $bar]] \
			[list o inner2 [list s $bar]] \
		] \
	]
}

proc our_serialize_template {} {
	global tmpl

	json normalize $tmpl
}


proc nop {} {
	global json
	json nop
}


proc main {} {
	global json tmpl simple_tmpl a

	puts "rl_json Parsed: [json parse [string trim $json]]"
	puts "yajltcl Parsed: [yajl::json2dict [string trim $json]]"
	puts "serialized: [our_serialize]"
	puts "new fmt: [new_fmt]"

	set bar	"Bar"

	benchmark::test 300 100 usec our_parse2dict_forced
	benchmark::test 300 100 usec yajltcl_parse2dict_forced
	benchmark::test 600 200 usec our_parse2dict
	benchmark::test 300 100 usec yajltcl_parse2dict

	benchmark::test 600 200 usec our_serialize
	benchmark::test 600 200 usec our_normalize
	benchmark::test 600 200 usec json normalize $json
	benchmark::test 600 200 usec json normalize "$json "

	benchmark::test 600 200 usec our_simple_template_vars
	benchmark::test 600 200 usec our_simple_template_dict
	benchmark::test 600 200 usec json from_template $simple_tmpl {
		foo		"Foo"
		bar		"Bar"
		baz		"Baz"
		a(x)		str
		a(y)		123
		a(on)		yes
		a(off)		0
		a(subdoc)	{{"inner" : "~S:bar"}}
		a(subdoc2)	{{"inner2" : "~S:bar"}}
	}
	benchmark::test 600 200 usec our_template_vars
	benchmark::test 600 200 usec our_template_dict
	benchmark::test 600 200 usec json from_template $tmpl {
		foo		"Foo"
		bar		"Bar"
		baz		"Baz"
		a(x)		str
		a(y)		123
		a(on)		yes
		a(off)		0
		a(subdoc)	{{"inner" : "~S:bar"}}
		a(subdoc2)	{{"inner2" : "~S:bar"}}
	}
	benchmark::test 600 200 usec naive_template
	benchmark::test 300 100 usec old_template
	benchmark::test 300 100 usec new_fmt
	benchmark::test 600 200 usec our_serialize_template

	benchmark::test 600 200 usec json exists $json baz 2
	benchmark::test 600 200 usec json get $json baz 2
	benchmark::test 600 200 usec json get_typed $json baz 2
	benchmark::test 600 200 usec json extract $json baz 2

	set d	[json get $json]
	benchmark::test 600 200 usec apply {d {lindex [dict get $d baz] 2}} $d
	benchmark::test 600 200 usec apply {json {json get $json baz 2}} $json

	benchmark::test 600 200 usec nop
}

main
