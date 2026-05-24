# Test notes for v13 strict CLIO correction

Problem reproduced with `Invades.iso` in v12 strict mode. It did not fail at core startup, but after several hundred frames strict mode entered a false data-abort:

```text
frame=350 ret=1 fault=5 addr=03400004 pc=00012364
```

`0x03400004` is CLIO offset `0x0004`, `CSysBits`, which is part of the Portfolio CLIO basic-control register block. Treating it as a DSP-bounds fault was incorrect.

Fix:

- Expanded the strict CLIO register whitelist to match the Portfolio CLIO register layout instead of only a small hand-picked subset.
- Kept the DSP/N-memory overflow checks intact.
- Added WASM exports for `threedoh_last_fault_address`, `threedoh_last_fault_pc`, and `threedoh_last_fault_type` so browser-side tests can diagnose future strict-fault regressions.

Tests run:

- Native strict regression runner with supplied BIOS and `Invades.iso`: passed 2000 emulated frames with no strict fault.
- `Clio.c` and `arm.c` compile-checked for both `BPP_TYPE=16` and `BPP_TYPE=32`.
- SDL3 and SDL1.2 backend object compile checks passed against local compatibility headers.
- WASM clang build: `make -f Makefile.wasm-clang clean && make -f Makefile.wasm-clang -j2` passed.
- WASM runtime with supplied BIOS and `Invades.iso`: started, executed 2000 frames, framebuffer populated, no strict fault.

```json
{ "frames": 2000, "bad": null, "nonzero": 76800, "lastFault": 0, "addr": 0, "pc": 0 }
```

- WASM runtime with supplied BIOS and `game.iso`: started, executed frames, no strict fault, soft reset returned success.

```json
{ "start": 0, "w": 320, "h": 240, "hz": 60, "fault": 0, "addr": 0, "pc": 0, "reset": 0 }
```

A real SDL3 windowed runtime test was still not possible in this container because SDL3 development/runtime libraries are not installed here.

---
# Test notes for v10

Reference checked: Longtris 3DO's `GestionAffichage.c` uses `GetDisplayType()`, maps `DI_TYPE_PAL1` to 320x288 at 50 Hz, maps `DI_TYPE_PAL2` to 384x288 at 50 Hz, and maps the fallback NTSC path to 320x240 at 60 Hz.

Changes in v10:

- `--pal`, `--pal1`, and `--force-pal` now force the emulated CLIO PAL/NTSC selector bit, not only host output timing and geometry.
- The forced PAL bit is reapplied after CLIO init/reset, after writes to ADBIOBits, and after CLIO savestate load.

WASM build:

```sh
make -f Makefile.wasm-clang clean
make -f Makefile.wasm-clang -j2
```

WASM runtime smoke test with the supplied `bios.bin` and `game.iso`, forced PAL before start:

```json
{ "start": 0, "frames": 4, "w": 320, "h": 288, "rate": 50, "badAlpha": 0, "nonzero": 92160, "reset": 0 }
```

CLIO PAL selector regression test:

```text
ok
```

The CLIO test verified that the PAL bit is clear by default, set by forced PAL before init, preserved after a BIOS-style write to ADBIOBits, cleared again by forced NTSC, and reasserted when loading a CLIO state while forced PAL is active.

A real SDL3 windowed runtime test was not possible in this container because SDL3 development/runtime libraries are not installed here.
# Test notes for v9

Reference checked: Longtris 3DO's `GestionAffichage.c` uses `GetDisplayType()`, maps `DI_TYPE_PAL1` to 320x288 at 50 Hz, maps `DI_TYPE_PAL2` to 384x288 at 50 Hz, and maps the fallback NTSC path to 320x240 at 60 Hz.

WASM build:

```sh
make -f Makefile.wasm-clang clean
make -f Makefile.wasm-clang -j2
```

WASM runtime smoke test with the supplied `bios.bin` and `game.iso`:

```json
[
  { "mode": 1, "start": 0, "frames": 4, "width": 320, "height": 240, "rate": 60, "alphaBad": 0 },
  { "mode": 2, "start": 0, "frames": 4, "width": 320, "height": 288, "rate": 50, "alphaBad": 0 }
]
```

Renderer unit test:

```text
pal_288_frame_copy_ok
```

Compile checks:

```sh
cc -std=gnu11 -Wall -Wextra -I/mnt/data/3doh_ui_v8/fake_sdl3 -Isrc -Isrc/freedo -DBPP_TYPE=32 -DTHREEDOH_PLATFORM_SDL3 -c src/platform/sdl3/threedoh_sdl3.c
cc -std=gnu11 -Wall -Wextra -I/mnt/data/3doh_ui_v8/fake_sdl12 -Isrc -Isrc/freedo -DBPP_TYPE=16 -c src/platform/sdl12/threedoh_sdl12.c
cc -std=gnu11 -Wall -Wextra -Isrc -Isrc/freedo -DBPP_TYPE=16 -c src/freedo/frame.c
cc -std=gnu11 -Wall -Wextra -Isrc -Isrc/freedo -DBPP_TYPE=32 -c src/freedo/frame.c
cc -std=gnu11 -Wall -Wextra -Isrc -Isrc/freedo -DBPP_TYPE=16 -c src/freedo/vdlp.c
cc -std=gnu11 -Wall -Wextra -Isrc -Isrc/freedo -DBPP_TYPE=32 -c src/freedo/vdlp.c
```

A real SDL3 windowed runtime test was not possible in this container because SDL3 development/runtime libraries are not installed here.

## v11 PAL centering fix

The PAL1 path was rechecked against Portfolio OS graphics behavior. `QueryGraphics(QUERYGRAF_TAG_DEFAULTDISPLAYTYPE, ...)` returns the graphics folio default display type, and `realCreateScreenGroup()` computes `vdl_Offset = (_DisplayHeight[displayType] - vdl_Height) / 2 + _DisplayOffset[displayType]`. 3DOh's PAL renderer now follows that behavior for 240-line VDL output inside a 288-line PAL1 field instead of anchoring it at y=0.

Additional local checks run for this package:

- `make -f Makefile.wasm-clang -j2`: passed.
- Forced-PAL WASM runtime with the supplied BIOS/ISO: passed. Output was 320x288 at 50 Hz, alpha-valid, with 240-line active content centered at lines 24..263 and black bars at 0..23 and 264..287.
- `frame.c`, `vdlp.c`, and `threedoh_core.c` compile-checked for both `BPP_TYPE=16` and `BPP_TYPE=32`: passed.
- SDL3 object compile checked with local SDL3 compatibility headers; link was not attempted because real SDL3 is not installed in this container.

## v12 strict ARM/CLIO/DSP fault pass

Changes checked for this package:

- WASM clang build: `make -f Makefile.wasm-clang clean && make -f Makefile.wasm-clang -j2` passed.
- WASM runtime smoke test with supplied BIOS/ISO passed:

```json
{ "start": 0, "frames": 4, "w": 320, "h": 240, "rate": 60, "alphaBad": 0, "nonzero": 76800, "reset": 0 }
```

- CLIO strict/compat regression harness passed. It verified that the valid DSP-memory window accepts accesses, the strict path rejects the old mirrored overflow area, compatibility mode still accepts that legacy mirror, and strict unknown CLIO reads raise the new ARM data-abort hook.
- `arm.c`, `Clio.c`, `DSP.c`, `frame.c`, `vdlp.c`, and `threedoh_core.c` compile-checked for both `BPP_TYPE=16` and `BPP_TYPE=32`: passed.

A real SDL3 windowed runtime test was not possible in this container because SDL3 development/runtime libraries are not installed here.
