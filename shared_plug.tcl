###############################################################################
# Some useful functions copied/adopted from OSS.tcl ###########################
###############################################################################

namespace eval __PLUG__ {

set PLSP(OPTIONS) { match number skip string start save restore then return
			subst checkpoint time }

# Current (working) parsed line + position of last action
set PLSP(CUR) ""
set PLSP(POS) 0
set PLSP(THN) 0

proc pl_keymatch { key klist } {
#
# Finds the closest match of key to keys in klist
#
	set res ""
	foreach k $klist {
		if { [string first $key $k] == 0 } {
			lappend res $k
		}
	}

	if { $res == "" } {
		error "$key not found"
	}

	if { [llength $res] > 1 } {
		error "multiple matches for $key: [join $res]"
	}

	return [lindex $res 0]
}

proc pl_isalnum { txt } {

	return [regexp {^[[:alpha:]_][[:alnum:]_]*$} $txt]
}

proc pl_valint { n { min "" } { max "" } } {
#
# Validate an integer value
#
	set n [string tolower [string trim $n]]
	if { $n == "" } {
		error "empty string"
	}

	if { [string first "." $n] >= 0 || [string first "e" $n] >= 0 } {
		error "not an integer number"
	}

	if [catch { expr $n } n] {
		error "not a number"
	}

	if { $min != "" && $n < $min } {
		error "must not be less than $min"
	}

	if { $max != "" && $n > $max } {
		error "must not be greater than $max"
	}

	return $n
}

proc error_pl_parse { msg } {

	error "pl_parse error, $msg"
}

proc parse_subst { a r l p } {
#
# Variable substitution in the command
#
	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	set ns [lindex $args 0]

	if { $ns != "" && [string index $ns 0] != "-" } {
		# the namespace
		set args [lrange $args 1 end]
	} else {
		set ns "::__plug__"
	}

	set line [uplevel #0 "namespace eval $ns { subst { $line } }"]

	lappend res $line
	set ptr 0

	return 0
}

proc parse_skip { a r l p } {
#
# Skip spaces (or the indicated characters), returns the first non-skipped
# character
#
	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	set na [lindex $args 0]

	if { $na != "" && [string index $na 0] != "-" } {
		# use as the set of characters
		set args [lrange $args 1 end]
		set line [string trimleft $line $na]
	} else {
		set line [string trimleft $line]
	}

	lappend res [string index $line 0]

	# pointer always at the beginning
	set ptr 0

	# success; this one always succeeds
	return 0
}

proc parse_match { a r l p } {

	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	set pat [lindex $args 0]
	set args [lrange $args 1 end]

	if { $pat == "" } {
		error_pl_parse "illegal empty pattern for -match"
	}

	if [catch { regexp -inline -indices -- $pat $line } mat] {
		error_pl_parse "illegal pattern for -match, $mat"
	}

	if { $mat == "" } {
		# no match at all, failure
		return 1
	}

	set ix [lindex [lindex $mat 0] 0]
	set iy [lindex [lindex $mat 0] 1]

	foreach m $mat {
		set fr [lindex $m 0]
		set to [lindex $m 1]
		lappend res [string range $line $fr $to]
	}

	set line [string replace $line $ix $iy]
	set ptr $ix

	return 0
}

proc parse_number { a r l p } {
#
# Parses something that should amount to a number, which can be an expression
#
	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	if [regexp {^[[:space:]]*"} $line] {
		# trc "PNUM: STRING!"
		# a special check for a string which fares fine as an
		# expression, but we don't want it; if it doesn't open the
		# expression, but occurs inside, that's fine
		return 1
	}

	set ll [string length $line]
	set ix $ll
	# trc "PNUM: $ix <$line>"

	while 1 {

		incr ix -1
		if { $ix < 0 } {
			# failure
			return 1
		}

		if ![catch { expr [string range $line 0 $ix] } val] {
			# found, remove the match
			set line [string range $line [expr $ix + 1] end]
			lappend res $val
			set ptr 0
			return 0
		}
	}
}

proc parse_time { a r l p } {
#
# Parses a time string than can be parsed by clock scan
#
	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	set ll [string length $line]
	set ix $ll

	while 1 {

		incr ix -1
		if { $ix < 0 } {
			# failure
			return 1
		}

		if ![catch { clock scan [string range $line 0 $ix] } val] {
			# found, remove the match
			set line [string range $line [expr $ix + 1] end]
			lappend res $val
			set ptr 0
			return 0
		}
	}
}

proc parse_string { a r l p } {
#
# Parses a string
#
	upvar $a args
	upvar $r res
	upvar $l line
	upvar $p ptr

	set par [lindex $args 0]

	if { $par != "" && [string index $par 0] != "-" } {
		# this is our parameter, string length
		set args [lrange $args 1 end]
		if [catch { pl_valint $par 0 1024 } par] {
			error_pl_parse "illegal parameter for -string, $par,\
				must be a number between 0 and 1024"
		}
	} else {
		# apply delimiters
		set par ""
	}

	if { $par == "" } {
		set nc [string index $line 0]
		if { $nc != "\"" } {
			# failure, must start with "
			return 1
		}
		set mline [string range $line 1 end]
	} else {
		set mline $line
	}

	set vals ""
	set nchs 0

	while 1 {

		if { $par != "" && $par != 0 && $nchs == $par } {
			# we have the required number of characters
			break
		}

		set nc [string index $mline 0]

		if { $nc == "" } {
			if { $par != "" } {
				# this is OK
				break
			}
			# assume no match
			return 1
		}

		set mline [string range $mline 1 end]

		if { $par == "" && $nc == "\"" } {
			# done 
			break
		}

		if { $nc == "\\" } {
			# escapes
			set c [string index $mline 0]
			if { $c == "" } {
				# delimiter error, will be diagnosed at next
				# turn
				continue
			}
			if { $c == "x" } {
				# get hex digits
				set mline [string range $mline 1 end]
				while 1 {
					set d [string index $mline 0]
					if ![ishex $d] {
						break
					}
					append c $d
					set mline [string range $mline 1 end]
				}
				if [catch { expr 0$c % 256 } val] {
					error "illegal escape in -string 0$c"
				}
				lappend vals $val
				incr nchs
				continue
			}
			if [isoct $c] {
				if { $c != 0 } {
					set c "0$c"
				}
				# get octal digits
				set mline [string range $mline 1 end]
				while 1 {
					set d [string index $mline 0]
					if ![isoct $d] {
						break
					}
					append c $d
					set mline [string range $mline 1 end]
				}
				if [catch { expr $c % 256 } val] {
					error "illegal escape in -string $c"
				}
				lappend vals $val
				incr nchs
				continue
			}
			set mline [string range $mline 1 end]
			set nc $c
			continue
		}
		scan $nc %c val
		lappend vals [expr $val % 256]
		incr nchs
	}

	lappend res $vals
	set ptr 0
	set line $mline
	return 0
}

proc pl_parse { args } {
#
# The parser
#
	variable PLSP
	variable plss

	set res ""
	# checkpoint
	set chk ""

	while { $args != "" } {

		set what [lindex $args 0]
		set args [lrange $args 1 end]

		if { [string index $what 0] != "-" } {
			error_pl_parse "selector $what doesn't start with '-'"
		}

		set what [string range $what 1 end]

		if [catch { pl_keymatch $what $PLSP(OPTIONS) } w] {
			error_pl_parse $w
		}
		
		# those that do not return values are serviced directly
		switch $w {

			"start" {

				set PLSP(CUR) [lindex $args 0]
				set args [lrange $args 1 end]
				set PLSP(POS) 0
				set PLSP(THN) 0
				# remove any saves
				array unset plss
				continue
			}

			"restore" {

				set p [lindex $args 0]
				if { $p != "" && [string index $p 0] != "-" } {
					if ![pl_isalnum $p] {
						error_pl_parse "illegal\
						  -restore tag, must be\
						  alphanumeric"
					}
					set args [lrange $args 1 end]
					
				} else {
					set p "+"
				}
				if ![info exists plss($p)] {
					error_pl_parse "-restore tag not found"
				}
				set PLSP(CUR) [lindex $plss($p) 0]
				set PLSP(POS) [lindex $plss($p) 1]
				set PLSP(THN) [lindex $plss($p) 2]
				continue
			}

			"save" {

				set p [lindex $args 0]
				if { $p != "" && [string index $p 0] != "-" } {
					if ![pl_isalnum $p] {
						error_pl_parse "illegal\
						  -save tag, must be\
						  alphanumeric"
					}
					set args [lrange $args 1 end]
					
				} else {
					set p "+"
				}
				set plss($p) [list $PLSP(CUR) $PLSP(POS) \
					$PLSP(THN)]
				continue
			}

			"then" {
				set PLSP(THN) 1
				continue
			}

			"return" {
				set na [lindex $args 0]
				if { $na != "" &&
				    [string index $na 0] != "-" } {
					if [catch { pl_valint $na 0 1024 } \
					    na] {
						error_pl_parse "illegal\
						    argument of -return, $na"
					}
					set res [lindex $res $na]
				}
				return $res
			}

			"checkpoint" {
				set chk [list $PLSP(CUR) $PLSP(POS) $PLSP(THN)]
				continue
			}
		}

		set ptr 0
		if $PLSP(THN) {
			# after the current pointer
			set line [string range $PLSP(CUR) $PLSP(POS) end]
		} else {
			set line $PLSP(CUR)
		}

		if [parse_$w args res line ptr] {
			# failure
			if { $chk != "" } {
				set PLSP(CUR) [lindex $chk 0]
				set PLSP(POS) [lindex $chk 1]
				set PLSP(THN) [lindex $chk 2]
			} else {
				set PLSP(THN) 0
			}
			return ""
		}

		if $PLSP(THN) {
			set PLSP(CUR) "[string range $PLSP(CUR) 0 \
				[expr $PLSP(POS) - 1]]$line"
			incr PLSP(POS) $ptr
		} else {
			set PLSP(POS) $ptr
			set PLSP(CUR) $line
		}
		set PLSP(THN) 0
	}

	return $res
}

namespace export pl_*

###############################################################################
# END __PLUG__ NAMESPACE ######################################################
###############################################################################

}

namespace import ::__PLUG__::*

###############################################################################
###############################################################################
###############################################################################

namespace eval __plug__ {

###############################################################################

variable CODES

set CODES(RC_OK)  			0
set CODES(RC_EVAL)  			1
set CODES(RC_EPAR)  			2
set CODES(RC_EADDR) 			3
set CODES(RC_ENIMP)			4
set CODES(RC_DUPOK)			5
set CODES(RC_ELEN)                      6
set CODES(RC_ERES)                      7

set CODES(CMD_GET)			1
set CODES(CMD_SET)			2

set CODES(CMD_SET_ASSOC)		0x13
set CODES(CMD_GET_ASSOC)		0x14
set CODES(CMD_CLR_ASSOC)		0x15
set CODES(CMD_RELAY)			0x41

###############################################################################

variable CMDS

set CMDS(getparams)	"command_getparams"
set CMDS(getassoc)	"command_getassoc"
set CMDS(setparams)	"command_setparams"
set CMDS(setassoc)	"command_setassoc"
set CMDS(learning)	"command_learning"
set CMDS(relay)		"command_relay"
set CMDS(send)		"command_send"
set CMDS(control)	"command_control"
set CMDS(script)	"command_script"

###############################################################################

variable RESP

set RESP([expr $CODES(CMD_GET)])		"response_getparams"
set RESP([expr $CODES(CMD_GET_ASSOC)])		"response_getassoc"
set RESP([expr $CODES(CMD_SET)])		"response_setparams"
set RESP([expr $CODES(CMD_SET_ASSOC)])		"response_setassoc"
set RESP([expr $CODES(CMD_CLR_ASSOC)])		"response_clrassoc"
set RESP([expr $CODES(CMD_RELAY)])		"response_relay"

###############################################################################

variable REPO

set REPO(0)		"report_event"
set REPO(1)		"report_relay"
set REPO(209)		"report_log"
set REPO(177)		"report_location"
set REPO(225)		"report_sniff"

###############################################################################

variable LOGTYPES

array set LOGTYPES {
			128	"b 02x"
			129	"w 04x"
			130	"l 08x"
			192	"b 1u"
			193	"w 1u"
			194	"l 1u"
			208	"bs 1d"
			209	"ws 1d"
			210	"ls 1d"
}

###############################################################################

variable BTYPES

array set BTYPES {
			0	"spar0"
			1	"ap320"
			2	"ap321"
			3	"chrob"
			4	"chrow"
			5	"war"
			6	"ap319"
			7	"spar7"
}
			
###############################################################################

proc cns { t } {
#
# Convert the context to the namespace for the plugin instance
#
	return "::__plug__::_ns_$t"
}

proc varvar { v } {
#
# Returns the value of a variable whose name is stored in a variable
#
	upvar $v vv
	return $vv
}

proc get_b { vl } {

	upvar $vl vv

	if { [llength $vv] < 1 } {
		error "get_b, list too short"
	}

	set v [expr { [lindex $vv 0] }]
	set vv [lrange $vv 1 end]
	return $v
}

proc get_w { vl } {

	upvar $vl vv

	if { [llength $vv] < 2 } {
		error "get_w, list too short"
	}

	set v [expr { [lindex $vv 0] | ([lindex $vv 1] << 8) }]
	set vv [lrange $vv 2 end]
	return $v
}

proc get_l { vl } {

	upvar $vl vv

	if { [llength $vv] < 4 } {
		error "get_l, list too short"
	}

	set v [expr {  [lindex $vv 0]        | \
		      ([lindex $vv 1] <<  8) | \
		      ([lindex $vv 2] << 16) | \
		      ([lindex $vv 3] << 24) }]
	set vv [lrange $vv 4 end]
	return $v
}

proc get_bs { vl } {

	upvar $vl vv

	if { [llength $vv] < 1 } {
		error "get_bs, list too short"
	}

	set v [expr { [lindex $vv 0] }]

	if [expr { $v & 0x80 }] {
		set v [expr { $v | 0xffffff00 }]
	}

	set vv [lrange $vv 1 end]
	return $v
}

proc get_ws { vl } {

	upvar $vl vv

	if { [llength $vv] < 2 } {
		error "get_ws, list too short"
	}

	set v [expr { [lindex $vv 0] | ([lindex $vv 1] << 8) }]

	if [expr { $v & 0x8000 }] {
		set v [expr { $v | 0xffff0000 }]
	}

	set vv [lrange $vv 2 end]
	return $v
}

proc get_ls { vl } {

	upvar $vl vv

	if { [llength $vv] < 4 } {
		error "get_ls, list too short"
	}

	set v [expr {  [lindex $vv 0]        | \
		      ([lindex $vv 1] <<  8) | \
		      ([lindex $vv 2] << 16) | \
		      ([lindex $vv 3] << 24) }]
	set vv [lrange $vv 4 end]
	return $v
}

proc put_b { vl v } {

	upvar $vl vv

	lappend vv [expr { $v }]
}

proc put_w { vl v } {

	upvar $vl vv

	lappend vv [expr {  $v       & 0xff }]
	lappend vv [expr { ($v >> 8) & 0xff }]
}

proc put_l { vl v } {

	upvar $vl vv

	lappend vv [expr {  $v        & 0xff }]
	lappend vv [expr { ($v >>  8) & 0xff }]
	lappend vv [expr { ($v >> 16) & 0xff }]
	lappend vv [expr { ($v >> 24) & 0xff }]
}

proc toh { lv } {

	set res ""

	foreach v $lv {
		lappend res [format "%02x" $v]
	}

	return [join $res]
}

proc idmp { lv th t } {

	term_write "${th}: \[[toh $lv]\]" $t
}

proc iinit { ags t } {
#
# Instance init
#
	set ns [cns $t]

	# if we are reopening; not sure if this is going to work
	iclose $t

	namespace eval $ns {

		variable plug

		array set plug {
			dump 		3
			confirm		3
			echo		1
			repeat		0
			quiet		3
			nodeid		""
			osq		1
			lastosq		"none"
			ref		0
			lastref		"none"
			isq		0
			laststat	0
			callbacks	""
			cbcount		0
		}
	}

	while { $ags != "" } {
		if { [string index $ags 0] == "D" } {
			incr ${ns}::plug(dump)
		}
		set ags [string range $ags 1 end]
	}

	term_write "*plugin started*" $t
}

proc iclose { t } {
#
# Close a plugin instance
#
	set ns [cns $t]

	if ![namespace exists $ns] {
		# a precaution, already closed
		return
	}

	# kill all callbacks
	namespace eval $ns {

		variable plug

		foreach c $plug(callbacks) {
			catch { after cancel [lindex $c 1] }
		}
	}

	# delete the namespace

	namespace delete $ns
}

proc icommand { line f t } {
#
# Instance input, i.e., a command line to process
#
#	f - file flag, i.e., running commands from file
#
	variable CMDS

	set ns [cns $t]

	# echo?
	set ec [varvar ${ns}::plug(echo)]

	if $ec {
		if { $f == 0 || $ec > 1 } {
			term_write $line $t
		}
	}

	set cc [pl_parse -start $line -skip -return 0]

	if { $cc == "" || $cc == "#" } {
		# empty line or comment
		return
	}

	if { $cc == ":" } {
		# a command for the interpreter
		set cc [string trim \
			[pl_parse -match "." -match ".*" -return 1]]
		set res [uplevel #0 "namespace eval $ns { $cc }"]
		term_write $res $t
		return
	}
		
	set cmd [pl_parse -subst $ns -skip -match {^[[:alpha:]_][[:alnum:]_]*} \
		-return 2]

	if { $cmd == "" } {
		# no keyword
		error "illegal command syntax, must begin with a keyword"
	}

	set cc [pl_keymatch $cmd [array names CMDS]]
	pl_parse -skip

	$CMDS($cc) $t
}

proc iinput { inp t } {
#
# An encapsulator for icommand to intercept errors
#
	if [catch { icommand $inp 0 $t } err] {
		term_write "!error: $err" $t
	}
}

proc incosq { t } {
#
# Increment the outgoing sequence number
#
	namespace eval [cns $t] {

		variable plug

		if { $plug(osq) == 127 } {
			set plug(osq) 2
		} else {
			incr plug(osq)
		}
	}
}

proc incref { t } {
#
# Increment the outgoing reference number
#
	namespace eval [cns $t] {

		variable plug

		if { $plug(ref) == 127 } {
			set plug(ref) 1
		} else {
			incr plug(ref)
		}
	}
}

proc iissue { code vals t { sopts "" } } {
#
# Issues a command:
#
#	code	- opcode
#	vals	- list of payload bytes
#	sopts	- a dictionary of overriding options (this command only):
#			ack		- yes/no (force ACK)
#			response	- yes/no (force response)
#			sequence	- force specific sequence number
#			opref		- force specific opref
#			repeat		- repetition count
#			local		- local Node Id
#                       remote		- remote Node Id
#
	set ns [cns $t]

	foreach v { confirm repeat osq ref nodeid dump } {
		set $v [varvar ${ns}::plug($v)]
	}

	if [dict exists $sopts "ack"] {
		# forced ACK/NOACK
		if [dict get $sopts "ack"] {
			set confirm [expr { $confirm |  1 }]
		} else {
			set confirm [expr { $confirm & ~1 }]
		}
	}

	if [dict exists $sopts "response"] {
		# forced response/no response
		if [dict get $sopts "response"] {
			set confirm [expr { $confirm |  2 }]
		} else {
			set confirm [expr { $confirm & ~2 }]
		}
	}

	if [dict exists $sopts "sequence"] {
		# forced (single) seqnum
		set osq [dict get $sopts "sequence"]
		# do not increment
		set ins 0
	} else {
		set ins 1
	}

	if [dict exists $sopts "opref"] {
		# forced (single) refnum
		set ref [dict get $sopts "opref"]
		# do not increment
		set inr 0
	} else {
		set inr 1
	}

	if [dict exists $sopts "repeat"] {
		# forced (single) refnum
		set repeat [dict get $sopts "repeat"]
	}
		
	if [expr { $confirm & 1 }] {
		set osq [expr { $osq | 0x80 }]
	}

	if [expr { $confirm & 2 }] {
		set ref [expr { $ref | 0x80 }]
	}

	# sequence number + Node Id + opcode + refnum
	if [dict exists $sopts "local"] {
		set nid [dict get $sopts "local"]
	} elseif { $nodeid == "" } {
		set nid 0
	} else {
		set nid $nodeid
	}

	# remote Node Id
	if [dict exists $sopts "remote"] {
		set rid [dict get $sopts "remote"]
	} else {
		set rid $nid
	}

	set out ""
	put_b out $osq
	put_w out $nid
	put_b out $code
	put_b out $ref
	put_w out $rid
	set out [concat $out $vals]

	if $dump {
		idmp $out "-->" $t
	}

	set ${ns}::plug(lastosq) [expr { $osq & 0x7f }]
	set ${ns}::plug(lastref) [expr { $ref & 0x7f }]

	set len [llength $out]
	if { $len > 82 } {
		error "command too long, $len bytes, 82 bytes is the max"
	}

	while 1 {
		node_write $out $t
		if { $repeat == 0 } {
			break
		}
		incr repeat -1
	}

	if $ins {
		incosq $t
	}

	if $inr {
		incref $t
	}
}

proc ioutput { bts t } {
#
# Instance output, i.e., something arriving from the node
#
	set ns [cns $t]

	if ![namespace exists $ns] {
		# a precaution
		return
	}

	foreach v { nodeid dump quiet } {
		set $v [varvar ${ns}::plug($v)]
	}

	set ln [llength $bts]

	if { $ln < 5 } {
		# garbage
		if $dump {
			idmp $bts "<-E" $t
		}
		return
	}

	set pay $bts
	set seq [get_b pay]
	set nid [get_w pay]
	set opc [get_b pay]

	set arq [expr { $seq & 0x80 }]
	set seq [expr { $seq & 0x7f }]

	if { $nid != 0 && $nodeid != "" && $nid != $nodeid } {
		if { $dump > 2 } {
			idmp $bts "<-N" $t
		}
		return
	}

	if { $opc == 0 } {
		# this is an ACK, check last unacked frame sequence number
		set lastosq [varvar ${ns}::plug(lastosq)]
		if [expr { $quiet & 1 }] {
			# report ACKs?
			if { $lastosq == $seq || [expr { $quiet & 2 }] } {
				# only if expected or showing replicates
				reportack $nid $lastosq $seq $pay $t
			}
		}
		# clear it
		set ${ns}::plug(lastosq) "none"
		if { $dump > 1 } {
			idmp $bts "<-A" $t
		}
		return
	}

	# last received sequence number
	set isq [varvar ${ns}::plug(isq)]

	if { $isq == $seq && ![expr { $quiet & 2 }] } {
		# a replicate, not reporting
		if { $dump > 1 } {
			idmp $bts "<-R" $t
		}
		if $arq {
			# also ACK replicates if ACK requested
			sendack 1 $t
		}
		return
	}

	# receive
	if { $opc == 0xff } {
		# this is a report, minimum length is 5
		set status [ireport $pay $t]
	} else {
		# response, minimum length is 7
		if { $ln < 7 } {
			# still garbage
			if $dump {
				idmp $bts "<-E" $t
			}
			return
		}
		set status [iresponse $opc $pay $t]
	}

	if $dump {
		idmp $bts "<-O" $t
	}

	set ${ns}::plug(laststat) $status
	set ${ns}::plug(isq) $seq

	if $arq {
		sendack 0 $t
	}

}

proc sendack { r t } {
#
# Send an ACK based on last status, r - replicate flags, positive ACKs are
# different for duplicates
#
	variable CODES

	set ns [cns $t]

	foreach v { isq laststat nodeid dump } {
		set $v [varvar ${ns}::plug($v)]
	}

	if { $nodeid != "" } {
		set nid $nodeid
	} else {
		set nid 0
	}

	set vals ""
	put_b vals $isq
	put_w vals $nodeid
	put_b 0
	if { $r && $laststat == 0 } {
		# convert OK to DUPOK
		set laststat $CODES(RC_DUPOK)
	}
	put_b $laststat

	if { $dump > 1 } {
		idmp $vals "A->" $t
	}

	node_write $vals $t
}

proc respcode { c } {
#
# Transform an ACK or response code into string
#
	variable CODES

	foreach d [array names CODES] {
		if { $CODES($d) == $c } {
			if [regexp "^RC_(.+)" $d j m] {
				return $m
			}
		}
	}

	return [format %02x $c]
}

proc reportack { nid lsq seq pay t } {

	set tr "nid=$nid seq=$seq"
	if { $lsq == $seq } {
		set hc "-"
	} else {
		set hc "+"
		if { $lsq == "" } {
			set lsq "none"
		}
		append tr " ($lsq)"
	}

	term_write "${hc}ACK: [respcode [lindex $pay 0]] $tr" $t
}

proc iresponse { opc pay t } {

	variable RESP
	variable CODES

	set opc [expr { $opc }]
	set seq [expr { [get_b pay] & 0x7f }]
	set orc [get_b pay]
	set nid [get_w pay]

	set ns [cns $t]

	set lastref [varvar ${ns}::plug(lastref)]

	if [info exists RESP($opc)] {
		set fun $RESP($opc)
		set sta $CODES(RC_OK)
		set hc "-"
	} else {
		set fun ""
		set sta $CODES(RC_ENIMP)
		set hc "!"
	}

	set res "${hc}RSP: [respcode $orc] nid=$nid ref=$seq"

	if { $seq != $lastref } {
		append res " ($lastref)"
	}

	set ret ""

	if { $fun != "" } {
		set ret [$fun $nid $pay $t sta]
	} elseif { $pay != "" } {
		set ret "<[toh $pay]>"
	}

	if { $ret != "" } {
		append res " $ret"
	}

	term_write $res $t

	return $sta
}

proc tstamp { } {

	return [clock format [clock seconds] -format "%H:%M:%S"]
}

proc ireport { pay t } {

	variable REPO
	variable CODES

	set opc [get_b pay]

	set res "[tstamp]-REP:"

	if [info exists REPO($opc)] {
		set sta $CODES(RC_OK)
		set ret [$REPO($opc) $pay $t sta]
	} else {
		set sta $CODES(RC_ENIMP)
		set ret $opc
		if { $pay != "" } {
			append ret " <[toh $pay]>"
		}
	}

	if { $ret != "" } {
		append res " $ret"
	}

	term_write $res $t

	return $sta
}

###############################################################################
# COMMANDS ####################################################################
###############################################################################

variable PARLIST {
		  { "nodeid"  	  1	sg	w	1	0xfffe	}
		  { "esn"     	  2 	g	lx			}
		  { "master"  	  3 	sg	w	1	0xfffe	}
		  { "netid"   	  4 	sg	w	1	0xfffe	}
		  { "tarplevel"	  5	s	b	0	2	}
		  { "tarprrec"    6	s	b	0	3	}
		  { "tarpslack"	  7	s	b	0	3	}
		  { "tarpfwd"	  8	s	b	0	1	}
		  { "tarpall"  	  9 	sg	bx	0	255	}
		  { "tarpcnt" 	 10 	g	www			}
		  { "tagmgr"  	 11 	sg	by	0	1	}
		  { "audit"   	 12 	sg	w	0	0xffff 	}
		  { "autoack"  	 13 	sg	by	0	1	}
		  { "beacon"  	 14 	sg	w	0	0xffff	}
		  { "version" 	 15 	g	wx			}
		  { "pegmode"	 16	sg	b	0	3	}
		  { "tarprss"	 17	sg	b	0	255	}
		  { "rfchan"     18     sg      b       0       255     }
		  { "uptime"  	 26 	g	l			}
		  { "memstat" 	 27 	g	ww			}
		  { "meminfo" 	 28 	g	ww			}
		  { "sniff"   	 29 	sg	bx	0	0xff	}
	}

variable STDOPTS { "ack" "response" "sequence" "opref" "repeat" "remote"
			"local" }

proc cselector { } {
#
# Parse a selector (-...) in a command
#
	return [pl_parse -skip -match {^-([[:alnum:]]+)} -return 2]
}

proc cbool { } {
#
# Parse a Boolean value: 0,1 or yes,no or on,off
#
	set cc [pl_parse -skip -number -return 1]

	if { $cc == "" } {
		set cc [string tolower \
			[pl_parse -skip -match {^([[:alpha:]]+)} -return 1]]
	} elseif ![catch { pl_valint $cc 0 1 } rr] {
		return $rr
	}

	if { [string first $cc "yes"] == 0 } {
		return 1
	}

	if { [string first $cc "no"] == 0 } {
		return 0
	}

	if { $cc == "on" } {
		return 1
	}

	if { $cc == "off" } {
		return 0
	}

	return ""
}

proc checkempty { } {
#
# Checks if we have parsed the entire line
#
	set cc [pl_parse -skip " \t," -match ".*" -return 1]
	if { $cc != "" } {
		error "illegal argument(s): $cc"
	}
}

proc stdopts { r k } {
#
# Processes the standard options of outgoing commands, one at a time
#
	upvar $r res

	if [dict exists $res $k] {
		error "duplicate option -$k"
	}

	if { $k == "response" || $k == "ack" } {

		set val [cbool]
		if { $val == "" } {
			error "Boolean value expected after -$k"
		}
		dict set res $k $val
		return
	}

	set val [pl_parse -skip -number -return 1]
	if { $val == "" } {
		error "a numerical value expected after -$k"
	}

	set min 0
	if { $k == "repeat" } {
		set max 9
	} elseif { $k == "remote" || $k == "local" } {
		set max 65535
	} else {
		set max 127
		if { $k != "opref" } {
			set min 1
		}
	}

	if [catch { pl_valint $val $min $max } val] {
		error "the argument of -$k must be a numerical value between\
			$min and $max"
	}

	dict set res $k $val
}

proc command_script { t } {
#
# Run commands from file
#
	set slist { "loop" "count" "name" "abort" "show" }

	set ns [cns $t]

	set nams ""

	while 1 {

		set sel [cselector]
		if { $sel == "" } {
			break
		}

		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}

		if { $k == "loop" || $k == "count" } {
			set val [pl_parse -skip -number -return 1]
			if { $val == "" } {
				error "a numerical value expected after -$k"
			}
			if [catch { pl_valint $val 0 } val] {
				error "the argument of -$k cannot be negative"
			}
			set used($k) $val
		} elseif { $k == "abort" } {
			# names are optional
			while 1 {
				set v [pl_parse -skip \
				  -match {^[[:alpha:]_][[:alnum:]_]*} -return 1]
				if { $v == "" } {
					break
				}
				lappend nams $v
			}
			set used(abort) ""
		} elseif { $k == "show" } {
			set used($k) ""
		} else {
			# name, allow multiple names to go with -abort
			while 1 {
				set v [pl_parse -skip \
				  -match {^[[:alpha:]_][[:alnum:]_]*} -return 1]
				if { $v == "" } {
					break
				}
				lappend nams $v
				if ![info exists used(abort)] {
					# just one, unless abort
					break
				}
			}
			if { $nams == "" } {
				error "an identifier expected after -name"
			}
			set used(name) ""
		}
	}

	# check if abort
	if [info exists used(abort)] {

		checkempty

		if { $nams == "" } {
			error "at least one identifier (name) required for\
				-abort"
		}

		if { [info exists used(loop)] || [info exists used(count)] } {
			error "-loop or -count illegal with -abort"
		}

		set res ""
		set count 0
		foreach cb [varvar ${ns}::plug(callbacks)] {
			set nm [lindex $cb 0]
			if { [lsearch -exact $nams $nm] < 0 } {
				lappend res $cb
			} else {
				catch { after cancel [lindex $cb 1] }
				term_write "stopping $nm" $t
				incr count
			}
		}
		if { $count == 0 } {
			term_write "no script stopped" $t
		} else {
			set ${ns}::plug(callbacks) $res
		}
		if [info exists used(show)] {
			showscripts $t
		}
		return
	}

	set dump [varvar ${ns}::plug(dump)]

	# now for the filename
	set fname [string trim [pl_parse -match ".*" -return 0]]
	if { $fname == "" } {
		if [info exists used(show)] {
			# still ok
			showscripts $t
			return
		}
		error "file name missing"
	}

	# read the file
	if [catch { open $fname "r" } fd] {
		error "cannot open file $fname, $fd"
	}

	if [catch { read $fd } cmds] {
		catch { close $fd }
		error "cannot read file $fname, $cmds"
	}

	catch { close $fd }

	set cmds [split $cmds "\n"]

	if ![info exists used(loop)] {
		# execute them now
		if { [info exists used(count)] && $used(count) != 1 } {
			error "without -loop, -count must be 1 or absent"
		}
		foreach cmd $cmds {
			icommand $cmd 1 $t
		}
		return
	} else {
		set delay $used(loop)
	}

	# set up a callback


	if { $nams != "" } {
		if { [llength $nams] > 1 } {
			error "at most one Id expected after -name"
		}
		set name [lindex $nams 0]
	} else {
		set name "f[varvar ${ns}::plug(cbcount)]"
		incr ${ns}::plug(cbcount)
	}

	# check for duplicate
	set fnd 0
	foreach cb [varvar ${ns}::plug(callbacks)] {
		if { [lindex $cb 0] == $name } {
			set fnd 1
			break
		}
	}
	if $fnd {
		error "duplicate script name $name"
	}

	if [info exists used(count)] {
		set count $used(count)
	} else {
		# mean infinity
		set count 0
	}

	if $dump {
		term_write "starting script $name, file $fname, [llength $cmds]\
			lines" $t
	}

	if { $nams == "" } {
		term_write "script name: $name" $t
	}

	# to seconds
	set delay [expr { $delay * 1000 }]

	set sc [list $name \
		[after $delay "::__plug__::runscript $name $t"] \
			$fname $delay $count $cmds]

	lappend ${ns}::plug(callbacks) $sc

	if [info exists used(show)] {
		showscripts $t
	} 
}

proc showscripts { t } {

	set scripts [varvar [cns $t]::plug(callbacks)]

	if { $scripts == "" } {
		term_write "no scripts" $t
		return
	}

	foreach s $scripts {

		lassign $s name cb fname delay count

		if { $count == 0 } {
			set count "infinity"
		}

		term_write "name=$name, file=$fname,\
			delay=[expr { $delay / 1000 }], left=$count" $t
	}
}

proc runscript { id t } {
#
# Runs a background script at intervals
#
	set ns [cns $t]

	if ![namespace exists $ns] {
		return
	}

	set clist [varvar ${ns}::plug(callbacks)]
	set dump [varvar ${ns}::plug(dump)]

	set ix 0
	set fi 0
	foreach cb $clist { 
		if { [lindex $cb 0] == $id } {
			set fi 1
			break
		}
		incr ix
	}

	if !$fi {
		return
	}

	lassign $cb id ci fn de co cs

	if { $dump > 1 } {
		term_write "running script $id" $t
	}

	foreach cmd $cs {
		if [catch { icommand $cmd 1 $t } err] {
			term_write "script $id, file $fn, error: $err" $t
			# force end
			set co 1
		}
	}

	if { $co == 1 } {
		# that was the last turn, remove the script
		if $dump {
			term_write "script $id ends" $t
		}
		set clist [lreplace $clist $ix $ix]
	} else {
		if $co {
			incr co -1
		}
		set clist [lreplace $clist $ix $ix [list $id \
			[after $de "::__plug__::runscript $id $t"] \
				$fn $de $co $cs]]
	}
	set ${ns}::plug(callbacks) $clist
}
	
proc command_control { t } {
#
# This one controls local flags and options (nothing goes out to the node)
#
	set ns [cns $t]

	set slist { "nodeid" "confirm" "dump" "echo" "repeat" "quiet" "show" }

	foreach k [lrange $slist 0 end-1] {
		# copy to locals, no change if error
		set pp($k) [varvar ${ns}::plug($k)]
	}

	while 1 {

		set sel [cselector]
		if { $sel == "" } {
			break
		}
		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}
		set used($k) ""

		switch $k {

		  "confirm" {
			# ack, opref, both, off
			set cc [string tolower [lindex \
				[pl_parse -skip -match {^([[:alpha:]]+)}] 1]]
			if { [string first $cc "ack"] == 0 } {
				set cc 1
			} elseif { [string first $cc "opref"] == 0 } {
				set cc 2
			} elseif { [string first $cc "both"] == 0 } {
				set cc 3
			} elseif { [string first $cc "off"] == 0 } {
				set cc 0
			} else {
				error "illegal argument of -confirm, must be:\
					ack, opref, both, or off"
			}
			set pp(confirm) $cc
		  }

		  "dump" {
			# a number between 0 and 9
			set cc [pl_parse -skip -number -return 1]
			if { $cc == "" } {
				error "a number expected after -dump"
			}
			if [catch { pl_valint $cc 0 9 } cc] {
				error "the argument of -dump must be between 0\
					and 9"
			}
			set pp(dump) $cc
		  }

		  "echo" {
			# on, off, scripts
			set cc [string tolower [lindex \
				[pl_parse -skip -match {^([[:alpha:]]+)}] 1]]
			if { $cc == "on" } {
				set cc 1
			} elseif { [string first $cc "off"] == 0 } {
				set cc 0
			} elseif { [string first $cc "scripts"] == 0 } {
				set cc 2
			} else {
				error "illegal argument of -echo, must be:\
					off, on, or scripts"
			}
			set pp(echo) $cc
		  }

		  "repeat" {
			# a number between 0 and 15
			set cc [pl_parse -skip -number -return 1]
			if { $cc == "" } {
				error "a number expected after -repeat"
			}
			if [catch { pl_valint $cc 0 15 } cc] {
				error "the argument of -repeat must be\
					between 0 and 15"
			}
			set pp(repeat) $cc
		  }

		  "quiet" {
			# on, off, acks, replicates
			set cc [string tolower [lindex \
				[pl_parse -skip -match {^([[:alpha:]]+)}] 1]]
			if { $cc == "on" } {
				set cc 0
			} elseif { [string first $cc "off"] == 0 } {
				set cc 3
			} elseif { [string first $cc "acks"] == 0 } {
				set cc 1
			} elseif { [string first $cc "replicates"] == 0 } {
				set cc 2
			} else {
				error "illegal argument of -quiet, must be:\
					off, on, acks, or replicates"
			}
			set pp(quiet) $cc
		  }

		  "nodeid" {
			set cc [pl_parse -skip -number -return 1]
			if { $cc == "" } {
				error "a number expected after -nodeid"
			}
			if [catch { pl_valint $cc 0 65534 } cc] {
				error "the argument of -nodeid must be\
					between 0 and 65534"
			}

			if { $cc == 0 } {
				set pp(nodeid) ""
			} else {
				set pp(nodeid) $cc
			}
		  }
		}
	}

	checkempty

	foreach k [lrange $slist 0 end-1] {
		# copy to locals, no change if error
		set ${ns}::plug($k) $pp($k)
	}

	if [info exists used(show)] {

		if { $pp(echo) == 0 } {
			set cc "off"
		} elseif { $pp(echo) == 1 } {
			set cc "on"
		} else {
			set cc "scripts"
		}

		if { $pp(nodeid) == "" } {
			set nid "unset"
		} else {
			set nid $pp(nodeid)
		}

		set res "nodeid=$nid, echo=$cc, confirm="

		if { $pp(confirm) == 0 } {
			set cc "off"
		} elseif { $pp(confirm) == 1 } {
			set cc "ack"
		} elseif { $pp(confirm) == 2 } {
			set cc "opref"
		} else {
			set cc "both"
		}

		append res "$cc, quiet="

		if { $pp(quiet) == 0 } {
			set cc "on"
		} elseif { $pp(quiet) == 1 } {
			set cc "acks"
		} elseif { $pp(quiet) == 2 } {
			set cc "replicates"
		} else {
			set cc "off"
		}

		append res "$cc, repeat=$pp(repeat), dump=$pp(dump)"

		term_write $res $t
	}
}

proc command_send { t } {
#
# Set a bunch of raw bytes
#
	variable STDOPTS

	set vals ""
	set sopts ""

	while 1 {
		set sel [cselector]
		if { $sel != "" } {
			# there is a selector
			set k [pl_keymatch $sel $STDOPTS]
			stdopts sopts $k
			continue
		}
		# get a value
		set val [pl_parse -skip -number -return 1]
		if { $val == "" } {
			break
		}
		set val [expr { $val & 0xff }]
		lappend vals $val
	}

	checkempty

	if { $vals == "" } {
		error "no values found"
	}

	iissue [lindex $vals 0] [lrange $vals 1 end] $t $sopts
}

proc command_getparams { t } {

	variable PARLIST
	variable STDOPTS
	variable CODES

	# the list of selectors and the corresponding tag values
	set slist ""
	foreach p $PARLIST {
		if { [string first "g" [lindex $p 2]] >= 0 } {
			set k [lindex $p 0]
			lappend slist $k
			set tlist($k) [lindex $p 1]
		}
	}

	set slist [concat $slist $STDOPTS]
	set sopts ""

	# the list of parameter tags to send in the request
	set vals ""

	while 1 {
		set sel [cselector]
		if { $sel == "" } {
			break
		}

		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}

		set used($k) ""

		if ![info exists tlist($k)] {
			# one of the standard flags
			stdopts sopts $k
			continue
		}
		lappend vals $tlist($k)
	}

	checkempty

	if { $vals == "" } {
		error "no parameters specified"
	}

	iissue $CODES(CMD_GET) $vals $t $sopts
}

proc command_setparams { t } {

	variable PARLIST
	variable STDOPTS
	variable CODES

	# the list of selectors and corresponding records
	set slist ""
	foreach p $PARLIST {
		if { [string first "s" [lindex $p 2]] >= 0 } {
			set k [lindex $p 0]
			lappend slist $k
			set tlist($k) [lrange $p 1 end]
		}
	}

	set slist [concat $slist $STDOPTS]
	set sopts ""

	# the list of settings to send in the request
	set vals ""

	while 1 {
		set sel [cselector]
		if { $sel == "" } {
			break
		}
		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}
		set used($k) ""

		if ![info exists tlist($k)] {
			stdopts sopts $k
			continue
		}

		# parameter tag
		set pt [lindex $tlist($k) 0]
		# value type
		set vt [lindex $tlist($k) 2]
		# boolean?
		if { [string first "y" $vt] > 0 } {
			set bo 1
		} else {
			set bo 0
		}
		# just the first letter
		set w [string index $vt 0]
		# expect the value
		if $bo {
			# a boolean value
			set val [cbool]
			if { $val == "" } {
				error "Boolean value expected after -$k"
			}
		} else {
			# a numerical value
			set val [pl_parse -skip -number -return 1]
			if { $val == "" } {
				error "a numerical value expected after -$k"
			}
			set min [lindex $tlist($k) 3]
			set max [lindex $tlist($k) 4]
			if [catch { pl_valint $val $min $max } val] {
				error "the argument of -$k must be a numerical\
					value between $min and $max"
			}
		}
		lappend vals $pt
		put_$w vals $val
	}

	checkempty

	if { $vals == "" } {
		error "no parameters specified"
	}

	set ll [llength $vals]
	if { $ll > 75 } {
		error "the request is too long ($ll bytes), the maximum is 75\
			bytes"
	}

	iissue $CODES(CMD_SET) $vals $t $sopts
}

proc command_getassoc { t } {
#
# Get the association list
#
	variable STDOPTS
	variable CODES

	set slist $STDOPTS
	lappend slist "from"

	set val 0
	set sopts ""

	while 1 {
		set sel [cselector]
		if { $sel == "" } {
			break
		}

		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}

		set used($k) ""

		if { $k == "from" } {
			set val [pl_parse -skip -number -return 1]
			if { $val == "" } {
				error "a numerical value expected after -from"
			}
			if [catch { pl_valint $val 0 19 } val] {
				error "the argument of -from must be a\
					 numerical value between 0 and 19"
			}
			continue
		}

		stdopts sopts $k
	}

	checkempty

	iissue $CODES(CMD_GET_ASSOC) [list $val] $t $sopts
}

proc command_setassoc { t } {
#
# Set the association list
#
	variable STDOPTS
	variable CODES

	set slist [concat { "clear" "from" "mask" "tags" } $STDOPTS]

	set vals ""
	set from ""
	set msk -1
	set clr 0
	set sopts ""
	set ntags 0

	while 1 {

		set sel [cselector]
		if { $sel == "" } {
			break
		}

		set k [pl_keymatch $sel $slist]

		if { $k == "from" } {

			if { $from != "" } {
				error "duplicate argument -from"
			}
			
			set from [pl_parse -skip -number -return 1]
			if { $from == "" } {
				error "a numerical value expected after -from"
			}
			if [catch { pl_valint $from 0 19 } from] {
				error "the argument of -from must be a\
					numerical value between 0 and 19"
			}
			continue
		}

		if { $k == "mask" } {
			set val [pl_parse -skip -number -return 1]
			if { $val == "" } {
				error "a numerical value expected after -mask"
			}
			if [catch { pl_valint $val 1 255 } val] {
				error "the argument of -mask must be a\
					single-byte, nonzero, unsigned,\
						numerical value"
			}
			set msk $val
			continue
		}

		if { $k == "clear" } {
			set clr 1
			continue
		}

		if { $k == "tags" } {

			if { $msk < 0 } {
				error "-mask must precede -tags"
			}

			# expect a list of numbers
			while 1 {
				set val [pl_parse -skip -number -return 1]
				if { $val == "" } {
					break
				}
				if [catch { pl_valint $val 1 65534 } val] {
					error "a Tag Id must be a number \
						between 1 and 65534"
				}
				if [info exists tags($val)] {
					error "duplicate Tag Id $val"
				}
				set tags($val) ""
				incr ntags
				put_w vals $val
				put_b vals $msk
			}
			continue
		}

		# standard option
		stdopts sopts $k
	}

	checkempty

	if $clr {
		if { $from != "" || $vals != "" } {
			error "-clear cannot be mixed with any other arguments"
		}
		set opc $CODES(CMD_CLR_ASSOC)
		set vals [list 0]
	} else {
		set opc $CODES(CMD_SET_ASSOC)
		if { $from == "" } {
			set from 0
		}
		if { [expr { $ntags + $from }] > 20 } {
			error "too many Tags"
		}
		set vals [concat [list $from] $vals]
	}

	iissue $opc $vals $t $sopts
}

proc command_learning { t } {
#
# Turn the learning mode on and off
#
	variable STDOPTS
	variable CODES

	set slist [concat { "on" "off" "show" } $STDOPTS]
	set which ""
	set sopts ""

	while 1 {

		set sel [cselector]
		if { $sel == "" } {
			break
		}

		set k [pl_keymatch $sel $slist]

		if { $k == "on" || $k == "off" || $k == "show" } {
			if { $which != "" } {
				error "only one of -on, -off, -show can be \
					specified"
			}
			if { $k == "on" } {
				set which 0
			} elseif { $k == "off" } {
				set which 255
			} else {
				set which 1
			}
		} else {
			stdopts sopts $k
		}
	}

	if { $which == "" } {
		set which 0
	}

	checkempty

	if { $which == 1 } {
		# request learning status
		set opc $CODES(CMD_GET_ASSOC)
		set par [list 20]
	} else {
		set opc $CODES(CMD_SET_ASSOC)
		set par [list 20 1 1 $which]
	}

	iissue $opc $par $t $sopts
}

proc command_relay { t } {
#
# Version 1.0 relay
#
	variable STDOPTS
	variable CODES

	set slist [concat { "mode" "destination" } $STDOPTS]
	set which ""
	set sopts ""
	set dest ""
	set payl ""

	while 1 {

		set sel [cselector]
		if { $sel == "" } {
			# check for a payload byte
			set val [pl_parse -skip -number -return 1]
			if { $val == "" } {
				break
			}
			set val [expr { $val & 0xff }]
			lappend payl $val
			continue
		}

		set k [pl_keymatch $sel $slist]
		if [info exists used($k)] {
			error "duplicate parameter -$k"
		}
		set used($k) ""

		if { $k == "mode" } {
			set which [pl_parse -skip -number -return 1]
			if [catch { pl_valint $which 0 2 } which] {
				error "illegal argument of -mode, must be\
					0, 1, or 2"
			}
			continue
		}

		if { $k == "destination" } {
			set dest [pl_parse -skip -number -return 1]
			if [catch { pl_valint $dest 1 65534 } dest] {
				error "illegal destination Id, must be a number\
					between 1 and 65534"
			}
			if { $dest == [varvar [cns $t]::plug(nodeid)] } {
				error "the destination is THIS node"
			}
			continue
		}

		stdopts sopts $k
	}

	checkempty

	if { $which == "" } {
		set which 0
	}

	if { $dest == "" } {
		error "destination not specified"
	}

	if { $payl == "" } {
		error "no payload"
	}

	set opc [expr { $CODES(CMD_RELAY) + $which }]
	set par ""
	put_w par $dest
	iissue $opc [concat $par $payl] $t $sopts
}

