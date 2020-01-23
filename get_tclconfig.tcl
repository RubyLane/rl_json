#!/usr/bin/env tclsh

package require http
try {
	package require tls
} on ok ver {
	puts "Have tls $ver"
	http::register https 443 ::tls::socket
} on error {} {}

cd [file dirname [file normalize [info script]]]
set url		https://core.tcl-lang.org/tclconfig/tarball/trunk/tclconfig.tar.gz
while {[incr tries] <= 5} {
	set token	[http::geturl $url]
	switch -glob -- [http::ncode $token] {
		30* {
			foreach {k v} [http::meta $token] {
				if {[string tolower $k] eq "location"} {
					set url	$v
					puts "Followed [http::ncode $token] redirect to $url"
					break
				}
			}
		}

		5* {
			puts stderr "Server error [http::ncode $token], delaying 1s and trying again"
			after 1000
		}

		2* {
			file delete -force -- tclconfig
			set h	[open |[list tar xzf -] wb+]
			try {
				puts -nonewline $h [http::data $token]
				chan close $h write
				read $h
				close $h
			} on ok {} {
				puts "Refreshed tclconfig with the latest version from core.tcl-lang.org"
				exit 0
			} trap CHILDSTATUS {errmsg options} {
				set status	[lindex [dict get $options -errorinfo] 2]
				puts stderr "tar exited with status $status"
				exit $status
			} trap CHILDKILLED {errmsg options} {
				set sig		[lindex [dict get $options -errorinfo] 2]
				puts stderr "tar killed by signal $sig"
				exit 1
			} finally {
				if {[info exists h] && $h in [chan names]} {
					close $h
				}
			}
		}

		default {
			puts stderr "Fetching tclconfig failed: [http::ncode $token] [http::error $token] [http::data $token]"
			parray $token
			exit 1
		}
	}
}

puts "Gave up after [expr {$tries-1}] attempts"
exit 1
