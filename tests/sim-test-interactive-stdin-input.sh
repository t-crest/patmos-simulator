#!/usr/bin/expect
# This script should be run by Expect and not a shell.
# See: https://linux.die.net/man/1/expect
# It executes pasim, giving it the input specified through stdin 
# using an interactive session (not piped, which would not be interactive.)
# 
# Takes 3 arguments:
# 1. Path to pasim executable.
# 2. Path to binary program that pasim is to run.
# 3. Path to a file containing the input to pass to pasim's stdin using an interactive session.
#
# Notes: 
#
# pasim has 2 seconds to run the program to completion, otherwise this script will 
# terminate it.
#
# The output of this script is the stdout of pasim.
#
# The exit code of this script is always 0, regardless of the exit code of pasim.
#
# Example call of this script:
#
# $ expect <script name> -- pasim program.bin input_file

set pasim [lindex $argv 0];
set program [lindex $argv 1];
set input [lindex $argv 2];
# 2 second timeout when this script ends.
set timeout 2

# Spawn program
spawn -noecho $pasim -V --maxc 40000 $program

# Send it some input
send [exec cat $input]

# Send program output to stdout
expect full_buffer