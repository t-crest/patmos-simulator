#
# Expected Result: r1 = 7
#

                .word   28;
				pand    p1 = p0, !p0
        (p1)    addi    r1  = r0 , 5    ||        (p0)    addi    r1  = r0 , 7;
                halt;
		nop;
		nop;
		nop;
