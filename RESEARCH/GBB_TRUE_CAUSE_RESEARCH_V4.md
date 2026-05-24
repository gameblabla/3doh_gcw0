# Gameblabla hardware-crash research, pass 4

This pass compared 3DOh/FreeDO behavior against MAME's current 3DO/CLIO/DSPP model and added targeted diagnostics for the suspected areas.

## MAME comparison points

MAME's 3DO driver maps RAM as 2 MB main DRAM plus a separate VRAM window at `0x00200000..0x003fffff`. It also documents that the real VRAM size should be 1 MB, though MAME currently maps more because BIOS boot still depends on that emulation shortcut.

MAME's CLIO/DSPP model also shows a few useful behavioral clues:

- DSPP interrupt output is routed to CLIO first-priority DSPP INT FIQ.
- CLIO pending IRQ registers are set/cleared explicitly; FIQ line state is recomputed from pending and mask registers.
- DSPP runs only when the GWILLING control bit is set.
- DSPP FIFO underflow returns the previous current sample and raises under/over state rather than returning generic zero.
- The MAME source explicitly marks DSPP mapping/restart behavior as incomplete and notes that DSPP should restart on counter reloads.

## Changes made in this pass

- CLIO DSPP control write at `0x17fc` now treats bit 0 as the actual GW/start bit instead of treating any nonzero write as start.
- Added CLIO DSPP control/reset diagnostics:
  - DSPP control write count
  - last control value
  - non-GW control write count
  - reset write count
  - last reset value
- Added ARM high-memory access counters for the `0x00200000..0x002fffff` region. This is not treated as a fault, because this region is VRAM-accessible in known 3DO memory maps and MAME also maps it.
- EI FIFO underflow now returns the last value supplied by that FIFO channel instead of always returning zero, matching MAME's previous-current underflow behavior more closely.
- Added the new diagnostics to the headless runner and wasm `threedohDiagnostics()` output.

## Gameblabla results

The Gameblabla minigame 2/3/4 scripts still do not reach the real-hardware crash in this emulator. The new data rules out several simpler theories:

- `0x17fc` DSPP control writes are normal on these paths: two writes, last value `0x00000001`, no non-GW writes.
- The high-memory accesses are real and heavy, but they occur early in the normal launcher and line up with the VRAM window, not a simple invalid main-RAM extension.
- FIFO starvation remains the strongest reproduced symptom. Fourth minigame path still reaches persistent EI FIFO starvation on channels 0, 1, and 2 by frame 5000.

## Current working hypothesis

The remaining mismatch is probably not a one-bit CLIO register bug and not a direct root `LaunchMe` boot problem. The next likely area is DSPP timing/restart semantics: FreeDO/3DOh currently runs a full DSP loop at video-frame cadence and resets DSP PC at each loop invocation, while MAME's source comments indicate DSPP should restart on counter reloads. That is exactly the layer that would decide whether audio-folio/soundspooler starvation becomes a fatal hardware-visible condition or stays survivable in emulators.
