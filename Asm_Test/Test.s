.data
#Array element 
a1: .word 3
a2: .word 5
a3: .word 1
a4: .word 2
a5: .word 4
a6: .word 9
a7: .word 20
a8: .word 19
a9: .word 14
a10: .word 45
a11: .word 30
a12: .word 25
a13: .word 16
a14: .word 39
a15: .word 42
a16: .word 50
a17: .word 15
a18: .word 28
a19: .word 33
a20: .word 10

.text
main:
    li x8, 20
    add x9, x0, x0
    add x10, x0, x0
    la x5, a1
                
Outer_Loop:    
    Inner_Loop:
        lw x6, 0(x5)
        lw x7, 4(x5)
        blt x6, x7, bypass
        swap:
            sw x7, 0(x5)
            sw x6, 4(x5)
        bypass:            
        addi x5, x5, 4
        addi x10, x10, 1
        blt x10, x8, Inner_Loop        
    addi x9, x9, 1
    blt x9, x8, Outer_Loop
exit:
    nop