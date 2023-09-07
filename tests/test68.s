#
# Tests load bundled with branch with cache fill  (both enabled)
#

                .word   60;
				addi	r2 = r0, dummy;
				brcf br_tar || lwc r3  = [r2];
                nop;
                nop;
                nop;
				halt;
				
				.word 20
br_tar:
				halt;
                nop;
                nop;
                nop;
dummy:		.word 0;

