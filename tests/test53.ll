;
; Tests 'callnd' doesn't execute the following instructions before the call.
;
; We test this as an .elf because this error didn't show up in binary mode.
;
; Compile with:
; 
;   patmos-llc test53.ll -filetype=obj -o test53.elf && patmos-ld.lld --nostdlib --static -o test53.elf test53.elf
;       

define i32 @_start() #0 {
entry:
   %0 = tail call i32 asm sideeffect "
			li 				$0 = 123
			pmov 			$$p7 = !$$p0 
			callnd			test_fn
			pmov 			$$p7 = $$p0 
			pmov 			$$p7 = $$p0 
			pmov 			$$p7 = $$p0 
			nop	
			nop	
			nop	
		
			li $$r0 = 24
test_fn:	   
			li (!$$p7) $0 = 0	# Set to 0 only if p7 is false
			li ( $$p7) $0 = 1	# Set to 1 only if p7 is true
			brcf 0 				# halt
			nop	
			nop	
			nop	
    ", "=r"()
  ret i32 %0
}

attributes #0 = { naked noinline }
