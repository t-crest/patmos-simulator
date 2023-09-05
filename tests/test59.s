#
# Tests two enabled loads and stores in the same bundle
#

                .word   100;
                addi	r1 = r0, dummy;
				pand 	p7 = p0, p0;
                (p7) 	lwc		r3 = [0] || (p7) swc		[0] = r2;
                halt;
                nop;
                nop;
                nop;
dummy:          .word   0;

