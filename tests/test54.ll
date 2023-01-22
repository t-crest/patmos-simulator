;
; Tests 'callnd' and 'retnd' work correctly together (returns to the right address).
;
; We test this as an .elf because this error didn't show up in binary mode.
;
; Compile with:
; 
;   patmos-llc test54.ll -filetype=obj -o test54.elf && patmos-ld.lld --nostdlib --static -o test54.elf test54.elf
;       

define i32 @_start() #0 {
entry:
   %0 = tail call i32 asm sideeffect "
			li 				$$r1 = 0
			li 				$$r2 = 0
			callnd			test_fn
			li 				$$r2 = 456  # Checkpoint
			brcf 0 				# halt
			nop	
			nop	
			nop	
		
			li $$r0 = 24
test_fn:	   
			li 				$$r1 = 123 # Checkpoint
			retnd
			nop	
			nop	
			nop	
    ", "=r"()
  ret i32 %0
}

attributes #0 = { naked noinline }
