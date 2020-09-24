#
# Tests whether pasim issues an error if the destination register
# of a load is used in the delay slot
# Expected Result: Error because of the lack of delay slot
#

                .word   40;
                add     r1  = r0, 0xF0080000;
                addi    r2  = r0, 2;
                lwl     r3  = [r1 + 0];
                addi    r8  = r3, 1; # Illegal use of r3
                halt;
                nop;
                nop;
                nop;
