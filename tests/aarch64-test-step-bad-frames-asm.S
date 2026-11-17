/* Frames for aarch64-test-step-bad-frames.c.

   Each leaf takes x0 = control block: it stores 1 to [x0] once it
   spins, and spins until [x0 + 4] is non-zero.  */

    .text

/* ---- A: leaf without CFI, called by a GCC-style frame (frame record at
   sp+384, CFA sp+480), called by a frame whose stack slots hold x1 (a
   stale x29) and a return address.  */

    .p2align 2
    .globl spin_without_cfi
    .type spin_without_cfi, %function
spin_without_cfi:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    mov w1, #1
    stlr w1, [x0]
    add x2, x0, #4
1:
    ldar w1, [x2]
    cbz w1, 1b
    ldp x29, x30, [sp], #16
    ret
    .size spin_without_cfi, .-spin_without_cfi
    .globl spin_without_cfi_end
spin_without_cfi_end:

    .p2align 2
    .globl gcc_style_frame
    .type gcc_style_frame, %function
gcc_style_frame:
    .cfi_startproc
    sub sp, sp, #480
    .cfi_def_cfa_offset 480
    stp x29, x30, [sp, #384]
    .cfi_offset 29, -96
    .cfi_offset 30, -88
    add x29, sp, #384
    bl spin_without_cfi
    .globl gcc_style_frame_return
gcc_style_frame_return:
    ldp x29, x30, [sp, #384]
    add sp, sp, #480
    .cfi_restore 29
    .cfi_restore 30
    .cfi_def_cfa_offset 0
    ret
    .cfi_endproc
    .size gcc_style_frame, .-gcc_style_frame

    .p2align 2
    .globl poisoned_frame
    .type poisoned_frame, %function
poisoned_frame:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    mov x29, sp
    .cfi_def_cfa w29, 16
    sub sp, sp, #1024
    adr x2, poisoned_frame_return
    mov x3, sp
    add x4, sp, #1024
2:
    stp x1, x2, [x3], #16
    cmp x3, x4
    b.lo 2b
    bl gcc_style_frame
    .globl poisoned_frame_return
poisoned_frame_return:
    mov sp, x29
    ldp x29, x30, [sp], #16
    .cfi_def_cfa sp, 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size poisoned_frame, .-poisoned_frame

/* ---- B: CFI says x29 was saved in a slot that holds x1 (a stale x29).  */

    .p2align 2
    .globl stale_fp_slot
    .type stale_fp_slot, %function
stale_fp_slot:
    .cfi_startproc
    sub sp, sp, #48
    .cfi_def_cfa_offset 48
    stp x29, x30, [sp, #32]
    add x29, sp, #32
    stp x1, x30, [sp]
    .cfi_offset 29, -48
    .cfi_offset 30, -40
    mov w1, #1
    stlr w1, [x0]
    add x2, x0, #4
4:
    ldar w1, [x2]
    cbz w1, 4b
    ldp x29, x30, [sp, #32]
    add sp, sp, #48
    .cfi_restore 29
    .cfi_restore 30
    .cfi_def_cfa_offset 0
    ret
    .cfi_endproc
    .size stale_fp_slot, .-stale_fp_slot
    .globl stale_fp_slot_end
stale_fp_slot_end:

    .p2align 2
    .globl fp_based_frame
    .type fp_based_frame, %function
fp_based_frame:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    mov x29, sp
    .cfi_def_cfa w29, 16
    bl stale_fp_slot
    .globl fp_based_frame_return
fp_based_frame_return:
    ldp x29, x30, [sp], #16
    .cfi_def_cfa sp, 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size fp_based_frame, .-fp_based_frame

/* ---- C: well-formed frames.  */

    .p2align 2
    .globl spin_with_cfi
    .type spin_with_cfi, %function
spin_with_cfi:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    mov x29, sp
    .cfi_def_cfa w29, 16
    mov w1, #1
    stlr w1, [x0]
    add x2, x0, #4
5:
    ldar w1, [x2]
    cbz w1, 5b
    ldp x29, x30, [sp], #16
    .cfi_def_cfa sp, 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size spin_with_cfi, .-spin_with_cfi
    .globl spin_with_cfi_end
spin_with_cfi_end:

    .p2align 2
    .globl sane_frame
    .type sane_frame, %function
sane_frame:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    mov x29, sp
    .cfi_def_cfa w29, 16
    bl spin_with_cfi
    .globl sane_frame_return
sane_frame_return:
    ldp x29, x30, [sp], #16
    .cfi_def_cfa sp, 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size sane_frame, .-sane_frame

/* ---- D: x30 is saved at CFA-8 (valid), x29 at an unmapped address.  */

    .p2align 2
    .globl bad_x29_loc_frame
    .type bad_x29_loc_frame, %function
bad_x29_loc_frame:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    mov x29, sp
    .cfi_def_cfa w29, 16
    bl bad_x29_loc_leaf
    .globl bad_x29_loc_frame_return
bad_x29_loc_frame_return:
    ldp x29, x30, [sp], #16
    .cfi_def_cfa sp, 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size bad_x29_loc_frame, .-bad_x29_loc_frame

    .p2align 2
    .globl bad_x29_loc_leaf
    .type bad_x29_loc_leaf, %function
bad_x29_loc_leaf:
    .cfi_startproc
    stp x29, x30, [sp, #-16]!
    .cfi_def_cfa_offset 16
    .cfi_offset 30, -8
    // DW_CFA_expression x29 = DW_OP_const8u 0xffffff80a2419420
    .cfi_escape 0x10, 0x1d, 0x09, 0x0e, 0x20, 0x94, 0x41, 0xa2, 0x80, 0xff, 0xff, 0xff
    mov w1, #1
    stlr w1, [x0]
    add x2, x0, #4
8:
    ldar w1, [x2]
    cbz w1, 8b
    ldp x29, x30, [sp], #16
    .cfi_def_cfa_offset 0
    .cfi_restore 29
    .cfi_restore 30
    ret
    .cfi_endproc
    .size bad_x29_loc_leaf, .-bad_x29_loc_leaf
    .globl bad_x29_loc_leaf_end
bad_x29_loc_leaf_end:

    .section .note.GNU-stack,"",%progbits
