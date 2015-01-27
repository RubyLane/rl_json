package require math::statistics

namespace eval bench {
	namespace export *

	variable match		*
	variable run		{}
	variable skipped	{}

	# Override this to a new lambda to capture the output
	variable output {
		{lvl msg} {puts $msg}
	}

	namespace path [concat [namespace path] {
		::tcl::mathop
	}]

proc output {lvl msg} { #<<<
	variable output
	tailcall apply $output $lvl $msg
}

#>>>
proc _intersect3 {list1 list2} { #<<<
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
proc _readfile fn { #<<<
	set h	[open $fn r]
	try {
		read $h
	} finally {
		close $h
	}
}

#>>>
proc _writefile {fn dat} { #<<<
	set h	[open $fn w]
	try {
		puts -nonewline $h $dat
	} finally {
		close $h
	}
}

#>>>
proc _run_if_set script { #<<<
	if {$script eq ""} return
	uplevel 2 $script
}

#>>>
proc _verify_res {variant retcodes expected match_mode got options} { #<<<
	if {[dict get $options -code] ni $retcodes} {
		bench::output error "Error: $got"
		throw [list BENCH BAD_CODE $variant $retcodes [dict get $options -code]] \
			"$variant: Expected codes [list $retcodes], got [dict get $options -code]"
	}

	switch -- $match_mode {
		exact  { if {$got eq $expected} return }
		glob   { if {[string match $expected $got]} return }
		regexp { if {[regexp $expected $got]} return }
		default {
			throw [list BENCH BAD_MATCH_MODE $match_mode] \
				"Invalid match mode \"$match_mode\", should be one of exact, glob, regexp"
		}
	}

	throw [list BENCH BAD_RESULT $variant $match_mode $expected $got] \
		"$variant: Expected ($match_mode): -----------\n$expected\nGot: ----------\n$got"
}

#>>>
proc _make_stats times { #<<<
	set res	{}

	foreach stat {
		arithmetic_mean min max number_of_data
		sample_stddev sample_var
		population_stddev population_var
	} val [math::statistics::basic-stats $times] {
		dict set res $stat $val
	}
	dict set res median [math::statistics::median $times]
	dict set res harmonic_mean [/ [llength $times] [+ {*}[lmap time $times {
		/ 1.0 $time
	}]]]
}

#>>>
proc bench {name desc args} { #<<<
	variable match
	variable run
	variable skipped
	variable output

	array set opts {
		-setup			{}
		-compare		{}
		-cleanup		{}
		-batch			100
		-match			exact
		-returnCodes	{ok return}
	}
	array set opts $args
	set badargs [lindex [_intersect3 [array names opts] {
		-setup -compare -cleanup -batch -match -result -returnCodes
	}] 0]

	if {[llength $badargs] > 0} {
		error "Unrecognised arguments: [join $badargs {, }]"
	}

	if {![string match $match $name]} {
		lappend skipped $name
		return
	}

	set normalized_codes	[lmap e $opts(-returnCodes) {
		switch -- $e {
			ok			{list 0}
			error		{list 1}
			return		{list 2}
			break		{list 3}
			continue	{list 4}
			default		{set e}
		}
	}]

	set make_script {
		{batch script} {
			format {set __bench_i %d; while {[incr __bench_i -1] > 0} %s} \
				[list $batch] [list $script]
		}
	}

	# Measure the instrumentation overhead to compensate for it
	set start		[clock microseconds]	;# Prime [clock microseconds], start var
	set times	{}
	set script	[apply $make_script $opts(-batch) list]
	for {set i 0} {$i < 1000} {incr i} {
		set start [clock microseconds]
		uplevel 1 $script
		lappend times [- [clock microseconds] $start]
	}
	set overhead	[::tcl::mathfunc::min {*}[lmap e $times {expr {$e / double($opts(-batch))}}]]
	#apply $output debug [format {Overhead: %.3f usec} $overhead]


	set variant_stats {}

	_run_if_set $opts(-setup)
	try {
		dict for {variant script} $opts(-compare) {
			set times	{}
			set it		0
			set begin	[clock microseconds]

			if {[info exists opts(-result)]} {
				set start	[clock microseconds]
				catch {uplevel 1 $script} r o
				lappend times	[- [clock microseconds] $start]
				incr it
				_verify_res $variant $normalized_codes $opts(-result) $opts(-match) $r $o
			}

			set bscript	[apply $make_script $opts(-batch) $script]
			while {[llength $times] < 10 || [- [clock microseconds] $begin] < 500000} {
				set start [clock microseconds]
				uplevel 1 $bscript
				lappend times [expr {
					([clock microseconds] - $start) / double($opts(-batch)) - $overhead
				}]
			}

			dict set variant_stats $variant [_make_stats $times]
		}

		lappend run $name $desc $variant_stats
	} finally {
		_run_if_set $opts(-cleanup)
	}
}; namespace export bench

#>>>
namespace eval display_bench {
	namespace export *
	namespace ensemble create
	namespace path [concat [namespace path] {
		::tcl::mathop
	}]

	proc _heading {txt {char -}} { #<<<
		format {%s %s %s} \
			[string repeat $char 2] \
			$txt \
			[string repeat $char [- 80 5 [string length $txt]]]
	}

	#>>>
	proc short {name desc variant_stats relative {pick median}} { #<<<
		variable output

		bench::output notice [_heading [format {%s: "%s"} $name $desc]]

		# Gather the union of all the variant names from this and past runs
		set variants	[dict keys $variant_stats]
		foreach {label past_run} $relative {
			foreach past_variant [dict keys $past_run] {
				if {$past_variant ni $variants} {
					lappend variants $past_variant
				}
			}
		}

		# Assemble the tabular data
		lappend rows	[list "" "This run" {*}[dict keys $relative]]
		lappend rows	{*}[lmap variant $variants {
			unset -nocomplain baseline
			set past_stats	[dict values $relative]
			list $variant {*}[lmap stats [list $variant_stats {*}$past_stats] {
				if {![dict exists $stats $variant]} {
					#string cat --
					set _ --
				} else {
					set val		[dict get $stats $variant $pick]
					if {![info exists baseline]} {
						set baseline	$val
						format %.3f $val
					} elseif {$baseline == 0} {
						format x%s inf
					} else {
						format x%.3f [/ $val $baseline]
					}
				}
			}]
		}]

		# Determine the column widths
		set colsize	{}
		foreach row $rows {
			set cols	[llength $row]
			for {set c 0} {$c < $cols} {incr c} {
				set this_colsize	[string length [lindex $row $c]]
				if {
					![dict exists $colsize $c] ||
					[dict get $colsize $c] < $this_colsize
				} {
					dict set colsize $c $this_colsize
				}
			}
		}

		# Construct the row format description
		set col_fmts	[lmap {c size} $colsize {
			#string cat %${size}s
			set _ %${size}s
		}]
		set fmt	"   [join $col_fmts { | }]"
		set col_count	[llength $col_fmts]

		# Output the row data
		foreach row $rows {
			set data	[list {*}$row {*}[lrepeat [- $col_count [llength $row]] --]]
			bench::output notice [format $fmt {*}$data]
		}

		bench::output notice ""
	}

	#>>>
}
proc run_benchmarks {dir args} { #<<<
	variable skipped
	variable run
	variable match
	variable output

	set match				*
	set relative			{}
	set display_mode		short
	set display_mode_args	{}

	set consume_args [list  \
		count {
			upvar 1 i i args args
			set from	$i
			incr i $count
			lrange $args $from [+ $from $count -1]
		} [namespace current] \
	]

	set i	0
	while {$i < [llength $args]} {
		lassign [apply $consume_args 1] next

		switch -- $next {
			-match {
				lassign [apply $consume_args 1] match
			}

			-relative {
				lassign [apply $consume_args 2] label rel_fn
				dict set relative $label [_readfile $rel_fn]
			}

			-save {
				lassign [apply $consume_args 1] save_fn
			}

			-display {
				lassign [apply $consume_args 1] display_mode
				set display_mode_args	{}
				while {[string index [lindex $args $i] 0] ni {"-" ""}} {
					lappend display_mode_args	[apply $consume_args 1]
				}
			}

			default {
				throw [list BENCH INVALID_ARG $next] \
					"Invalid argument: \"$next\""
			}
		}
	}

	set stats	{}
	foreach f [glob -nocomplain -type f -dir $dir -tails *.bench] {
		uplevel 1 [list source [file join $dir $f]]
	}

	if {[info exists save_fn]} {
		_writefile $save_fn $run
	}

	foreach {name desc variant_stats} $run {
		set relative_stats	{}
		foreach {label relinfo} $relative {
			foreach {relname reldesc relstats} $relinfo {
				if {$relname eq $name} {
					lappend relative_stats $label $relstats
					break
				}
			}
		}
		try {
			display_bench $display_mode $name $desc $variant_stats $relative_stats {*}$display_mode_args
		} trap {TCL LOOKUP SUBCOMMAND} {errmsg options} {
			puts $options
			apply $output error "Invalid display mode: \"$display_mode\""
		} trap {TCL WRONGARGS} {errmsg options} {
			apply $output error "Invalid display mode params: $errmsg"
		}
	}
}

#>>>
}
# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
