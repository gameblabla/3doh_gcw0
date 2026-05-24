# Gameblabla minigame hardware-crash research, pass V5

This pass replaced the old FreeDO/3DOh DSPP execution model with a reload-driven scheduler.

## Old behavior

`_dsp_Loop()` was called from the 44.1 kHz audio queue and restarted the DSP program on every generated sample. That meant the DSP program was run as a whole burst once per host audio sample, whether or not the DSPP down-counter had actually reached its reload point.

That can mask timing-sensitive launcher/minigame transitions: if software programs a reload value other than the default 567, the emulator still gives it a restart at every audio sample.

## New behavior

`_dsp_Loop()` is still called by the 44.1 kHz audio queue, but now it acts as a scheduler tick:

- the DSPP counter is decremented by 567 DSP ticks per 44.1 kHz sample interval;
- the DSP program is restarted only when the counter reaches or crosses zero;
- multiple reloads in one sample interval are allowed, capped defensively at 64 to keep broken reload programming deterministic;
- the DAC sample returned to the mixer is always the current `0x3fe/0x3ff` output pair.

This follows the direction implied by MAME's 3DO TODO about restarting DSPP execution on counter reloads, rather than restarting blindly once per audio sample.

## Added diagnostics

New DSP counters are printed by the headless runner and exported to wasm/browser diagnostics:

- `dsp_ticks`
- `dsp_reload`
- `dsp_frames`
- `dsp_sleep`
- `dsp_defer`
- `dsp_multi`
- `dsp_cnt`
- `dsp_prld`
- `audlock` write/reset/last-value triplet

## Audlock observation

Gameblabla minigame paths write DSP audio status value `0x8000` heavily. MAME's DSPP model treats the MSB write at `0x3eb` as AUDLOCK. This build records those writes and resets the DSP program state after an AUDLOCK frame, but this still does not make minigames 2, 3, or 4 hard-fault in 3DOh.

## Current result

The new scheduler changes the timing profile materially. In the fourth minigame path at frame 4500, the trace shows roughly:

- `dsp_ticks ~= 3.3M`
- `dsp_reload ~= 258k`
- `dsp_frames ~= 255k`
- `audlock` writes/resets ~= 149k

Despite that, the emulator still does not reproduce the real-hardware crash. The remaining discrepancy is likely below the level of simple DSPP restart cadence: either exact DSPP FIFO/underflow interrupt semantics, exact AUDLOCK/output FIFO behavior, or Portfolio audio folio's task/signal failure propagation.

This pass should be kept as an accuracy improvement and a better diagnostic base, not treated as a final proof of the hardware crash cause.
