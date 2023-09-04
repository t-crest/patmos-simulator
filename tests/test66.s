#
# Tests branches in second issue slot
#

                .word   100;
				addi	r2 = r0, 0;
				addi	r3 = r0, 123;
				addi	r4 = r0, 0;
				addi	r5 = r0, 0;
				addi	r6 = r0, 0;
				addi	r1 = r0, 456 || br branch_tar;
				addi	r4 = r0, 789;
				addi	r5 = r0, 159;
				addi	r6 = r0, 753;
halt_label:   	halt;
                nop;
                nop;
                nop;
				
				addi	r3 = r0, 0;		# Ensure if 'branch_tar' isn't hit, r3 is cleared	
branch_tar:
				addi	r2 = r0, 179;
                halt;
                nop;
                nop;
                nop;

