package require Tcl 8.6.2

#json fmt obj {
#	foo {array {num 1} {n 2} {n 5}}
#	bar {s "hello, world"}
#	baz {o
#		sub1 {bool yes}
#		sub2 {number 10e6}
#		sub3 true
#	}
#}

namespace eval json_old {
	namespace export *
	namespace ensemble create
	namespace path ::tcl::mathop

	proc get args {tailcall dict get {*}$args}

	try {
		package require yajltcl
	} on ok {} {
		proc parse str {yajl::json2dict $str}
	} on error {} {
		proc parse str { #<<<
			set offset	0
			set parsed	[_parse_value]
			_trim_whitespace
			if {$str ne ""} {
				throw {RL JSON FMT} "Trailing garbage at offset $offset"
			}
			set parsed
		}

		#>>>
		proc _eat_chars {count {expect {}}} { #<<<
			upvar 1 str str offset offset
			set consumed	[::string range $str 0 [- $count 1]]
			if {$expect ne ""} {
				if {$expect ne $consumed} {
					throw {RL JSON FMT} "Expecting \"$expect\" at offset $offset, got \"$consumed\""
				}
			}
			incr offset $count
			set str	[::string range $str $count end]
			set consumed
		}

		#>>>
		proc _trim_whitespace {} { #<<<
			upvar 1 str str offset offset
			if {[regexp -indices "^\[ \t\n\]+" $str match]} {
				set trim	[+ [lindex $match 1] 1]
				_eat_chars $trim
			}
		}

		#>>>
		proc _parse_value {} { #<<<
			upvar 1 str str offset offset
			_trim_whitespace
			switch -glob -- $str {
				"\{*"   _parse_object
				"\\\[*" _parse_array
				"\"*"   _parse_string
				[-0-9]* _parse_number
				true*   _parse_true
				false*  _parse_false
				null*   _parse_null

				default {
					throw {RL JSON FMT} "Character \"[::string index $str 0]\" at offset $offset doesn't start a valid value"
				}
			}
		}

		#>>>
		proc _parse_object {} { #<<<
			upvar 1 str str offset offset
			set start	$offset
			_eat_chars 1 "\{"
			_trim_whitespace

			set dict	{}

			if {[::string index $str 0] eq "\}"} {
				_eat_chars 1 "\}"
				return $dict
			}

			while {$str ne ""} {
				lappend dict [_parse_string]
				_trim_whitespace
				_eat_chars 1 :
				_trim_whitespace
				lappend dict [_parse_value]

				_trim_whitespace
				switch -exact -- [::string index $str 0] {
					, {
						_eat_chars 1
						_trim_whitespace
					}
					"\}" {
						_eat_chars 1
						return $dict
					}
					default {
						throw {RL JSON FMT} "Expecting \",\" or \"\}\" at offset $offset, got: \"[::string index $str 0]\""
					}
				}
			}

			throw {RL JSON FMT} "Object starting at offset $start wasn't terminated"
		}

		#>>>
		proc _parse_array {} { #<<<
			upvar 1 str str offset offset
			set start	$offset
			_eat_chars 1 \[
			_trim_whitespace

			set arr		{}

			if {[::string index $str 0] eq "\]"} {
				_eat_chars 1	;# the "]" character
				return $arr
			}

			while {$str ne ""} {
				lappend arr [_parse_value]
				switch -exact -- [::string index $str 0] {
					, {
						_eat_chars 1
						_trim_whitespace
					}
					\] {
						_eat_chars 1
						return $arr
					}
					default {
						throw {RL JSON FMT} "Expecting \",\" or \"\]\" at offset $offset, got: \"[::string index $str 0]\""
					}
				}
			}

			throw {RL JSON FMT} "Array starting at offset $start wasn't terminated"
		}

		#>>>
		proc _parse_string {} { #<<<
			# Doesn't exclude control characters (JSON spec allows any UNICODE char
			# except bare " and control characters)
			upvar 1 str str offset offset
			set start	$offset
			_eat_chars 1 \"

			set out	{}

			while {$str ne ""} {
				if {![regexp -indices "\[\\\\\"\]" $str found]} break
				set idx	[lindex $found 0]
				if {$idx > 0} {
					append out	[::string range $str 0 [- $idx 1]]
					_eat_chars $idx
				}

				# No default clause - flow can only reach here if the next char is \ or "
				switch -exact -- [::string index $str 0] {
					\\ {
						switch -exact -- [::string index $str 1] {
							\" {append out \"; _eat_chars 2}
							\\ {append out \\; _eat_chars 2}
							/  {append out /;  _eat_chars 2}
							b  {append out \b; _eat_chars 2}
							f  {append out \f; _eat_chars 2}
							n  {append out \n; _eat_chars 2}
							r  {append out \r; _eat_chars 2}
							t  {append out \t; _eat_chars 2}
							u {
								_eat_chars 2	;# the \u character sequence
								set hex	[_eat_chars 4]
								if {![regexp {^[0-9a-fA-F]+$} $hex]} {
									throw {RL JSON FMT} "Invalid escape sequence at offset $offset"
								}
								set charnum	[format %d 0x$hex]
								if {$charnum > 0xffff} {
									# Tcl only supports the Unicode basic multilingual page (0x0 - 0xffff)
									throw {RL JSON UNSUPPORTED} "Escape sequence at offset $offset describes a unicode character not supported by this version of Tcl"
								}
								append out [format %c $charnum]
							}
							default {
								throw {RL JSON FMT} "Invalid escape sequence at offset $offset"
							}
						}
					}
					\" {
						_eat_chars 1
						return $out
					}
				}
			}

			throw {RL JSON FMT} "String starting at offset $start wasn't terminated"
		}

