# Gameblabla hardware-crash investigation, continuation

This pass follows the FIFO-starvation result from the prior package. The key correction is that an exhausted active CLIO EI FIFO must not be silently disabled by the emulator. A disabled FIFO and a starved-but-still-armed FIFO are different hardware states.

Old 3DOh behavior, even after one-shot reload consumption, did this when an active EI FIFO drained with no queued reload:

```c
FIFOI[channel].StartAdr = 0;
return 0;
```

That makes a still-armed DSP instrument become a quiet zero source. It also removes repeated empty-FIFO interrupt pressure from the emulated machine. On hardware, the channel remains programmed until the ARM/audio folio explicitly clears or reloads it; the empty condition remains observable and continues to assert the relevant CLIO FIFO interrupt as the DSP keeps trying to consume the starved input.

## Change in this pass

- EI FIFO end-of-active-buffer with no reload no longer clears `StartAdr`.
- Repeated incrementing and non-incrementing reads from an exhausted but still programmed EI FIFO now keep generating the channel empty FIQ and return zero.
- Truly disabled FIFOs (`StartAdr == 0`) still return zero quietly.

## Why this matters for Gameblabla

The prior trace already showed that path 4 enters a runaway starvation state on channels 0, 1, and 2. Quietly disabling the FIFO converts that hardware-visible starvation into benign zeros. Keeping the FIFO armed makes the emulated interrupt condition match the failing hardware case more closely.

This is still not a forced title-specific crash; it is a generic CLIO FIFO state-machine correction.
