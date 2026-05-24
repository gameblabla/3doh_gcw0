# Input scripting

Hosted SDL builds accept deterministic controller scripts. This is useful for reproducing crashes that require menu navigation.

Inline events:

```sh
./3doh-sdl3 game.iso bios.bin --input "1550f:P,1930f:DOWN,1980f:A"
```

Script file:

```sh
./3doh-sdl3 game.iso bios.bin --input-script scripts/gameblabla_second_game.input
```

Event forms:

```text
1550f:P          # tap P at frame 1550
1930f:DOWN       # tap DOWN at frame 1930
25.833:A         # tap A at 25.833 seconds
1930f:DOWN:down  # hold DOWN
1945f:DOWN:up    # release DOWN
1930f:+DOWN      # equivalent to down
1945f:-DOWN      # equivalent to up
```

Taps are released automatically after `--input-hold-ms`, defaulting to 120 ms.

Known Gameblabla compilation repro scripts:

```sh
./3doh-sdl3 gameblablacompil_3do.iso bios.bin --input-script scripts/gameblabla_second_game.input
./3doh-sdl3 gameblablacompil_3do.iso bios.bin --input-script scripts/gameblabla_third_game.input
./3doh-sdl3 gameblablacompil_3do.iso bios.bin --input-script scripts/gameblabla_fourth_game.input
```
