#
# Tests the use of stores in the second issue slot where only one is enabled
#

                .word   60;
				addi    r1 = r0, DUMMY1;
				addi    r2 = r0, DUMMY2;
				addi    r3 = r0, 12;
				pand	p1 = p0, p0
                (p1) swm  [r1] = r3 || (p1) swm  [r2] = r4				
                halt;
                nop;
                nop;
                nop;

DUMMY1:			.word 0;
DUMMY2:			.word 0;
