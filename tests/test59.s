#
# Tests can have store and load enabled at the same time
#

                .word   60;
				addi    r1 = r0, DUMMY1;
				addi    r2 = r0, DUMMY2;
				addi    r3 = r0, 456;
				pand	p3 = p0, p0
                (p3) swm  [r2] = r3 || (p0) lwm  r4 = [r1]		
				lwm  r5 = [r2]					
                halt;
                nop;
                nop;
                nop;

DUMMY1:			.word 123;
DUMMY2:			.word 0;
