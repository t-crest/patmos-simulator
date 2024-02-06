#
# Tests bundled instructions can write to the same register if they are not both enabled.
# This test ensures that if the predicates where both enabled originally, and then one disabled
# right before the bundle, it will work correctly.
#
# I.e., this is a test that the pipelining is correctly accounted for in the check for the predicates and writes.
#

                .word	100;
                pand	p1 = p0, p0 || pand	p2 = p0, p0;
                pand	p1 = p0, p0 || pand	p2 = p0, p0;
                pand	p1 = p0, p0 || pand	p2 = p0, p0;
                pand	p1 = p0, p0 || pand	p2 = p0, !p0;
                (p1)	addi   r1 = r0, 32 || 	(p2) subi   r1 = r0, 5;
                halt;
                nop;
                nop;
                nop;

