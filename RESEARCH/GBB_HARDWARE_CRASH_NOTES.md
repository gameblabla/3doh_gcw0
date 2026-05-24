# Gameblabla compilation investigation notes

This pass used the real `gameblablacompil_3do.iso` from `gbb(1).zip` and the scripts in `scripts/`.

## Reproduction path used

```
make -f Makefile.headless clean
make -f Makefile.headless

./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_second_game.input 3600 200
./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_third_game.input 3600 200
./3doh-headless bios.bin gameblablacompil_3do.iso scripts/gameblabla_fourth_game.input 3600 200
```

The stock emulator path does not stop on a CPU/DSP strict fault for any of those scripted paths. That matches the reported emulator behavior.

## Direct LaunchMe replacement result

Replacing the root `LaunchMe` directory entry with `kill`, `menu`, or `Vigo` does not launch the selected mini-game as a standalone title. Those root entries are directory/application containers, not direct Opera boot executables. Replacing `LaunchMe` with these entries consistently reaches a bad item/unmapped access path and stops at:

```
fault=1 pc=001e8b98 addr=fffffed2
```

Replacing `LaunchMe` with root files such as `killminds`, `helicopters`, or `hinote` is also invalid as a boot path: those files begin with asset/file-table records, not ARM executable code. The observed direct-launch black screen is therefore expected and does not prove that those games are standalone boot images.

## State inherited from main LaunchMe

The normal menu path enters the second/third/fourth games with the DSP already started and the audio/CLIO path already configured. At the handoff, the launcher has already established:

- DSP run state: `dsp_run=1/0`
- DSP PC/status around `dsp_pc=02d`, `dsp_status=003dc901`
- ARM/DSP semaphore traffic
- CLIO FIQ masks such as `e0100a06` and `e0300a06`
- EI FIFO streaming/reload activity, especially channel 4/5 and later channel 0/2 depending on the selected game

That means the meaningful comparison is not “direct LaunchMe versus menu LaunchMe”. The direct path is malformed. The relevant inherited state is the active audio-folio / DSP / CLIO FIFO state set by the real launcher before the selected game begins.

## Correctness fixes applied in this pass

Two concrete permissive behaviors were found in the shared core:

1. DSP CPU-supplied EI reads at `0xf0..0xfc` returned pseudo-random data even though the value is already latched in DSP EI memory. The old source even had the correct `IMem[addr - 0x80]` path commented out. This is now fixed to return the latched word.

2. CLIO EI FIFO status and non-increment reads were too permissive. `_clio_GetEIFIFOStat()` reported “ready” whenever `StartAdr` was nonzero, even after the active span was exhausted. `_clio_EIFIFONI()` could still read memory for an empty/disabled FIFO. The patched version reports ready only when the active span has data or a reload span is pending, and non-increment reads from empty/disabled EI FIFO return zero instead of reading address zero.

These changes affect SDL1.2, SDL3, and wasm because they are in the shared FreeDO core.

## Remaining mismatch

After these corrections, the scripted Gameblabla second/third/fourth paths still do not produce a strict CPU/DSP fault in the emulator. The evidence so far rules out the earlier raw DSPP resource-window theory and rules out direct-boot register magic. The remaining likely mismatch is higher-level audio/CLIO behavior: FIFO/DMA reload timing, FIQ ordering/acknowledgement, or audio-folio failure propagation around the already-running DSP state.
