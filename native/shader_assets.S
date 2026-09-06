/* Build products from shaders/gears_lit.pipe; never checked into source. */
.section .rodata.ps5_gears_shaders,"a",@progbits

.balign 256
.global ps5_gears_gs_start
ps5_gears_gs_start:
.incbin "build/shaders/gears_lit.gs.bin"
.global ps5_gears_gs_end
ps5_gears_gs_end:

.balign 256
.global ps5_gears_ps_start
ps5_gears_ps_start:
.incbin "build/shaders/gears_lit.ps.bin"
.global ps5_gears_ps_end
ps5_gears_ps_end:

.section .note.GNU-stack,"",@progbits
