Main:
T1:
    lui x4, 0x10000
    addi x4, x4, 0x03c
    lw x5, 0(x4)
    addi x5, x5, 1
    sw x5, 0(x4)
T2:    
    lui x4, 0x10100
    addi x4, x4, 0x03c
    lw x5, 0(x4)
    addi x5, x5, 2
    sw x5, 0(x4)
T3:    
    lui x4, 0x11100
    addi x4, x4, 0x03c
    lw x5, 0(x4)
    addi x5, x5, 3
    sw x5, 0(x4)
T4:    
    lui x4, 0x11200
    addi x4, x4, 0x03c
    lw x5, 0(x4)
    addi x5, x5, 4
    sw x5, 0(x4)
T5:   
    lui x4, 0x10100
    addi x4, x4, 0x000
    add  x6, x0, x0
    addi x7, x7, 20
    loop:
        lw x5, 0(x4)
        addi x4, x4, 4
        addi x6, x6, 1
        blt x6, x7, loop
exit:
    nop
    
T6:
    lui x4, 0x00100
    addi x4, x4, 0x0fc
    lw x5, 0(x4)
T7:
    lui x4, 0x01100
    addi x4, x4, 0x0fc
    lw x5, 0(x4)
    addi x10, x10, 100
    sw x10, 0(x4)
    add x10, x0, x4
T8:
    lui x4, 0x11100
    addi x4, x4, 0x0fc
    lw x5, 0(x4)        
T9:
    lui x4, 0x11200
    addi x4, x4, 0x0fc
    lw x5, 0(x4)
T10:
    addi x10, x10, -8
    addi x11, x0, 50
    sw x11, 0(x10)
T11:
    lui x4, 0x17200
    addi x4, x4, 0x03c
    addi x10, x0, 20
    sw x10, 0(x4)
    lui x4, 0x17e00
    addi x4, x4, 0x02c
    lw x10, 0(x4)
    