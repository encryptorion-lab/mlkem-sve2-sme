// Isolated SME helpers for timer calibration. The C caller never enters
// streaming mode: smstart zeroes Z/P and would corrupt the compiler frame.

    .text
    .align 2

    .global _calib_mode_switch
    .global calib_mode_switch
_calib_mode_switch:
calib_mode_switch:
    stp  d8,  d9, [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
    smstart
    smstop
    ldp d14, d15, [sp], #16
    ldp d12, d13, [sp], #16
    ldp d10, d11, [sp], #16
    ldp  d8,  d9, [sp], #16
    ret

    .global _calib_ssve_muls
    .global calib_ssve_muls
_calib_ssve_muls:
calib_ssve_muls:
    stp  d8,  d9, [sp, #-16]!
    stp d10, d11, [sp, #-16]!
    stp d12, d13, [sp, #-16]!
    stp d14, d15, [sp, #-16]!
    smstart
    ptrue p0.h
    mov z0.h, #1
    mov z1.h, #3
    mov x1, #256
1:
    mul z0.h, p0/m, z0.h, z1.h
    subs x1, x1, #1
    b.ne 1b
    smstop
    ldp d14, d15, [sp], #16
    ldp d12, d13, [sp], #16
    ldp d10, d11, [sp], #16
    ldp  d8,  d9, [sp], #16
    ret
