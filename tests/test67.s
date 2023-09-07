#
# Tests main-memory store bundled with call (both enabled)
#

                .word   60;
				addi	r1 = r0, 123;
				addi	r2 = r0, dummy;
				addi	r3 = r0, 456;
				addi	r4 = r0, 0;		
				addi	r5 = r0, 0;	
				callnd call_tar || swm [r2] = r3;
				addi	r5 = r0, 789;
				halt;
                nop;
                nop;
                nop;
				
				.word 20
call_tar:
				lwm r4 = [r2];
				retnd;   	      	
				nop;  
				nop;
				nop;
dummy:		.word 0;

