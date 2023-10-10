#
# Tests multiply in second issue slot
#

                .word   100;
                addl	r1 = r0, 2147483649;
                addi	r2 = r0, 2;
                addi	r3 = r0, 0;
                mts		sl = r0;
                mts		sh = r0;
                addi	r3 = r0, 123 || mulu 	r1, r2;
                nop;
                halt;
                nop;
                nop;
                nop;

