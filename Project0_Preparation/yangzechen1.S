.global main
main:

// TODO: [p0-task1] Add your code here!
    li  t0, 0                 # t0 is used for saving the total sum
    li  t1, 1                 # t1 is used as the iterator
    li  t2, 100               # t2 is the upper boundary of the loop
Loop:
    add     t0, t0, t1
    addi    t1, t1, 1
    ble     t1, t2, Loop      # branch to Loop if t1 <= 100

end:
	j end
