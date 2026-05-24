# Alone in the Dark PAL research pass v7

This pass adds `Alone in the Dark (Europe)` as a PAL-only regression case.

Observed baseline on the v6 tree:

```
stop frame=72 fault=5 pc=00008308 addr=034025e0
```

`0x034025e0` is inside the CLIO DSPP N16 host window.  The prior strict-bus whitelist allowed host writes to this DSPP N window but treated host reads as invalid.  That is too strict for BIOS/Portfolio code.  Opera falls through to the generic CLIO register array for N-window reads, while MAME models these windows as host write paths.  Therefore the hardware-safe behavior for now is: aligned N32/N16 reads are legal CLIO reads and return the latched CLIO register word, not a strict DSP/CLIO bounds fault.

The false strict fault is fixed by whitelisting the aligned DSPP N windows for reads.  Diagnostic counters were added for these reads.

The later Alone PAL loading/timing issue is not declared fixed here.  Enabling the old `FIX_BIT_TIMING_6` path lets the headless run pass 4000 frames, but that path is still a timing hack.  Its effect points to missing XBUS/CD DMA latency or timer coupling rather than to the DSPP N-window read itself.

The prior AUDLOCK-reset experiment has also been disabled in the DSP scheduler.  Resetting DSP program state directly from every 0x3eb/AUDLOCK write generated millions of synthetic resets in Alone PAL and is broader than MAME’s output-frame handling.
