# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4

set big	[string repeat a [expr {int(1e8)}]]	;# Allocate 100MB to pre-expand the zippy pool
unset big

set this_script	[file normalize [info script]]
set seen	{}
while {[file type $this_script] eq "link"} {
	dict set seen $this_script	1
	set this_script	[file normalize [file join [file dirname $this_script] [file readlink $this_script]]]
	if {[dict exists $seen $this_script]} {
		error "Symlink loop detected while trying to resolve [file normalize [info script]]"
	}
}
unset seen
set here	[file dirname $this_script]
tcl::tm::path add $here

package require teabase_bench

proc main {} {
	global here
	try {
		puts "[string repeat - 80]\nStarting benchmarks\n"
		teabase_bench::run_benchmarks [file dirname [file normalize [info script]]] {*}$::argv
	} on ok {} {
		exit 0
	} trap {BENCH BAD_RESULT} {errmsg options} {
		puts stderr $errmsg
		exit 1
	} trap {BENCH BAD_CODE} {errmsg options} {
		puts stderr $errmsg
		exit 1
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
