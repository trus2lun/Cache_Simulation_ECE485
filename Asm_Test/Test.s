.data
#Array element
a0: .word 10 
a1: .word 3
a2: .word 5
a3: .word 1

.text
main:
    li x8, 4
    addi x8, x8, -1
    add x9, x0, x0                
Outer_Loop:
    la x5, a0
    add x10, x0, x0
    add x11, x8, x0
    sub x11, x11, x9    
    Inner_Loop:
        lw x6, 0(x5)
        lw x7, 1(x5)
        blt x6, x7, bypass
        sw x7, 0(x5)
        sw x6, 1(x5)
        bypass:                    
        addi x5, x5, 1
        addi x10, x10, 1
        blt x10, x11, Inner_Loop        
    addi x9, x9, 1
    blt x9, x8, Outer_Loop
exit:
    nop