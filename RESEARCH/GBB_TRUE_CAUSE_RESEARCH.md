# Gameblabla hardware-crash investigation notes

This pass continues the Gameblabla second/third/fourth minigame investigation.

## Confirmed negative result

The earlier strict DSPP resource/window checks did not fire on the scripted paths. EI/EO/N-memory window misuse is therefore not the immediate mismatch exposed by these scripts.

## Direct LaunchMe replacement is not a valid control

Replacing the root `LaunchMe` with files such as `kill`, `menu`, `Vigo`, `killminds`, `helicopters`, or `hinote` does not boot the same program in a clean state. Several of those objects are small containers or asset/file-table records rather than direct Opera startup executables; the resulting black screen is a malformed boot path, not proof of a single missing register initialization.

## New finding: CLIO EI FIFO reload is too permissive

The old CLIO EI/EO FIFO reload path copied `NextAdr/NextLen` into `StartAdr/StartLen` but left the reload registers intact. That makes a one-shot reload buffer behave like a permanent circular reload. On real CLIO-style FIFO programming, once the queued reload has been consumed, the next slot must no longer be pending unless the CPU/audio folio writes a new reload descriptor.

This emulator behavior masks audio FIFO starvation. After making reload consumption one-shot by clearing `NextAdr/NextLen` on reload, the fourth Gameblabla path shows massive EI FIFO underflow after the minigame handoff:

- fourth path, frame 3000: `eif_empty=62534`, channel 0 and 1 empty counts already high;
- fourth path, frame 5000: `eif_empty=1147482`, with channel 0/1/2 starvation continuing;
- while the old behavior reported only one empty read because it kept reusing the stale reload descriptor.

The first path remains stable with only the expected end-of-buffer event on channel 4. The second path continues streaming channel 5 after handoff. The third path stops consuming additional EI FIFO data after entering the minigame path, but does not show the same runaway empty-read storm as the fourth path in this trace.

## What changed in code

- EI FIFO reload now consumes the queued reload descriptor: `NextAdr/NextLen` are cleared when copied into `StartAdr/StartLen`.
- EO FIFO reload now does the same.
- Added ARM current PC/CPSR diagnostics.
- Added per-channel EI FIFO empty/reload diagnostics.
- Added wasm exports and browser-console diagnostics for the new counters.

## Current conclusion

The strongest concrete mismatch found so far is CLIO FIFO reload semantics: 3DOh was treating reload descriptors as endlessly reusable, which hides FIFO underflow/starvation that appears during the Gameblabla fourth-game path. This is plausibly on the causal chain for the real-hardware crash/freeze, because the title uses the 3DO sound spooler and DSP instruments heavily, and those paths depend on accurate EI FIFO reload/underflow/FIQ behavior.

This still does not prove the final CPU-level abort vector for all reported minigame paths. It narrows the remaining work to audio folio / sound spooler behavior after FIFO starvation, not raw DSPP resource-window faults.
