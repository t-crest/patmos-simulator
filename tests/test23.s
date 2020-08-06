#
# Checks the UART status flag for whether it is ready to 
# receive data. If so, the program outputs '!' to the UART 
# (which should be seen in stdout.)
# Expected Result: '!' is output, and r2 = 0x21
#

                .word   56;
                add     r1  = r0, 0xF0080000;
                addi    r2  = r0, 1;
                lwl     r3  = [r1 + 0];
                nop;
                andi    r3  = r3, 1;
		cmpneq  p1 = r3, r2;
          (!p1) addi    r2 = r0, 0x21;
          (!p1) swl     [r1 + 1] = r2;
                halt;
                nop;
                nop;
                nop;
