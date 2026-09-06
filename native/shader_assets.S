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

.section .rodata.ps5_bsp_flat_shaders,"a",@progbits

.balign 256
.global ps5_bsp_flat_gs_start
ps5_bsp_flat_gs_start:
.incbin "build/shaders/bsp_flat.gs.bin"
.global ps5_bsp_flat_gs_end
ps5_bsp_flat_gs_end:

.balign 256
.global ps5_bsp_flat_ps_start
ps5_bsp_flat_ps_start:
.incbin "build/shaders/bsp_flat.ps.bin"
.global ps5_bsp_flat_ps_end
ps5_bsp_flat_ps_end:

.section .rodata.ps5_bsp_textured_shaders,"a",@progbits

.balign 256
.global ps5_bsp_textured_gs_start
ps5_bsp_textured_gs_start:
.incbin "build/shaders/bsp_textured.gs.bin"
.global ps5_bsp_textured_gs_end
ps5_bsp_textured_gs_end:

.balign 256
.global ps5_bsp_textured_ps_start
ps5_bsp_textured_ps_start:
.incbin "build/shaders/bsp_textured.ps.bin"
.global ps5_bsp_textured_ps_end
ps5_bsp_textured_ps_end:

.section .rodata.ps5_bsp_resource_shaders,"a",@progbits

.balign 256
.global ps5_bsp_resource_gs_start
ps5_bsp_resource_gs_start:
.incbin "build/shaders/bsp_resource.gs.bin"
.global ps5_bsp_resource_gs_end
ps5_bsp_resource_gs_end:

.balign 256
.global ps5_bsp_resource_ps_start
ps5_bsp_resource_ps_start:
.incbin "build/shaders/bsp_resource.ps.bin"
.global ps5_bsp_resource_ps_end
ps5_bsp_resource_ps_end:

.section .rodata.ps5_bsp_overlay_shaders,"a",@progbits

.balign 256
.global ps5_bsp_overlay_gs_start
ps5_bsp_overlay_gs_start:
.incbin "build/shaders/bsp_overlay.gs.bin"
.global ps5_bsp_overlay_gs_end
ps5_bsp_overlay_gs_end:

.balign 256
.global ps5_bsp_overlay_ps_start
ps5_bsp_overlay_ps_start:
.incbin "build/shaders/bsp_overlay.ps.bin"
.global ps5_bsp_overlay_ps_end
ps5_bsp_overlay_ps_end:

.section .note.GNU-stack,"",@progbits
