package require parse_args

namespace eval ::urlencode {
	namespace export *
	namespace ensemble create -prefixes no

	namespace eval _uri {
		variable uri_charmap

		# Set up classvar uri_charmap <<<
		# The reason that this is here is so that the sets, lists and charmaps
		# are generated only once, making the instantiation of uri objects much
		# lighter
		array set uri_common {
			reserved	{; ? : @ & = + $ ,}
			lowalpha	{a b c d e f g h i j k l m n o p q r s t u v w x y z}
			upalpha		{A B C D E F G H I J K L M N O P Q R S T U V W X Y Z}
			digit		{0 1 2 3 4 5 6 7 8 9}
			mark		{- _ . ! ~ * ' ( )}
			alpha		{}
			alphanum	{}
			unreserved	{}
		}

		set uri_common(alpha)		[concat $uri_common(lowalpha) $uri_common(upalpha)]
		set uri_common(alphanum)	[concat $uri_common(alpha) $uri_common(digit)]
		set uri_common(unreserved)	[concat $uri_common(alphanum) $uri_common(mark)]

		variable uri_charmap
		variable uri_charmap_compat
		variable uri_charmap_path
		variable uri_charmap_path_compat
		variable uri_decode_min	{}
		variable uri_decode	{}

		for {set i 0} {$i < 256} {incr i} {
			set c	[binary format c $i]
			if {$c in $uri_common(unreserved)} {
				lappend uri_charmap	$c
			} else {
				lappend uri_charmap	[format %%%02x $i]
			}
			if {$c in $uri_common(alphanum)} {
				lappend uri_charmap_compat	$c
			} else {
				lappend uri_charmap_compat	[format %%%02x $i]
				lappend uri_decode_min		[format %%%02x $i] $c [format %%%02X $i] $c
			}
			lappend uri_decode [format %%%02x $i] $c [format %%%02X $i] $c
		}
		lappend uri_decode + " "
		lappend uri_decode_min + " "
		unset uri_common

		set uri_charmap_path	$uri_charmap
		lset uri_charmap 0x2f /
		set uri_charmap_path_compat	$uri_charmap_compat
		lset uri_charmap_compat 0x2f /

		# Set up classvar uri_charmap >>>
		proc percent_encode {charset map data} { #<<<
			binary scan [encoding convertto $charset $data] cu* byteslist
			set out	""
			foreach byte $byteslist {
				append out	[lindex $map $byte]
			}
			set out
		}

		#>>>
		proc percent_decode {charset data} { #<<<
			variable uri_decode
			encoding convertfrom $charset [string map $uri_decode $data]
		}

		#>>>
	}
	proc rfc_urlencode args { #<<<
		# besides alphanumeric characters, the valid un-encoded characters are: - _ . ! ~ * ' ( )
		parse_args::parse_args $args {
			-charset	{-default utf-8}
			-part		{-enum {query path} -default query}
			str			{-required}
		}

		switch -- $part {
			query { set map		$::urlencode::_uri::uri_charmap }
			path  { set map		$::urlencode::_uri::uri_charmap_path }
		}

		::urlencode::_uri::percent_encode $charset $map $str
	}

	#>>>
	proc rfc_urldecode args { #<<<
		parse_args::parse_args $args {
			-charset	{-default utf-8}
			str			{-required}
		}

		# the encoding convertto here is in case we're passed unicode, not percent encoded utf-8
		::urlencode::_uri::percent_decode $charset [encoding convertto utf-8 $str]
	}

	#>>>
	proc encode_query args { #<<<
		switch -- [llength $args] {
			0       { return }
			1       { set params	[lindex $args 0] }
			default { set params $args }
		}

		if {[llength $params] == 0} return

		string cat ? [join [lmap {k v} $params {
			if {$v eq ""} {
				rfc_urlencode -- $k $v
			} else {
				format %s=%s [rfc_urlencode -- $k] [rfc_urlencode -- $v]
			}
		}] &]
	}

	#>>>
	proc decode_query q { #<<<
		set params	{}
		foreach part [split $q &] {
			lassign [split $part =] ke ve
			lappend params [rfc_urldecode -- $ke] [rfc_urldecode -- $ve]
		}
		set params
	}

	#>>>
}

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
