.globl memset

.section .text
memset:
	beqz a2, 2f
	add a2,a2,a0
1:	sb a1,0(a0)
	addi a0,a0,1
	bne a0,a2,1b
2:	ret

.globl memcpy
.section .text
memcpy:
    mv      t0,a0
    add     a2,a2,a1
    j       2f
1:  lb      t2,(a1)
    addi    a1,a1,1
    sb      t2,(t0)
    addi    t0,t0,1
2:  bne     a1,a2,1b
    ret
