# 3DOh clang/WebAssembly browser port

This tree adds a browser-oriented WebAssembly target that uses upstream clang/LLD rather than Emscripten, SDL, Asyncify, or WASI.

## Build

```sh
make -f Makefile.wasm-clang
```

Equivalent shorthand:

```sh
make -f Makefile.wasm
```

Serve the output locally:

```sh
make -f Makefile.wasm-clang serve
```

Then open `http://localhost:8000/`.

The generated package is written to:

```text
build/wasm-clang/index.html
build/wasm-clang/3doh.js
build/wasm-clang/3doh.wasm
```

## Runtime model

The browser owns the asynchronous frame loop through `requestAnimationFrame()`. The C side exports:

```c
int      threedoh_start(void);
int      threedoh_frame(void);
int      threedoh_soft_reset(void);
void     threedoh_shutdown(void);
uint8_t *threedoh_framebuffer_ptr(void);
void     threedoh_input_button_event(int button, int pressed);
```

Each `threedoh_frame()` call executes one 3DO frame, renders a 320x240 RGBA framebuffer, and exposes a per-frame audio sample block for JavaScript to enqueue into Web Audio.

## BIOS and ISO loading

The build does not embed or preload any BIOS or game image. The page refuses to start unless the user supplies both files. The browser UI uses two separate drop targets: one for the BIOS and one for the ISO, each with its own file picker:

```text
bios.bin
*.iso
```

The BIOS and ISO stay in browser memory. The C filesystem shim reads BIOS bytes and ISO sectors through explicit JavaScript host imports. This avoids copying a whole CD image into WebAssembly linear memory.

## Reset and game switching

The browser toolbar has two runtime-management buttons:

```text
Soft reset
Load new game
```

`Soft reset` reinitializes the emulated machine state against the currently loaded BIOS and ISO without asking for files again.

`Load new game` stops the current runtime, keeps the BIOS bytes loaded, clears the ISO slot, reopens the modal loader, and refuses to start until a new ISO is supplied.

## Controls

Default keyboard mapping:

```text
Arrow keys / WASD       D-pad
Z / Enter               A
X / Space               X
C                       B
V                       C
Q / E                   L / R
P                       Pause / Start
```

The Controls panel includes persistent keyboard remapping in `localStorage`. Gamepad API polling is also wired with the usual standard-pad layout and analog-axis fallbacks.

## Fullscreen and audio

The Fullscreen button requests browser fullscreen on the emulator stage. Web Audio is resumed from a user gesture as required by browser autoplay policy. The first click, key press, or gamepad input attempts to resume audio.

## 16bpp and 24bpp/32bpp display paths

The old compact 16bpp renderer remains guarded behind `BPP_TYPE != 32`; this is the path to use for GCWZERO-class targets.

The WASM target is compiled with:

```text
-DBPP_TYPE=32
```

The 32bpp path renders directly to browser `ImageData` RGBA output and keeps the additional VDL source data needed for 24-bit-style output:

- canonical `0xRRGGBB` background colors;
- independent 32-entry R/G/B CLUT channels with 8-bit output values;
- fixed-CLUT bypass handling when requested by `VDL_CLUTBYPASSEN`;
- `VDL_BLSB_BLUE` handling, where bitmap bit 15 becomes the least-significant blue bit for custom-CLUT output;
- current/previous bitmap sampling for the 32bpp renderer when VDL interpolation bits are present.

The 16bpp-only build does not store the extra current-line buffer and converts RGB888 VDL output back to RGB565.

## Files added or changed for the port/refactor

```text
Makefile.wasm-clang
Makefile.wasm
Makefile.sdl12
Makefile.sdl3
BACKENDS.md
web/wasm_clang/index.html
web/wasm_clang/3doh.js
src/core/threedoh_core.c
src/core/threedoh_core.h
src/platform/threedoh_platform.h
src/platform/common/input_packet.h
src/platform/wasm/threedoh_wasm.c
src/platform/wasm/wasm_fs.c
src/platform/wasm/wasm_input.c
src/platform/wasm/wasm_sound.c
src/platform/sdl12/threedoh_sdl12.c
src/platform/sdl3/threedoh_sdl3.c
src/wasm/freestanding/wasm_runtime.c
src/wasm/freestanding/include/*.h
```

`src/freedo/freedocore.h`, `src/freedo/frame.c`, and `src/freedo/vdlp.c` contain the guarded display-path changes for 16bpp-only and 32bpp/24-bit-aware output. The old `src/video.c`, `src/input.c`, and `src/sound.c` are retained for reference but are not used by the new backend Makefiles.
