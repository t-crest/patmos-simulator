#
# Tests the use of stores in the second issue slot where only one is enabled
#

                .word   84;
				addi    r1 = r0, DUMMY1;
				addi    r2 = r0, DUMMY2;
				addi    r3 = r0, 12;
				addi    r4 = r0, 34;
				addi    r5 = r0, 0;
				addi    r6 = r0, 0;
				pand	p2 = p0, p0
				pand	p3 = p0, !p0
                (p2)swm  [r1] = r3 || (p3) swm  [r2] = r4	
                (p3)swm  [r1] = r3 || (p2) swm  [r2] = r4	
				lwm  r5 = [r1]
				lwm  r6 = [r2]				
                halt;
                nop;
                nop;
                nop;

DUMMY1:			.word 0;
DUMMY2:			.word 0;
