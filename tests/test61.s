#
# Tests branches in both issue slots with only one enabled
#

                .word   40;
				add    r1 = r0, r0;
                (p0) br  brto1 || (p0) br  brto2	
                nop;
                nop;
                halt;
                nop;
                nop;
                nop;
brto1:
brto2: