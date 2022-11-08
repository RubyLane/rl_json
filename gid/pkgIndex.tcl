#
# Tcl package index file
#
proc load_library_rl_json { dir basename } {
    if { $::tcl_platform(pointerSize) == 8 } {
	set bits 64
    } else {
	set bits 32
    }
    set debug ""
    # set debug "d"
    if { $::tcl_platform(platform) eq "windows"} {
	set library ${basename}_${bits}${debug}[ info sharedlibextension]
    } else {
	set library lib${basename}_${bits}${debug}[ info sharedlibextension]
    }
    load [file join $dir $library] $basename
}

package ifneeded rl_json 0.11.4 [list load_library_rl_json $dir rl_json]

