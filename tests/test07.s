#
# Expected Result: sl = 35 & sh = 0
#

                .word   32;
                addi    r1  = r0 , 5    ||                addi    r2  = r0 , 7;
                mul           r1, r2;
                halt;
		nop;
		nop;
		nop;