###############################################################################
# RESPONSES ###################################################################
###############################################################################

proc response_getparams { nid pay t sta } {

	variable PARLIST
	variable CODES
	upvar $sta status

	set res "getparams:"
	set status $CODES(RC_OK)

	while { $pay != "" } {

		set ord [get_b pay]

		set r ""
		foreach p $PARLIST {
			if { [lindex $p 1] == $ord &&
			    [string first "g" [lindex $p 2]] >= 0 } {
				set r $p
				break
			}
		}

		if { $r == "" } {
			append res " !err=$ord"
			set status $CODES(RC_EPAR)
			break
		}

		set ta [lindex $r 0]
		set vt [lindex $r 3]

		append res " $ta="

		if { $ta == "tarpall" } {
			# a special case 
			if [catch { get_b pay } val] {
				append res "!trunc"
				set status $CODES(RC_EPAR)
				break
			}
			if  [expr { $val & 1 }] {
				set fw "y"
			} else {
				set fw "n"
			}
			set sl [expr { ($val >> 1) & 3 }]
			if  [expr { ($val >> 3) & 1 }] {
				set dw "y"
			} else {
				set dw "n"
			}
			set rr [expr { ($val >> 4) & 3 }]
			set lv [expr { ($val >> 6) & 3 }]
			append res "fw:$fw/sl:$sl/dw:$dw/rr:$rr/lv:$lv"
			continue
		}

		set rrr ""
		set err 0
		while { $vt != "" } {
			set w [string index $vt 0]
			set vt [string range $vt 1 end]
			if [catch { get_$w pay } val] {
				set err 1
				break
			}
			if { 0 && $ta == "nodeid" } {
				# DISABLED!!! set the node Id
				set [cns $t]::plug(nodeid) $val
			}
			if { [string index $vt 0] == "y" } {
				# boolean
				set vt [string range $vt 1 end]
				if $val {
					set val "y"
				} else {
					set val "n"
				}
			} elseif { [string index $vt 0] == "x" } {
				# hex formatting
				if { $w == "l" } {
					set nhx 4
				} elseif { $w == "w" } {
					set nhx 2
				} else {
					set nhx 1
				}
				set vt [string range $vt 1 end]
				set val [format %0${nhx}x $val]
			}
			lappend rrr $val
		}

		if $err {
			append res "!trunc"
			set status $CODES(RC_EPAR)
			break
		}

		append res [join $rrr "/"]
	}

	return $res
}

