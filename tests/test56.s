#
# Tests the use of loads in the second issue slot where they are both enabled
#

                .word   60;
				addi    r1 = r0, DUMMY1;
				addi    r2 = r0, DUMMY2;
				pand	p1 = p0, p0
				pand	p2 = p0, p0
                (p1)lwm  r3 = [r1] || (p2) lwm  r4 = [r2]				
                halt;
                nop;
                nop;
                nop;

DUMMY1:			.word 123;
DUMMY2:			.word 456;
