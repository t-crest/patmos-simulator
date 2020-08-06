#!/usr/bin/expect
set pasim [lindex $argv 0];
set program [lindex $argv 1];
set input [lindex $argv 2];
# 3 second timeout when this script ends.
set timeout 3 

# Spawn program
spawn -noecho $pasim -V $program

# Send it some input
send [exec cat $input]

# Send program output to stdout
expect full_buffer