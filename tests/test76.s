#
# Tests non-delayed return ignores following instructions
#

                .word	100;
                addi    r1 = r0, 0;
                brcfnd	x;
                nop;
                nop;
                nop;
                halt;
                nop;
                nop;
                nop;
                .word	24;
x:              addi	r1 = r1, 1337;
                retnd;
				addi	r1 = r1, 1;
				addi	r1 = r1, 2;
				addi	r1 = r1, 3;

