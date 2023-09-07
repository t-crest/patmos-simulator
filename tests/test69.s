#
# Tests branch bundled with call (both enabled)
#

                .word   60;
				pand p1 = p0, p0
				call br_tar || br br_tar;
                nop;
                nop;
                nop;
				halt;
				
				.word 20
br_tar:
				halt;
                nop;
                nop;
                nop;
dummy:		.word 0;