proc response_getassoc { nid pay t sta } {
#
# Returns the association list or the learning status
#
	variable CODES
	upvar $sta status

	set res "getassoc:"
	set status $CODES(RC_OK)

	# the first byte of payload is the index
	if { $pay == "" } {
		append res " !trunc"
		set status $CODES(RC_EPAR)
		return $res
	}

	set inx [get_b pay]

	if { $inx >= 20 } {
		# this is the learning status
		set res "learning:"
		if [catch { get_b pay } val] {
			append res " !trunc"
			set status $CODES(RC_EPAR)
			return $res
		}
		if { $val == 0 } {
			append res " off"
		} else {
			append res " on"
		}
		return $res
	}

	append res " <$inx>"

	while { $pay != "" } {

		if [catch {
			set tag [get_w pay]
			set msk [get_b pay]
		} ] {
			append res " !trunc"
			set status $CODES(RC_EPAR)
			break
		}
		if $tag {
			set msk [format %02x $msk]
			append res " tag=$tag/mask=$msk"
		}
	}

	return $res
}

proc response_setparams { nid pay t sta } {

	variable CODES
	upvar $sta status

	set status $CODES(RC_OK)
	return "setparams"
}

proc response_setassoc { nid pay t sta } {

	variable CODES
	upvar $sta status

	set status $CODES(RC_OK)
	return "setassoc"
}

