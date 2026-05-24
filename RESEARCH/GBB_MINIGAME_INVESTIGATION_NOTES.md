# Gameblabla minigame 2/3/4 investigation pass

This pass does not force a Gameblabla crash.  It adds instrumentation needed to compare the menu-launch path against a direct `LaunchMe`/entry-file path and removes one emulator-only shortcut that can hide real Portfolio behavior.

## What was found from the previous DSPP pass

The earlier strict DSPP resource/window pass tested the supplied `scripts/gameblabla_second_game.input`, `scripts/gameblabla_third_game.input`, and `scripts/gameblabla_fourth_game.input` against the Gameblabla compilation image.  Those scripted menu paths produced no strict DSPP resource/window faults.  That rules out the simplest explanation: raw CLIO DSPP EI/EO/NMem mirror-window misuse is not what lets the minigames run under this emulator while failing on hardware.

The next likely area is not raw DSPP address decoding.  It is runtime state inherited from the launcher: DSP run/stop/reset sequencing, ARM<->DSP semaphore traffic, DSP-generated AudioFIQ state, and FIFO/DMA underflow/overflow behavior.

## Current uploaded `gbb.zip`

The uploaded `gbb.zip` in this turn is a static `gameblabla.github.io` website archive, not the Gameblabla compilation disc image or a source tree containing `LaunchMe` files.  It includes pages such as `hb/helicopters/index.html`, but no ISO/CUE/BIN and no `LaunchMe`/`mainLaunchMe` payload to patch or directly compare.  Because of that, this pass cannot reproduce the second/third/fourth minigame path in this container.

## Changes made

- Defaulted wasm-clang and SDL1.2 builds to `USE_HLE_SWI=0`.
  - SDL3 was already defaulting to `USE_HLE_SWI=0`.
  - `USE_HLE_SWI=1` is still available as an explicit build option.
- Added a no-SDL deterministic runner:
  - `Makefile.headless`
  - `src/tools/headless_runner.c`
- Added CLIO/DSP diagnostic counters without adding them to save-state payloads:
  - CLIO FIQ generation count, observed pending-FIQ count, last reason0/reason1, IRQ pending/mask banks.
  - EI FIFO read/empty/reload counts.
  - EO FIFO write/disabled/full counts.
  - DSP run-start/run-stop/reset counts.
  - DSP INT writes and last INT value.
  - ARM/DSP semaphore read/write/ack counts.
  - CPU-supplied DSP EI writes/reads, including the old FreeDO random-return path at `0xf0..0xfc`.
  - Current DSP PC/status snapshots.
- Exported the same counters from the wasm build so browser runs can be inspected without native SDL.

## How to run the scripted comparison once the real compilation image is present

Build the headless runner:

```sh
make -f Makefile.headless clean
make -f Makefile.headless
```

Run the existing scripts:

```sh
./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_second_game.input 3200 100
./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_third_game.input 3200 100
./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_fourth_game.input 3200 100
```

For a direct-entry `LaunchMe` build, run the same executable and compare these fields frame-by-frame:

- `dsp_run`, `dsp_reset`, `dsp_int`, `dsp_intval`, `dsp_pc`, `dsp_status`
- `sema_arm_w`, `sema_arm_r`, `sema_dsp_w`, `sema_dsp_ack`
- `fiq_gen`, `fiq_need`, `fiq_last`, `irq0`, `irq1`
- `eif_rd`, `eif_empty`, `eif_reload`, `eof_wr`, `eof_dis`, `eof_full`, `fifo_last`
- `cpus_w`, `cpus_r`, `cpus_rand`, `cpus_ch`

The working hypothesis to confirm or falsify is: `mainLaunchMe` is leaving DSP/FIFO/AudioFIQ state in a configuration that 3DOh accepts permissively, while hardware exposes the bad state as a crash/hang.  The direct-entry black screen is consistent with missing launcher-initialized state, but the uploaded archive is insufficient to identify the exact file/register delta here.
