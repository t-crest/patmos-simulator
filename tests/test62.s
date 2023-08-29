#
# Tests a call and branch in bundle with only one enabled
#

                .word   160;
				add    r1 = r0, 111;
				add    r2 = r0, r0;
				add    r3 = r0, r0;
				add    r4 = r0, r0;
				add    r5 = r0, r0;
				add    r6 = r0, r0;
				add    r7 = r0, r0;
				add    r8 = r0, r0;
				pand   p5 = p0, p0;
                (p5) br  brto1 || (!p0) call  brto2		
				addi	r2 = r0, 222;	
				addi	r3 = r0, 333;
				addi	r1 = r0, 0;	# shouldn't be executed
                halt;
                nop;
                nop;
                nop;

brto1:			addi	r4 = r0, 444;
                (!p5) br  brto1 || (p0) call  brto3	
				addi	r5 = r0, 555;	
				addi	r6 = r0, 666;
				addi	r7 = r0, 777;
                halt;
                nop;
                nop;
                nop;

brto2:			addi	r1 = r0, 0;	# shouldn't be executed
                halt;
                nop;
                nop;
                nop;

brto3:			addi	r8 = r0, 888;
                halt;
                nop;
                nop;
                nop;
