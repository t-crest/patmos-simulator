#
# Tests cannot have enabled branch and call in bundle
#

                .word   40;
				add    r1 = r0, r0;
                (p0) br  brto1 || (p0) call  brto2	
                nop;
                nop;
                halt;
                nop;
                nop;
                nop;
brto1:
brto2: