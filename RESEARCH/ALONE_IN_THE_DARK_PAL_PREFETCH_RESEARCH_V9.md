# Alone in the Dark PAL research v9

This pass addresses the strict-mode abort reported after the Krisalis logo:

```
Strict fault: unaligned prefetch at PC=10411882 address=10411882
```

The old strict behavior aborted immediately when `REG_PC & 3` was non-zero. That is too strict for this code path. The ARM instruction bus is word-granular, and this title reaches a return path where R15 still carries status/low-bit residue. The emulator now records that event, aligns the fetch address, and then lets the normal bus/MMU/device mapping decide whether the target is valid.

The aligned target was still outside the emulator's mapped windows (`0x10411880`). The useful executable code exists at the low DRAM decode mirror (`0x00011880`), so instruction prefetch now mirrors only the low 2 MB DRAM decode when the fetch address is outside normal RAM/ROM/device windows. This is deliberately restricted to instruction prefetches; normal data/device accesses remain strict.

Result: Alone in the Dark PAL no longer trips the strict unaligned-prefetch fault and reaches the post-logo idle/wait loop without a strict abort.

This does not yet fix the black-screen/post-logo load stall. The trace now shows the ARM parked around `0x00019660`, polling CLIO `VCnt` (`0x03400034`) with interrupts masked. That points to a remaining timing/video-counter/Portfolio wait-loop issue rather than the previous strict prefetch fault.

The v8 broad XBUS timing correction was narrowed to match the hardware-shaped part of Opera's Timing Hack 6 more closely: XBUS DMA pressure slows the CLIO timer source, while the large ARM bus-time correction is only applied in the same low-CEL-pressure window used by the original timing logic. This avoids applying the full correction to every DMA pulse.
