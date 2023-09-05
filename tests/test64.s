#
# Tests stack load/store in second issue slot
#

                .word   100;
                addi	r1 = r0, stack_base;
                addi	r2 = r0, 123;
                addi	r3 = r0, 0;
                addi	r4 = r0, 0;
                addi	r5 = r0, 0;
                mts		s5 = r1;
                mts		s6 = r1;
                sres	2;
                addi	r4 = r0, 456 || sws		[0] = r2;
				addi	r5 = r0, 789 || lws		r3 = [0];
                sfree	2;
                halt;
                nop;
                nop;
                nop;
stack_base:
                .word   0;
                .word   0;
                .word   0;
                .word   0;

