# Strict hardware-fault notes for Gameblabla compilation

SDL3 builds deliberately do not define `HLE_SWI` by default. The hosted SDL3 path now uses the real SWI vector path unless `USE_HLE_SWI=1` is explicitly supplied to `make -f Makefile.sdl3`.

The input rig added in v14 remains available and the included scripts cover the Gameblabla compilation paths that are reported to crash on real hardware:

```sh
./3doh-sdl3 gameblablacompil_3do.iso bios.bin \
  --input-script scripts/gameblabla_second_game.input \
  --stop-after-frames 3000

./3doh-sdl3 gameblablacompil_3do.iso bios.bin \
  --input-script scripts/gameblabla_third_game.input \
  --stop-after-frames 3000

./3doh-sdl3 gameblablacompil_3do.iso bios.bin \
  --input-script scripts/gameblabla_fourth_game.input \
  --stop-after-frames 3000
```

Strict mode now also validates MADAM CEL source spans, CCB reads/writes, CLIO XBUS DMA spans, FIFO spans, and DSP program/data memory accesses. Bad CEL/DMA state is converted into a strict MADAM/DSP/CLIO fault instead of allowing the host process to loop forever or read arbitrary memory.

This pass is intentionally conservative about the ARM60 cache. The emulator still does not contain a complete ARM60 cache/MMU model. If one of the Gameblabla failures is ultimately caused by stale instruction/data cache behaviour rather than bad DMA/CEL/DSP memory state, the next step is a dedicated ARM60 cache model rather than another memory-bound check.

## v16 ARM60 CP15/cache/MMU/write-buffer check

Added CP15/cache/MMU/write-buffer instrumentation and lightweight behavior. The Gameblabla second/third/fourth scripts were rerun with CP15 counters exported through WASM. The observed CP15 operation count stayed zero during the reproduced launch paths, so the current mismatch is probably not directly caused by cache-control/MMU/write-buffer code in those paths. HLE_SWI remains disabled by default for SDL3.
