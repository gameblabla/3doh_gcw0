# MADAM strictness adjustment (v17)

v16 treated the MADAM CEL work guard as a strict ARM data abort. That was too aggressive.

The observed optiDoom/DOOM hack failure was:

```
Strict fault: MADAM cel runaway at PC=00045540 address=03300000
Strict fault: MADAM cel runaway at PC=00045558 address=03300000
```

That address is not a bad ARM-side bus address. It was the emulator's synthetic fallback address for a host-side CEL work budget exhaustion.

The fix in v17 changes MADAM CEL runaway handling from "strict ARM abort" to "soft CEL termination / clipping" by default. CPU, CLIO, DSP, MMU, and real memory-range faults remain strict. This better matches the practical behavior of 3DO software: malformed CPU/DSP access is much more likely to kill the machine than a large or awkward CEL render.

New SDL hosted options:

```
--compat-madam          default; MADAM work-budget exhaustion clips/terminates the current CEL work without ARM abort
--strict-madam          debug option; restores v16-style MADAM runaway aborts
--strict-bus            still default for CPU/CLIO/DSP/bus faults
--compat-bus            older permissive bus/device behavior
```

The host strict-fault message now includes `madamclip=<count>`, and WASM exports `threedoh_madam_soft_clip_count()` for diagnostics.

The work guard remains present to prevent emulator host lockups. It just no longer claims that the hardware would necessarily crash.
