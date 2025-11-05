# Compare two JSON values, ignoring non-semantic elements (optional
# whitespace, object key ordering, etc)
proc _compare_json {opts j1 j2 {path {}}} { #<<<
	set mismatch {
		msg {
			upvar 1 path path
			if {[llength $path] != 0} {
				append msg ", at path $path"
			}
			throw {RL TEST JSON_MISMATCH} $msg
		}
	}

	try {
		json type $j1
	} on error errmsg {
		apply $mismatch "Cannot parse left JSON value:\n$errmsg"
	} on ok j1_type {}

	try {
		json type $j2
	} on error errmsg {
		apply $mismatch "Cannot parse right JSON value:\n$errmsg"
	} on ok j2_type {}

	set j1_val	[json get $j1]
	set j2_val	[json get $j2]

	if {$j2_type eq "string" && [regexp {^\?([A-Z]):(.*)$} $j2_val - cmp_type cmp_args]} {
		# Custom matching <<<
		set escape	0
		switch -- $cmp_type {
			D { # Compare dates <<<
				if {$j1_type ne "string"} {
					apply $mismatch "left value isn't a date: $j1, expecting a string, got $j1_type"
				}

				set now		[clock seconds]
				try {
					set scanned	[clock scan $j1_val]
				} on error {errmsg options} {
					apply $mismatch "Can't interpret left date \"$j1_val\""
				}

				switch -regexp -matchvar m -- $cmp_args {
					today {
						if {[clock format $scanned -format %Y-%m-%d] ne [clock format $now -format %Y-%m-%d]} {
							apply $mismatch "left date \"$j1_val\" is not today"
						}
					}

					{^within ([0-9]+) (second|minute|hour|day|week|month|year)s?$} {
						lassign $m - offset unit
						if {![tcl::mathop::<= [clock add $now -$offset $unit] $scanned [clock add $now $offset $unit]]} {
							apply $mismatch "left date \"$j1_val\" is not within $offset $unit[s? $offset] of the current time"
						}
					}

					default {
						error "Invalid date comparison syntax: \"$cmp_args\""
					}
				}
				#>>>
			}

			G { # Glob match <<<
				if {$j1_type ne "string"} {
					apply $mismatch "left value isn't a string: $j1"
				}
				if {![string match $cmp_args $j1_val]} {
					apply $mismatch "left value doesn't match glob: \"$cmp_args\""
				}
				#>>>
			}

			R { # Regex match <<<
				if {$j1_type ne "string"} {
					apply $mismatch "left value isn't a string: $j1"
				}
				if {![regexp $cmp_args $j1_val]} {
					apply $mismatch "left value doesn't match regex: \"$cmp_args\""
				}
				#>>>
			}

			L { # Literal value <<<
				set j2_val	$cmp_args
				set escape	1
				#>>>
			}

			default {
				error "Invalid custom comparison type \"$cmp_type\""
			}
		}
		if {!$escape} {
			return
		}
		# Custom matching >>>
	}

	if {$j1_type ne $j2_type} {
		apply $mismatch "JSON value types differ: left $j1_type != right $j2_type"
	}

	switch -- $j1_type {
		object {
			# Two JSON objects are considered to match if they have the same
			# keys (regardless of order), and the values stored in those keys
			# match according to this function
			if {[json get $j1 ?size] != [json get $j2 ?size]} {
				apply $mismatch "Object keys differ: left ([json get $j1 ?keys]) vs. right ([json get $j2 ?keys])"
			}

			lassign [intersect3 [json get $j1 ?keys] [json get $j2 ?keys]] \
				j1_only both j2_only

			switch -- [llength $j1_only],[llength $j2_only] {
				0,0 {}
				*,0 {if {[dict get $opts -subset] ni {right intersection}} {apply $mismatch "Left object has extra keys: $j1_only"}}
				0,* {if {[dict get $opts -subset] ni {left intersection}} {apply $mismatch "Right object has extra keys: $j2_only"}}
				*,* {if {[dict get $opts -subset] ne "intersection"} {apply $mismatch "Left object has extra keys: $j1_only and right object has extra keys: $j2_only"}}
			}

			foreach key $both {
				_compare_json $opts [json extract $j1 $key] [json extract $j2 $key] [list {*}$path $key]
			}
		}

		array {
			# Two JSON arrays are considered to match if they have the same
			# number of elements, and each element (in order) is considered to
			# match by this function
			if {[json get $j1 ?length] != [json get $j2 ?length]} {
				apply $mismatch "Arrays are different length: left [json get $j1 ?length] vs. right [json get $j2 ?length]"
			}
			set idx	-1
			json foreach e1 $j1 e2 $j2 {
				incr idx
				_compare_json $opts $e1 $e2 $idx
			}
		}

		string    { if {[json get $j1] ne [json get $j2]} {apply $mismatch "Strings differ: left: \"[json get $j1]\" vs. right: \"[json get $j2]\""} }
		number    { if {[json get $j1] != [json get $j2]} {apply $mismatch "Numbers differ: left: [json extract $j1] vs. right: [json extract $j2]"} }
		boolean   { if {[json get $j1] != [json get $j2]} {apply $mismatch "Booleans differ: left: [json extract $j1] vs. right: [json extract $j2]"} }
		null      { }

		default {
			error "Unsupported JSON type for compare: \"$j1_type\""
		}
	}
}

#>>>
proc compare_json args { #<<<
    set opts [list -subset none j1 [lindex $args 0] j2 [lindex $args 1]]
    try {
        _compare_json $opts [dict get $opts j1] [dict get $opts j2]
    } trap {RL TEST JSON_MISMATCH} {errmsg options} {
        return $errmsg
    } on ok {} {
        return match
    }
}

#>>>
proc intersect3 {list1 list2} { #<<<
	set firstonly       {}
	set intersection    {}
	set secondonly      {}

	set list1	[lsort -unique $list1]
	set list2	[lsort -unique $list2]

	foreach item $list1 {
		if {[lsearch -sorted $list2 $item] == -1} {
			lappend firstonly $item
		} else {
			lappend intersection $item
		}
	}

	foreach item $list2 {
		if {[lsearch -sorted $intersection $item] == -1} {
			lappend secondonly $item
		}
	}

	list $firstonly $intersection $secondonly
}

#>>>

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
