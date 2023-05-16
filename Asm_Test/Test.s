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
    
    jal swap
swap:
    slli x6, x11, 2
    add x6, x10, x6
    lw x5, 0(x6)
    lw x7, 4(x6)
    sw x7, 0(x6)
    sw x5, 4(x6)
    jalr x0, x1, 0
