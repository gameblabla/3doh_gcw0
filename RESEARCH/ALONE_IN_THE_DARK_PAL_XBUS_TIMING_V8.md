# Alone in the Dark PAL XBUS timing research v8

This pass keeps `fixmode = 0` and does not enable Opera/libretro's per-title `FIX_BIT_TIMING_6` path.

## Problem reproduced

The PAL run previously passed the fixed DSPP N-window read at `0x034025e0`, then later aborted around frame 1598 with an ARM prefetch fault into unmapped/garbage code. Enabling Opera's Timing Hack 6 avoided that abort, but that is a title compatibility switch rather than a hardware model.

## Hardware model added

This build models a generic CLIO/XBUS DMA timing side effect:

- CLIO DMA enable bit `0x00100000` is treated as a DMA-to/from-XBUS activity pulse.
- Each pulse opens a short observation window and accumulates a small timer-pressure value.
- CLIO timer register `0x120` writes are adjusted only while this XBUS DMA pressure is active, using the same kind of timer-coupling behavior Opera exposes internally.
- The QUARZ scheduler reacts to this active XBUS DMA pressure globally. It is not keyed to a disc ID or title name.

The goal is to replace the empirical Alone-specific Timing Hack 6 with a hardware-event-driven timing model.

## Validation

Alone in the Dark (Europe) was run in PAL mode from the uploaded CUE/BIN image:

- v7 baseline: false DSPP N-window strict fault fixed, but later ARM prefetch fault around frame 1598.
- v8: PAL headless completed 4500 frames with `fault=0`.
- v8: wasm smoke completed 120 PAL frames with `fault=0`, `320x288`, and the DSPP N-window read recorded.

This is still research-grade. It removes the observed early PAL abort without enabling the per-title hack, but it is not yet a proof that every post-logo interactive path is correct on all titles.
