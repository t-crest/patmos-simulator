#
# Tests non-delayed return ignores following non-instruction bytes
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
                .word	2147483647;
                .word	2147483647;
                .word	2147483647;