proc response_clrassoc { nid pay t sta } {

	variable CODES
	upvar $sta status

	set status $CODES(RC_OK)
	return "clrassoc"
}

proc response_relay { nid pay t sta } {

	variable CODES
	upvar $sta status

	set status $CODES(RC_OK)
	return "relay"
}

###############################################################################
# REPORTS #####################################################################
###############################################################################

proc report_relay { pay t sta } {

	variable CODES
	upvar $sta status

	set res "relay:"

	if [catch {
		set mod [get_b pay]
		set src [get_w pay]
	} ] {
		append res " !trunc"
		set status $CODES(RC_EPAR)
		return $res
	}

	set mod [expr { $mod - $CODES(CMD_RELAY) }]
	if { $mod < 0 || $mod > 2 } {
		set mod "?"
	}

	append res "\[mode=$mod,src=$src\]"

	if { $pay != "" } {
		append res " <[toh $pay]>"
	}

	set status $CODES(RC_OK)

	return $res
}

proc report_event { pay t sta } {

	variable CODES
	variable BTYPES
	upvar $sta status

	set res "event:"

	if [catch {
		set peg [get_w pay]
		set tag [get_w pay]
		set del [get_b pay]
		set vol [get_b pay]
		set rss [get_b pay]
		set xat [get_b pay]
		set etp [get_b pay]
		set seq [get_b pay]
	} ] {
		append res " !trunc"
		set status $CODES(RC_EPAR)
		return $res
	}

	set vol [format %1.2f \
		[expr { ((($vol << 3) + 1000.0) / 4095.0) * 5.0 }]]

	set bt [expr { $etp >> 4 }]

	if [info exists BTYPES($bt)] {
		set bt $BTYPES($bt)
	} else {
		set bt "?"
	}

	set as [expr { $etp & 0xf }]	


	append res " peg=$peg tag=$tag del=$del vlt=$vol rss=$rss"
	append res " xat=[format %02x $xat] dev=$bt but=$as seq=$seq"

	if { $pay != "" } {
		set bb [get_b pay]
		if [expr { $bb & 0x80 }] {
			set ak "no"
		} else {
			set ak "yes"
		}
		set tr [expr { ($bb >> 4) & 0x7 }]
		set gl [expr { $bb & 0xf }]
		append res " glo=$gl try=$tr ack=$ak"
	}

	if { $pay != "" } {
		set bb [get_b pay]
		append res " dia=$bb"
	}

	if { [llength $pay] >= 5 && [get_b pay] } {
		# location report
		set ref [get_w pay]
		set rss ""
		while { $pay != "" } {
			lappend rss [get_b pay]
		}
		append res "\n[out_location $peg $tag $ref $rss]"
	}

	set status $CODES(RC_OK)

	return $res
}

proc nrssv { rss } {
#
# Normalize the RSS vector
#
	set nrs [llength $rss]

	if { $nrs < 2 } {
		error "less than 2 rss values"
	}

	if { $nrs > 8 } {
		# multiple values per PL, must be divisible by 4
		if { [expr { $nrs % 4 }] != 0 } {
			error "rss vector length, $nrs, not divisible by 4"
		}
		set rsv ""
		while { $rss != "" } {
			set av 0
			set ac 0
			for { set i 0 } { $i < 4 } { incr i } {
				set v [lindex $rss $i]
				if { $v != 0 } {
					set av [expr { $av + $v }]
					incr ac
				}
			}
			if { $ac > 1 } {
				set av [expr { ($av + ($ac/2)) / $ac }]
			}
			lappend rsv $av
			set rss [lrange $rss 4 end]
		}
		set rss $rsv
		set nrs [expr { $nrs / 4 }]
	}

	if { $nrs < 8 } {
		# fewer than NPL, stuff with zeros before the last value
		set lv [lindex $rss end]
		set rss [lrange $rss 0 end-1]
		while { $nrs < 8 } {
			lappend rss 0
			incr nrs
		}
		lappend rss $lv
	}

	return $rss
}

proc out_location { peg tag ref rss } {

	set res "location: peg=$peg tag=$tag ref=$ref\n"

	# include the raw vector
	append res "RAW:"
	foreach r $rss {
		append res " [format %3d $r]"
	}
	append res "\nNRM:"

	if [catch { nrssv $rss } rss] {
		append res " BAD RSS VECTOR: $rss!\n"
		return $res
	}

	for { set pl 0 } { $pl < 8 } { incr pl } {
		append res " [format %3d [lindex $rss $pl]]"
	}

	return $res
}

