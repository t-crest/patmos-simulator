#
# Tests branches in both issue slots with only one enabled
#

                .word   128;
				add    r1 = r0, r0;
				add    r2 = r0, r0;
				add    r3 = r0, r0;
				add    r4 = r0, r0;
                (!p0) br  brto1 || (p0) br  brto2		
				addi	r1 = r0, 123;	
				addi	r2 = r0, 456;
                halt;
                nop;
                nop;
                nop;

brto1:			addi	r3 = r0, 789;
                halt;
                nop;
                nop;
                nop;

brto2:			addi	r3 = r0, 159;
				(p0) br  brto3 || (!p0) br  brto1
                nop;
                nop;
                halt;
                nop;
                nop;
                nop;

brto3:			addi	r4 = r0, 753;
                halt;
                nop;
                nop;
                nop;
