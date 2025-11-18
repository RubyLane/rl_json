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
