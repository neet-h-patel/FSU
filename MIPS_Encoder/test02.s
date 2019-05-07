   .text
   addu $s0,$zero,$zero
   addu $s1,$zero,$zero
   addiu $v0,$zero,5
   syscall
   sw $v0,n($gp)

L1:
   lw $s2,n($gp)
   slt $t0,$s1,$s2
   beq $t0,$zero,L2
   addiu $v0,$zero,5
   syscall
   addu $s0,$s0,$v0
   addiu $s1,$s1,1
   j L1

L2:
   addu $a0,$s0,$zero
   addiu $v0,$zero,1
   syscall
   addiu $v0,$zero,10
   syscall

L3:
   div $t0,$s0
   mfhi $t0
   mult $t0,$t0
   mfhi $t0
   mflo $t1
   beq $t0,$t1,L3
   syscall

L4:
   addu $t0,$t0,$t0
   beq $t0,$zero,L4
   addu $t0,$t0,$t0
   addu $t0,$t0,$t0
   addu $t0,$t0,$t0
   bne $t0,$zero,L4
   j L1

   .data
n: .word 0

