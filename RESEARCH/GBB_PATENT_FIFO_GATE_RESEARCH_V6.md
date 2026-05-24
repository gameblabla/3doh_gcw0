# Gameblabla / CLIO FIFO DMA gate research v6

Prompt for this pass: compare the Opera commit 7cef4a724a52dcf26046719ba43ae23cac10c6c7, the patent it cites, and MAME's 3DO driver.

## Patent reference

The Opera commit explicitly cites `WO09410641A1` in the CLIO FIFO comments. In modern publication-number form this appears to mean `WO94/10641 A1` / `WO1994010641A1`. I could not retrieve an independent patent-database page from the available search tools, so the patent identification here is taken from the Opera commit text itself, not independently verified title/assignee metadata.

The relevant patent locations cited by Opera are:

- `46.25`: writing the current/start FIFO descriptor clears the next descriptor.
- `49.16`: FIFO reload from the next descriptor.

## Comparison

### Opera/libretro commit

Opera's December 2023 change says stale next descriptors must not reload unless the corresponding CLIO DMA enable bit is still set. If a FIFO reaches the end and either no next address exists or DMA is disabled, Opera disables the FIFO by clearing `start.addr` and returns zero for EI reads. It does not clear `next.addr`/`next.len` on a successful reload; that descriptor remains reusable while DMA remains enabled.

This directly contradicts the previous v2/v3 research patch in this tree, which made every next descriptor one-shot and kept exhausted FIFOs armed to create persistent starvation. That older behavior was useful diagnostically because it exposed starvation, but it is probably not hardware-correct.

### MAME

MAME has the more architecturally complete DSPP CPU and DSPX-style DMA model, and it explicitly has an output FIFO underflow path that returns previous data. However, MAME's current 3DO Green CLIO mapping still has several CLIO/DSPP TODOs. In particular, the Green CLIO FIFO register block at `0x0300` is not modeled at the same level as Opera/FreeDO's old CLIO FIFO path, and MAME's driver comments still mention incomplete DSPP IRQ/reload semantics.

### Choice for this codebase

For the specific Green CLIO FIFO reload behavior used by Gameblabla, Opera/libretro's 2023 commit is the better source to follow. It is built on the same FreeDO-derived CLIO FIFO model as 3DOh/Opera and references both Portfolio OS `clio.h` and the 3DO patent comment locations. MAME remains valuable for DSPP execution, AUDLOCK, and previous-sample underflow behavior, but is less directly useful for this specific FIFO register path because that CLIO FIFO block is not equivalently implemented there.

## Implemented in this pass

- EI FIFO reload now requires:
  - `NextAdr != 0`, and
  - DMA enable bit `0x304[channel]` set.
- EI FIFO no longer clears `NextAdr`/`NextLen` after successful reload.
- EI FIFO disables itself by clearing `StartAdr` when it reaches end-of-current and cannot reload.
- EO FIFO reload now requires:
  - `NextAdr != 0`, and
  - DMA enable bit `0x304[16 + channel]` set.
- EO FIFO no longer clears `NextAdr`/`NextLen` after successful reload.
- Added diagnostics:
  - `reload_dma_block`
  - `last_reload_dma_block`

## Gameblabla result

The title still does not hard-crash like real hardware. This is still not a fake-crash patch.

However, the diagnostics changed materially:

- Previous v3/v4 behavior produced enormous EI empty/starvation counts for minigames 3 and 4.
- With Opera/patent-style reusable-next reloads gated by DMA enable, those empty counts collapse back to zero in the tested paths.
- Minigame 4 reaches frame 6000 with repeated reloads on channels 0, 1, 2, and 4, no strict fault, and no DMA-disabled reload block.

This means the earlier starvation storm was an artifact of the one-shot-next patch, not a trustworthy real-hardware failure mechanism.

## Current best lead after this pass

The real-hardware crash is now less likely to be raw CLIO FIFO starvation. The next likely targets are:

1. Exact DSPP instruction behavior / status flags.
2. DSPP audio frame/AUDLOCK reset semantics.
3. CLIO BadBits / underflow-overflow reporting to Portfolio OS.
4. Portfolio audio-folio task/signal behavior when DSPP FIFO state is unusual.

The important correction is that `next` FIFO descriptors should persist and be gated by DMA enable rather than consumed once.
