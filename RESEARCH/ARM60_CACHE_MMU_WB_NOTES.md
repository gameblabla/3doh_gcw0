# ARM60 cache/MMU/write-buffer pass

This pass adds a lightweight ARM60/ARM610-style CP15 model and uses it to make cache/MMU/write-buffer behavior visible to strict-mode debugging.

## Implemented

- CP15 MRC/MCR handling for coprocessor 15 registers 0..7.
- Undefined-instruction trap for unsupported coprocessor operations in strict mode.
- CP15 ID/control/translation-table-base/domain/fault-status/fault-address state.
- Control-register bits for MMU enable, unified IDC enable, and write-buffer enable.
- 4 KB mixed instruction/data cache model with 16-byte lines.
- Write-buffer queue for bufferable RAM stores.
- Simple first-level and second-level page-table translation, including section, large-page, and small-page descriptors.
- Domain/AP permission checks sufficient to produce translation/permission aborts instead of silently continuing.
- Unbufferable device/system accesses flush the write buffer before touching MADAM/CLIO/SPORT/NVRAM.
- Cache/write-buffer state is flushed on reset and before savestates.
- Debug counters exposed through the core and WASM frontend:
  - CP15 control register
  - CP15 operation count
  - cache flush count
  - write-buffer flush count

## Why this was added

Some real-hardware failures are caused by CPU/cache/DMA incoherence rather than by plain invalid pointers. The earlier strict mode caught several invalid bus/DSP paths, but it still treated the CPU as if all RAM stores were immediately visible and all cache-control operations were irrelevant.

This pass does not make the emulator a cycle-exact ARM60 implementation. It adds enough CP15 state to distinguish these cases:

- software explicitly enabling/disabling cache or write buffer;
- software installing page tables and selecting cacheable/bufferable regions;
- software relying on cache flushes before DMA/CEL/DSP access;
- illegal CP15/coprocessor accesses that should not be ignored.

## Gameblabla compilation result

The Gameblabla second/third/fourth-game scripts were rerun after this patch. The CP15 counters stayed at zero during the reproduced menu/launch paths. That means the currently reproduced paths do not appear to execute CP15 cache/MMU/write-buffer setup or maintenance at all.

Current conclusion: cache/MMU/write-buffer behavior was a valid thing to rule out, but it is probably not the direct cause of the Gameblabla mismatch in the reproduced test path. The remaining mismatch is more likely in one of these areas:

- ARM exception/abort delivery details after an invalid device/DSP/MADAM condition;
- CLIO/DSP audio state and Portfolio audio-folio resource handling;
- MADAM/CEL DMA side effects still not converted to a hardware-like abort;
- FIQ/IRQ ordering around audio/DMA;
- a remaining permissive memory window that should bus-error instead of returning harmless data.

## Useful commands

Native SDL3, with HLE SWI disabled by default:

```sh
make -f Makefile.sdl3
./3doh-sdl3 gameblablacompil_3do.iso bios.bin --input-script scripts/gameblabla_second_game.input
```

WASM diagnostic functions exported:

```c
threedoh_cp15_control()
threedoh_cp15_ops()
threedoh_cp15_cache_flushes()
threedoh_cp15_writebuffer_flushes()
```
