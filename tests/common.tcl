package require tcltest 2.5
::tcltest::loadTestedCommands

proc runtests script {
	try {
		set testfile	[uplevel 1 {info script}]
		set ns			::[file tail $testfile]
		if {[namespace exists $ns]} {error "Test namespace \"$ns\" already exists"}
		namespace eval $ns {
			namespace path {
				::rl_json
				::tcltest
			}

			proc tclver spec {package vsatisfies [package provide Tcl] $spec}
			proc tcl9? {y n} [expr {[tclver 9.0-] ? {set y} : {set n}}]

			# number format constraints
			testConstraint underscore_separators	[string is integer -strict 1_000_000]
			testConstraint decimal_literals			[string is integer -strict 0d11]
			testConstraint binary_literals			[string is integer -strict 0b11]

			# Tcl version constraint for tests that depend on Tcl 9-only
			# syntax like the 3-argument form of `info loaded`.
			testConstraint tcl9						[tclver 9.0-]

			# TESTMODE builds expose extra knobs (e.g.
			# ::rl_json::unload_strategy) used by the test suite to
			# cover alternative internal code paths.
			testConstraint testmode					[::rl_json::build-info testmode]
			testConstraint unload					[::rl_json::build-info unload]
		}
		namespace eval $ns [list source [file join [file dirname [info script]] helpers.tcl]]
		namespace eval $ns $script
	} on error {errmsg options} {
		puts stderr "Error in $testfile: [dict get $options -errorinfo]"
	} finally {
		if {[info exists ns] && [namespace exists $ns]} {
			namespace delete $ns
		}
		::tcltest::cleanupTests
		return
	}
}

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4 noexpandtab
