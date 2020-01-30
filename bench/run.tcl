#!/usr/bin/env cfkit8.6
# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4

set big	[string repeat a [expr {int(1e8)}]]	;# Allocate 100MB to pre-expand the zippy pool
unset big

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

package require platform
package require bench

proc with_chan {var create use} {
	upvar 1 $var h
	set h	[uplevel 1 [list if 1 $create]]
	try {
		uplevel 1 [list if 1 $use]
	} on return {r o} - on break {r o} - on continue {r o} {
		dict incr o -level 1
		return -options $o $r
	} finally {
		if {[info exists h] && $h in [chan names]} {
			catch {close $h}
		}
	}
}

proc readtext fn { with_chan h {open $fn r} {read $h} }


proc benchmark_mode script {
	set restore_state_actions	{}

	with_chan root {open |[list sudo [info nameofexecutable]] rb+} {
		chan configure $root -encoding binary -translation binary
		proc sudo args {
			upvar 1 root root
			puts $root [encoding convertto utf-8 [string map [list %script% [list $args]] {
				catch %script% r o
				set resp [encoding convertto utf-8 [list $r $o]]
				puts -nonewline [string length $resp]\n$resp
				flush stdout
			}]]
			flush $root
			set bytes	[gets $root]
			if {$bytes eq ""} {
				if {[eof $root]} {
					puts stderr "Root child died"
					close $root
					unset root
					return
				}
			}
			if {![string is integer -strict $bytes]} {
				error "Root child sync error"
			}
			lassign [encoding convertfrom utf-8 [read $root $bytes]] r o
			return -options $o $r
		}
		sudo eval {
			#chan configure stdin -buffering none -encoding utf-8
			chan configure stdout -buffering none -encoding binary -translation binary

			proc with_chan {var create use} {
				upvar 1 $var h
				set h	[uplevel 1 [list if 1 $create]]
				try {
					uplevel 1 [list if 1 $use]
				} on return {r o} - on break {r o} - on continue {r o} {
					dict incr o -level 1
					return -options $o $r
				} finally {
					if {[info exists h] && $h in [chan names]} {
						close $h
					}
				}
			}

			proc readtext fn          { with_chan h {open $fn r} {read $h} }
			proc writetext {fn value} { with_chan h {open $fn w} {puts -nonewline $h $value} }

			proc set_turbo state {
				if {$state} {
					set mode "turbo enable\nquit"
				} else {
					set mode "turbo disable\nquit"
				}
				#with_chan h {open |[list i7z_rw_registers 2>@ stderr] w} {
				#	chan configure $h -buffering none -encoding binary -translation binary -blocking 1
				#	puts $h $mode
				#}
				set res	[exec echo $mode | i7z_rw_registers]
				if {[regexp {Turbo is now (Enabled|Disabled)} $res - newstate]} {
					set newstate
				} else {
					set res
				}
			}

			proc get_turbo {} {
				expr {
					![exec rdmsr 0x1a0 --bitfield 38:38]
				}
			}
		}

		try {
			switch -glob -- [platform::generic] {
				linux-* {
					# Disable turbo boost <<<
					lappend restore_state_actions	[list sudo set_turbo [sudo get_turbo]]
					sudo set_turbo off
					# Disable turbo boost >>>

					# Disable frequency scaling <<<
					foreach governor [glob -nocomplain -type f /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor] {
						lappend restore_state_actions [list sudo writetext $governor [string trim [readtext $governor]]]
						sudo writetext $governor performance
					}
					# Disable frequency scaling >>>

					# Set highest scheduling priority
					lappend restore_state_actions	[list sudo exec renice --priority [exec nice] [pid]]
					sudo exec renice --priority -20 [pid]


					# Disable hyperthreading (effectively by disabling sibling cores, preventing the kernel from scheduling tasks for them)
					set siblings	{}
					foreach core [glob -nocomplain -type d -directory /sys/devices/system/cpu -tails cpu*] {
						if {![regexp {^cpu[0-9]+$} $core]} continue
						if {[file readable /sys/devices/system/cpu/$core/online]} {
							lappend restore_state_actions [list sudo writetext /sys/devices/system/cpu/$core/online [string trim [readtext /sys/devices/system/cpu/$core/online]]]
						}
						set sibs	[readtext /sys/devices/system/cpu/$core/topology/thread_siblings_list]
						dict lappend siblings $sibs $core
					}
					dict for {group cores} $siblings {
						set keep	[lmap core $cores {
							if {[file readable /sys/devices/system/cpu/$core/online]} continue
							set core
						}]
						if {$keep eq {}} {
							set keep	[list [lindex $cores 0]]
						}
						foreach core $cores {
							if {$core in $keep} continue
							sudo writetext /sys/devices/system/cpu/$core/online 0
						}
					}
				}

				default {
					puts stderr "Don't know the magic to set [platform::generic] up for repeatable benchmarking"
				}
			}

			uplevel 1 [list if 1 $script]
		} finally {
			foreach action [lreverse $restore_state_actions] {
				#puts stderr "Running restore action:\t$action"
				set errors 0
				try $action on error {errmsg options} {
					puts stderr "Error processing restore action \"$action\": [dict get $options -errorinfo]"
					set errors	1
				}
				if {$errors} {
					throw exit 2
				}
			}
		}
		close $root write
		read $root
	}
}

proc main {} {
	try {
		set here	[file dirname [file normalize [info script]]]
		benchmark_mode {
			puts "[string repeat - 80]\nStarting benchmarks\n"
			bench::run_benchmarks $here {*}$::argv
		}
	} on ok {} {
		exit 0
	} trap {BENCH INVALID_ARG} {errmsg options} {
		puts stderr $errmsg
		exit 1
	} trap exit code {
		exit $code
	} on error {errmsg options} {
		puts stderr "Unhandled error from benchmark_mode: [dict get $options -errorinfo]"
		exit 2
	}
}

main

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
