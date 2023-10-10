#
# Tests bundled mutiplies with only one enabled
#

                .word   100;
                addi	r1 = r0, 1;
                addl	r2 = r0, 2147483648;
                addi	r3 = r0, 6;
                addi	r4 = r0, 0;
                mts		sl = r0;
                mts		sh = r0;
                (!p0) mulu 	r0, r0 || mulu 	r1, r3;
                nop;
				mfs		r4 = sl;
                mulu 	r2, r3 || (!p0) mulu 	r0, r0;
                nop;
                halt;
                nop;
                nop;
                nop;