		#>>>
		proc _parse_number {} { #<<<
			upvar 1 str str offset offset
			regexp -nocase -indices {^-?(0|[1-9][0-9]*)(\.[0-9]+)?(e[+-]?[0-9]+)?} $str found
			set len	[+ [lindex $found 1] 1]
			# The number formats permitted by JSON and checked by the regex above are a
			# subset of the formats allowed by Tcl, so just pass the string through.
			_eat_chars $len
		}

		#>>>
		proc _parse_true {} { #<<<
			upvar 1 str str offset offset
			# "true" is a valid Tcl boolean value, we could pass it through, but yajltcl returns 1
			_eat_chars 4 true
			return 1
		}

		#>>>
		proc _parse_false {} { #<<<
			upvar 1 str str offset offset
			# "false" is a valid Tcl boolean value, we could pass it through, but yajltcl returns 0
			_eat_chars 5 false
			return 0
		}

		#>>>
		proc _parse_null {} { #<<<
			upvar 1 str str offset offset
			# tricky - Tcl doesn't have a representation for NULL.  The choices here
			# are to choke or return "", go with the latter
			_eat_chars 4 null
			return ""
		}

		#>>>
	}

	proc script script { #<<<
		global _json_docstack
		lappend _json_docstack	{}

		uplevel 1 $script

		try {
			lindex $_json_docstack end
		} finally {
			set docstack	[lrange $_json_docstack end-1]
		}
	}

	#>>>

	namespace eval fmt {
		namespace export *
		namespace ensemble create
		namespace path ::tcl::mathop

		# Build quote_map <<<
		variable quote_map {}

		set ranges {args {
			set res	{}
			foreach {from to} $args {
				for {set i $from} {$i <= $to} {incr i} {
					lappend res $i
				}
			}
			set res
		}}

		lappend quote_map \
			"\""	"\\\"" \
			"\\"	"\\\\" \
			\b		"\\b" \
			\f		"\\f" \
			\n		"\\n" \
			\r		"\\r" \
			\t		"\\t"

		foreach q [apply $ranges 0x00 0x07  0x0b 0x0b  0x0e 0x1F  0x7F 0x9F] {
			lappend quote_map [format %c $q] [format \\u%4X $q]
		}
		# Build quote_map >>>

		proc string val {
			variable quote_map
			::string cat \" [::string map $quote_map $val] \"
		}

		proc object args {
			if {[llength $args] == 1} {set args	[lindex $args 0]}
			if {[llength $args] % 2 != 0} {error "json fmt object needs an even number of arguments"}
			::string cat \{ [join [lmap {k v} $args {
				format {%s:%s} [json_old fmt string $k] [json_old fmt {*}$v]
			}] ,] \}
		}

		proc array args {
			::string cat \[ [join [lmap e $args {
				json_old fmt {*}$e
			}] ,] \]
		}

		proc number val {+ $val 0}
		proc true {}  { ::string cat true }
		proc false {} { ::string cat false }
		proc null {}  { ::string cat null }
		proc bool val { expr {$val ? "true" : "false"} }
	}
}

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
