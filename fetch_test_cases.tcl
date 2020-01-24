#!/usr/bin/env tclsh

# Dependencies:
#	rl_json:		https://github.com/RubyLane/rl_json
#	parse_args: 	https://github.com/RubyLane/parse_args
#	rl_http:		https://github.com/RubyLane/rl_http
#	urlencode:		copied to support/

set here	[file dirname [file normalize [info script]]]

tcl::tm::path add [file join $here support]

package require rl_json		;# yeah...
package require rl_http
package require parse_args
package require urlencode

interp alias {} json {} ::rl_json::json
interp alias {} parse_args {} ::parse_args::parse_args

proc http {method url args} { #<<<
	parse_args $args {
		-log				{-default {{lvl msg} {}}}
		-if_modified_since	{-# {seconds since 1970-01-01 00:00:00 UTC}}
	}

	set headers	{}

	if {[info exists if_modified_since]} {
		lappend headers If-Modified-Since [clock format $if_modified_since -format {%a, %d %b %Y %H:%M:%S GMT} -gmt 1]
		#puts "If-Modified-Since: [lindex $headers end]"
	}

	while {[incr tries] <= 3} {
		try {
			apply $log notice "Fetching $method $url"
			rl_http instvar h $method $url -headers $headers
		} on error {errmsg options} {
			throw [list HTTP RL_HTTP {*}[dict get $options -errorcode]] $errmsg
		}

		switch -glob -- [$h code] {
			2*               {return [$h body]}
			304		         {throw [list HTTP CODE [$h code]] "Not modified"}
			301 - 302 - 307  {set url [lindex [dict get [$h headers] location] 0]}
			403 {
				if {[dict exists [$h headers] x-ratelimit-limit]} { # Rate limiting <<<
					set limit		[lindex [dict get [$h headers] x-ratelimit-limit] 0]
					set remaining	[lindex [dict get [$h headers] x-ratelimit-remaining] 0]
					set reset		[lindex [dict get [$h headers] x-ratelimit-reset] 0]
					throw [list HTTP RATELIMIT $reset $limit $remaining] "Rate limited until [clock format $reset]"
					#>>>
				}

				throw [list HTTP CODE [$h code]] [$h body]
			}
			503 {
				apply $log warning "Got 503 error, waiting and trying again"
				after 2000
			}
			default {
				throw [list HTTP CODE [$h code]] "Error fetching $method $url: [$h code]\n[$h body]"
			}
		}
	}

	throw [list HTTP TOO_MANY_TRIES [expr {$tries-1}]] "Ran out of patience fetching $method $endoint after $tries failures"
}

#>>>

namespace eval ::github {
	namespace export *
	namespace ensemble create -prefixes no

	proc endpoint args { #<<<
		parse_args $args {
			-owner		{-required}
			-repo		{-required}
			args		{-name path_parts}
		}

		string cat \
			https://api.github.com/repos/ \
			[urlencode rfc_urlencode -part path -- $owner] \
			/ \
			[urlencode rfc_urlencode -part path -- $repo] \
			/contents/ \
			[join [lmap e $path_parts {urlencode rfc_urlencode -part path -- $e}] /]
	}

	#>>>

	proc api {method args} { #<<<
		set endpoint	[github endpoint {*}$args]

		http $method $endpoint
	}

	#>>>
}

proc writetext {fn data} { #<<<
	set h	[open $fn w]
	try {
		puts -nonewline $h $data
	} finally {
		close $h
	}
}

#>>>
proc writebin {fn data} { #<<<
	set h	[open $fn wb]
	try {
		puts -nonewline $h $data
	} finally {
		close $h
	}
}

#>>>

parse_args $argv {
	-pretty		{-boolean}
}

set dest	[file join $here tests JSONTestSuite test_parsing]
file mkdir $dest

set listing	[github api GET -owner nst -repo JSONTestSuite test_parsing]

#puts [json pretty $listing]

json foreach file $listing {
	set fn	[file join $dest [json get $file name]]
	if {[file exists $fn]} {
		set mtime	[file mtime $fn]
		try {
			puts -nonewline "Fetching [json get $file name] -if_modified_since $mtime"
			http GET [json get $file download_url] -if_modified_since $mtime
		} on ok contents {
			puts " [string length $contents] bytes"
			writebin [file join $dest [json get $file name]] $contents
		} trap {HTTP CODE 304} {} {
			puts " not modified"
			continue
		} on error {errmsg options} {
			puts " Error: $errmsg"
		}
	} else {
		try {
			puts -nonewline "Fetching [json get $file name]"
			http GET [json get $file download_url]
		} on ok contents {
			puts " [string length $contents] bytes"
			writebin [file join $dest [json get $file name]] $contents
		} on error {errmsg options} {
			puts " Error: $errmsg"
		}
	}
}

# vim: foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
