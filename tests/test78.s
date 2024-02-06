#
# Tests bundled instructions can write to the same register if they are not both enabled.
#

                .word	100;
                pand	p1 = p0, p0;
                pand	p2 = p0, !p0;
                (p1)	addi   r1 = r0, 16 || 	(p2) subi   r1 = r0, 5;
                (p2)	addi   r2 = r1, 16 || 	(p1) subi   r2 = r1, 5;
                halt;
                nop;
                nop;
                nop;

