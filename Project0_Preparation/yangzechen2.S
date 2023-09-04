.global main
main:

# TODO: [p0-task1] Add your code here!
    li  t0, 1                   # t0 is the iterator of the loop
    li  t1, 100                 # t1 is the upper boundary of the loop
Loop1:
    mv      a0, t0              # a0: val to check
    li      a1, 0               # a1: 1 for prime, 0 for non-prime
    call    prime_checker
    addi    t0, t0, 1
    ble     t0, t1, Loop1

end:
	j end

prime_checker:
    li      t2, 1               # t2: iterator for the factors
    beq     a0, t2, p_c_end     # exit if a0 == 1
    li      t2, 2
Loop2:
    mul     t3, t2, t2
    bgt     t3, a0, p_c_end     # branch if t2*t2 > a0
    rem     t4, a0, t2          # t4 = a0 % t2
    bnez    t4, Loop2           # branch if t4 != 0
    li      a1, 1
p_c_end:
    ret
