# Gameblabla minigame 2/3/4 hardware-crash research, pass 3

This pass continues from the FIFO-starvation work. It does not force a crash. It adds stricter CLIO FIFO signalling and diagnostics to narrow down why the emulator still survives cases reported to crash on real hardware.

## Changes made

- A programmed EI FIFO whose active span is exhausted and has no queued reload is now treated as still armed instead of fully idle.
  - `_clio_GetEIFIFOStat()` now returns `1` for this starved-but-armed state.
  - It still returns `2` when active data exists or a valid reload span is pending.
  - It returns `0` only when the FIFO is actually disabled.
- EI FIFO empty interrupts are reasserted as a level condition while the FIFO remains programmed and empty.
  - Clearing IRQ pending bits no longer permanently hides an empty programmed FIFO.
  - The current empty-FIFO level mask is re-applied after IRQ clears, FIFO programming, and empty FIFO reads.
- Added diagnostics for:
  - CLIO FIFO level reassert count and mask.
  - ARM FIQ entry count, separate from the existing CLIO pending/need counters.
  - Active FIFO descriptor snapshots in the headless runner.

## Evidence from the real Gameblabla compilation ISO

All runs below used the scripted menu handoff path, not direct root `LaunchMe` replacement.

### Minigame 2 path, 6000 frames

- No ARM fault yet.
- ARM FIQ entries: 31,106.
- EI empty reads: 2.
- Starvation remains minor and appears on channels 4/5 only.
- Level reassertions: 15, last mask `0x00200000`.

### Minigame 3 path, 6000 frames

- No ARM fault yet.
- ARM FIQ entries: 28,916.
- EI empty reads: 38,240.
- Starvation is now visible on EI FIFO channel 0:
  - `ch0=38239/4`
  - FIFO0 descriptor: `StartAdr=0x001eccac`, remaining length 20, no reload.
- Level reassertions: 15, last mask `0x00010000`.

This is a new useful discriminator: the old emulator semantics effectively allowed this channel-0 starvation to look less severe.

### Minigame 4 path, 6000 frames

- No ARM fault yet.
- ARM FIQ entries: 29,146.
- EI empty reads: 99,942.
- Starvation remains concentrated on EI FIFO channels 0, 1, and 2:
  - `ch0=35004/1`
  - `ch1=33534/1`
  - `ch2=31403/1`
- FIFO descriptors are below the 3 MB main-memory limit:
  - FIFO0: `StartAdr=0x001eccaa`, remaining length 22, no reload.
  - FIFO1: `StartAdr=0x001eccaa`, remaining length 22, no reload.
  - FIFO2: `StartAdr=0x001ecca0`, remaining length 32, no reload.
- Level reassertions: 17, last mask `0x00040000`.

## What this rules out

- This still does not look like a simple 2 MB vs 3 MB RAM-size problem. The starved FIFO buffers are under `0x00200000`.
- The emulator is taking FIQs; the issue is not simply that CLIO pending bits never become ARM-visible.
- Raw DSPP resource-window faults remain unsupported as the primary explanation because previous strict-DSPP traces did not fire those counters on the scripted paths.

## Current working conclusion

The strongest verified mismatch is still in the CLIO/audio FIFO and sound-spooler failure path, not in direct booting, raw DSPP resource windows, or obvious main-RAM bounds.

This pass makes CLIO starvation more hardware-like by preserving armed-empty FIFO state and reasserting empty FIFO interrupt pressure. It exposes additional starvation in minigame 3 and preserves the already-severe minigame 4 starvation. However, it still does not reproduce the real hardware crash. The next likely targets are:

1. exact EI FIFO status bit semantics as seen by DSP code;
2. audio folio/soundspooler handling of starvation signals;
3. CLIO interrupt acknowledge/mask semantics for repeated FIFO-empty events;
4. DSP sample FIFO timing and whether these channels should produce underrun side effects beyond returning zero.
