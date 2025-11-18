# all.tcl --
#
# This file contains a top-level script to run all of the Tcl
# tests.  Execute it by invoking "source all.tcl" when running tcltest
# in this directory.
#
# Copyright © 1998-1999 Scriptics Corporation.
# Copyright © 2000 Ajuba Solutions
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package prefer latest
package require tcltest 2.5
namespace import ::tcltest::*

configure -testdir [file normalize [file dirname [info script]]] {*}$argv

if {[singleProcess]} {
    interp debug {} -frame 1
}

set checkmem	false
set argv [lmap e $argv {
	if {$e eq "-checkmem"} {
		set checkmem	true
		continue
	}
	set e
}]

if {$checkmem && [llength [info commands memory]] == 1} {
	memory init on
	#memory onexit memdebug
	#memory validate on
	#memory trace on
	memory tag startup

	proc intersect3 {list1 list2} {
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

	proc _mem_objs {} {
		set h	[file tempfile name]
		try {
			memory objs $name
			seek $h 0
			gets $h	;# Throw away the "total objects:" header
			set res	{}
			while {![eof $h]} {
				set line	[gets $h]
				lappend res $line
			}
			set res
		} finally {
			file delete $name
			close $h
		}
	}

	proc _mem_info {} {
		set res	{}
		foreach line [split [string trim [memory info]] \n] {
			if {![regexp {^(.*?)\s+([0-9]+)$} $line - k v]} continue
			dict set res $k $v
		}
		set res
	}

	proc _diff_mem_info {before after} {
		set res	{}

		dict for {k v} $after {
			dict set res $k [expr {$v - [dict get $before $k]}]
		}
		set res
	}

	proc _mem_active tag {
		set h	[file tempfile name]
		try {
			memory active $name
			seek $h 0
			set res	{}
			while {![eof $h]} {
				set line	[gets $h]
				if {[string match *$tag $line]} {
					lappend res $line
				}
			}
			set res
		} finally {
			file delete $name
			close $h
		}
	}

	proc memtest {name args} {
		if {[llength $::tcltest::match] > 0} {
			set ok	0
			foreach match $::tcltest::match {
				if {[string match $match $name]} {
					set ok	1
					break
				}
			}
			if {!$ok} return
		}
		set tag		"test $name"
		set before		[_mem_objs]
		set before_inf	[_mem_info]
		memory tag $tag
		try {
			uplevel 1 [list if 1 [list ::tcltest::test $name {*}$args]]
		} finally {
			set after_inf	[_mem_info]
			memory tag "overhead"
			set diff		[_diff_mem_info $before_inf $after_inf]
			if {[dict get $diff {current packets allocated}] > 80} {
				set after		[_mem_objs]
				#set new_active	[_mem_active $tag]
				#set new_active	[_mem_active ""]
				#puts stderr "$name: $diff"
				if {[incr ::count] > 0} {
					#if {[llength $new_active] > 0} {
					#	puts "$name new active:\n\t[join $new_active \n\t]"
					#}
					lassign [intersect3 $before $after] freed common new
					set new	[lmap line $new {
						if {![string match *unknown* $line]} continue
						#if {![regexp {/generic/(rl_json.|parser.|tclstuff.)} $line]} continue
						switch -glob -- $line {
							*tclIOCmd.c* -
							*tclLiteral* -
							*tclExecute* continue
						}
						set line
					}]
					set freed	[lmap line $freed {
						if {![string match *unknown* $line]} continue
						#if {![regexp {/generic/(rl_json.|parser.|tclstuff.)} $line]} continue
						switch -glob -- $line {
							*tclIOCmd.c* -
							*tclLiteral* -
							*tclExecute* continue
						}
						if {![string match *unknown* $line]} continue
						set line
					}]
					if {[llength $freed] > 0} {
						puts stderr "$name Existing objs freed:\n\t[join $freed \n\t]"
					}
					if {[llength $new] > 0} {
						#puts stderr "$name New objs created:\n\t[join $new \n\t]"
						#key = 0x0x1494cd8, objPtr = 0x0x1494cd8, file = unknown, line = 0
						puts stderr "$name New objs created:"
						foreach line $new {
							if {[regexp {, objPtr = 0x(0x.*?),} $line - addr]} {
								::rl_json::json _leak_info $addr
							}
						}
					}
					#set common	[lmap line $common {
					#	if {![regexp {/generic/(rl_json.|parser.|tclstuff.)} $line]} continue
					#	set line
					#}]
					#if {[llength $common] > 0} {
					#	puts stderr "$name New objs created:\n\t[join $common \n\t]"
					#}
				}
			}
		}
	}

	proc memtest2 {name args} {
		set bodyidx	[lsearch $args -body]
		if {$bodyidx == -1} {
			puts stderr "Can't find -body param to inject memory monitoring"
		} else {
			set body	[lindex $args $bodyidx+1]
			set args	[lreplace $args $bodyidx+1 $bodyidx+1 "set _checkmem_result \[::rl_json::checkmem [list $body] _checkmem_newactive\]; if {\[llength \$_checkmem_newactive\] > 0} {error \"Leaked memory:\\n\\t\[join \$_checkmem_newactive\[unset _checkmem_newactive\] \\n\\t\]\"} else {unset _checkmem_newactive}; return -level 0 \$_checkmem_result\[unset _checkmem_result\]"]
		}
		tailcall ::tcltest::test $name {*}$args
	}

	proc memtest3 {name args} {
		if {[llength $::tcltest::match] > 0} {
			set ok	0
			foreach match $::tcltest::match {
				if {[string match $match $name]} {
					set ok	1
					break
				}
			}
			if {!$ok} return
		}

		::rl_json::checkmem {
			uplevel 1 [list ::tcltest::test $name {*}$args]
		} newactive

		::tcltest::test "$name-mem" "Memory test for $name" -body {
			if {[llength $newactive] > 0} {
				return -level 0 "Leaked memory:\n\t[join $newactive \n\t]"
			}
		} -result {}
	}

	if 1 {
		rename test _test
		#interp alias {} test {} ::memtest
		#interp alias {} test {} ::memtest2
		interp alias {} ::test {} ::memtest3
	}
} else {
	proc memtest args {tailcall ::tcltest::test {*}$args}
}

set ErrorOnFailures [info exists env(ERROR_ON_FAILURES)]
unset -nocomplain env(ERROR_ON_FAILURES)
if {[runAllTests] && $ErrorOnFailures} {exit 1}
# if calling direct only (avoid rewrite exit if inlined or interactive):
if { [info exists ::argv0] && [file tail $::argv0] eq [file tail [info script]]
  && !([info exists ::tcl_interactive] && $::tcl_interactive)
} {
    proc exit args {}
}

if {[llength [info commands memory]] == 1} {
	memory tag shutdown
	#rl_json::json free_cache
	unload -nocomplain {} rl_json
	memory objs tclobjs_remaining
}
