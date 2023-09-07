#
# Tests stack store bundled with call (both enabled)
#

                .word   60;
				addi	r1 = r0, 123;
				addi	r2 = r0, 456;
				addi	r3 = r0, 0;	
				addi	r4 = r0, stack_top;	
				mts		ss = r4;
				mts		st = r4;
				sres	1
				callnd call_tar || sws [0] = r2;
				addi	r1 = r0, 0; # If executed, clear r1
				halt;
                nop;
                nop;
                nop;
				
				.word 20
call_tar:
				lws r3 = [0];
				halt;   	      	
				nop;  
				nop;
				nop;
stack_top:
				.word 0;
				.word 0;
				.word 0;
				.word 0;
				.word 0;

