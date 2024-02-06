#
# Tests two enabled instructions cannot be bundled if they write to the same register
#

                .word	100;
				addi   r1 = r0, 1 || addi   r1 = r0, 2;
                halt;
                nop;
                nop;
                nop;