proc report_location { pay t sta } {

	variable CODES

	upvar $sta status

	set res "location:"
	set rss ""

	if [catch {
		set peg [get_w pay]
		set tag [get_w pay]
		set ref [get_w pay]
		for { set i 0 } { $i < 32 } { incr i } {
			lappend rss [get_b pay]
		}
	} ] {
		append res " !trunc"
		set status $CODES(RC_EPAR)
		return $res
	}

	set res [out_location $peg $tag $ref $rss]

	set statuc $CODES(RC_OK)

	return $res
}

proc report_log { pay t sta } {

	variable CODES
	variable LOGTYPES
	upvar $sta status

	set res "log:"
	set status $CODES(RC_OK)

	if [catch {
		set sv [get_b pay]
		set tp [get_b pay]
	} ] {
		append res " !trunc"
		set status $CODES(RC_EPAR)
		return $res
	}

	append res " <sev=$sv,typ=$tp>"

	set more 0
	while { $pay != "" } {

		set tp [get_b pay]

		if { $tp <= 74 } {
			# ASCII characters
			incr tp
			while { $tp } {
				if [catch { get_b pay } b] {
					append res " !trunc"
					set status $CODES(RC_EPAR)
					return $res
				}
				append res [format %c $b]
				incr tp -1
			}
			continue
		}

		if ![info exists LOGTYPES($tp)] {
			set more 1
			break
		}

		# show one value
		if [catch { logvalue res pay $LOGTYPES($tp) }] {
			set status $CODES(RC_EPAR)
			break
		}
	}

	if $more {
		set tp [expr { $tp - 96 }]
		if ![info exists LOGTYPES($tp)] {
			append res " !bad"
			set status $CODES(RC_EPAR)
			return $res
		}
		while { $pay != "" } {
			if [catch { logvalue res pay $LOGTYPES($tp) }] {
				set status $CODES(RC_EPAR)
				break
			}
		}
	}

	return $res
}

proc report_sniff { pay t sta } {

	variable CODES
	upvar $sta status

	set status $CODES(RC_OK)
##
## Tarp header:
##
##	msg_type	1 byte
##	seq_no		1 byte
##	snd		1 word
##	rcv		1 word
##	hoc		7 bits
##	prox		1 bit
##	hco		7 bits
##	weak		1 bit
##
	set length [llength $pay]

	set res "sniff: "

	if { $length < 12 } {
		# too short
		append res "<[toh $pay]>"
		return $res
	}

	set nid [get_w pay]
	set v [get_b pay]
	set typ [lindex { "NULL" "PONG" "PONA" "MAST"
			  "REPO" "REPA" "FRWD" "FRWA"
			  "BRST" "LOCA" "RPC " "RPCA" } $v]
	if { $typ == "" } {
		set typ [format "T=%02x" $v]
	}

	set seq [get_b pay]
	set snd [get_w pay]
	set rcv [get_w pay]
	set v [get_b pay]
	set hoc [expr { $v & 0x7f }]
	if [expr { $v & 0x80 }] {
		set pro "P"
	} else {
		set pro "-"
	}
	set v [get_b pay]
	set hco [expr { $v & 0x7f }]
	if [expr { $v & 0x80 }] {
		set wea "W"
	} else {
		set wea "-"
	}

	set tai [lrange $pay end-1 end]
	set pay [lrange $pay 0 end-2]

	set lqi [get_b tai]
	set rss [get_b tai]

	append res $typ
	append res " SEQ=[format %03d $seq]"
	append res " SND=[format %05d $snd]"
	append res " RCV=[format %05d $rcv]"
	append res " HOC=[format %03d $hoc]"
	append res " HCO=[format %03d $hco]"
	append res " $pro$wea"
	append res " RSS=[format %03d $rss]"
	append res " LQI=[format %03d $lqi]"
	append res " <[toh $pay]>"
	return $res
}

proc logvalue { r p tp } {

	upvar $r res
	upvar $p pay

	set vt [lindex $tp 0]
	set fm [lindex $tp 1]

	set v [get_$vt pay]

	append res " [format %$fm $v]"
}

###############################################################################
# END OF __plug__ NAMESPACE ###################################################
###############################################################################

}

if [info exists PM(DPF)] {

###############################################################################
# PITER #######################################################################
###############################################################################

proc plug_init { ags } {

	::__plug__::iinit $ags 0
}

proc plug_close { } {

	::__plug__::iclose 0
}

proc plug_inppp_b { in } {

	upvar $in inp

	set inp [string trim $inp]

	::__plug__::iinput $inp 0

	return 0
}

proc plug_outpp_b { in } {

	upvar $in inp

	set bts ""

	# this is a bunch of pairs of hex digits, turn them into proper numbers
	foreach v $inp {
		lappend bts [expr { "0x$v" }]
	}

	::__plug__::ioutput $bts 0

	return 0
}

proc node_write { line t } {

	pt_outln $line
}

proc term_write { line t } {

	pt_tout $line
}

namespace export plug_*

} else {

###############################################################################
# UDAEMON #####################################################################
###############################################################################

proc vplug_init { nn hn tp t } {

	variable __ps

	if { $tp != "a321p" } {
		# this only works for Pegs
		return 0
	}

	::__plug__::iinit "" $t
	# receiver state
	set __ps($t,S) 0

	return 1
}

proc vplug_close { t } {

	::__plug__::iclose $t
}

proc vplug_receive { bytes t hex } {

	variable __ps

	set chunk ""

	while { $bytes != "" } {
		set c [string index $bytes 0]
		set bytes [string range $bytes 1 end]
		scan $c %c v
		lappend chunk $v
	}

	while 1 {

		set bl [llength $chunk]
		if { $bl == 0 } {
			return ""
		}

		switch $__ps($t,S) {

		0 {
			# waiting for STX
			for { set i 0 } { $i < $bl } { incr i } {
				set c [lindex $chunk $i]
				if { $c == 2 } {
					# STX
					set __ps($t,S) 1
					set __ps($t,E) 0
					set __ps($t,B) ""
					set __ps($t,L) 0
					# initialize the parity byte
					set __ps($t,P) 5
					break
				}
			}
			if { $i == $bl } {
				return ""
			}
			incr i
			set chunk [lrange $chunk $i end]
		}

		1 {
			# parse for escapes
			for { set i 0 } { $i < $bl } { incr i } {
				set c [lindex $chunk $i]
				if !$__ps($t,E) {
					if { $c == 3 } {
						# that's it
						set __ps($t,S) 0
						set chunk [lrange $chunk \
							[expr { $i + 1 }] end]
						if $__ps($t,P) {
							term_write \
							    "Parity error!" $t
							break
						}
						# remove the parity byte and
						# process the message
						::__plug__::ioutput [lrange \
						    $__ps($t,B) 0 end-1] $t
						break
					}
					if { $c == 2 } {
						# reset
						set __ps($t,S) 0
						set chunk [lrange $chunk $i end]
						break
					}
					if { $c == 16 } {
						# escape
						set __ps($t,E) 1
						continue
					}
				} else {
					set __ps($t,E) 0
				}
				lappend __ps($t,B) $c
				set __ps($t,P) \
					[expr { ($__ps($t,P) + $c) & 0xFF }]

				incr __ps($t,L)
			}
			if $__ps($t,S) {
				set chunk ""
			}
		}

		}
	}
}

proc vplug_input { in t } {

	upvar $in inp

	set inp [string trim $inp]

	::__plug__::iinput $inp $t

	return 1
}

proc node_write { m t } {

	set r [list 0x02]
	set h [expr { (-(0x02 + 0x03)) & 0xFF } ]

	foreach v $m {

		if { $v == 0x02 || $v == 0x03 || $v == 0x10 } {
			lappend r 0x10
		}

		lappend r $v

		set h [expr { ($h - $v) & 0xFF } ]
	}

	if { $h == 0x02 || $h == 0x03 || $h == 0x10 } {
		lappend r 0x10
	}

	lappend r $h
	lappend r 0x03

	set out ""

	foreach v $r {
		append out [binary format c $v]
	}

	uart_send $out $t
}

proc term_write { line t } {

	term_output "$line\n" $t
}

namespace export vplug_*

###############################################################################
# END PITER OR UDAEMON ########################################################
###############################################################################
} 
