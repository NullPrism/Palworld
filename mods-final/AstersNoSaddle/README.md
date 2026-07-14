# Aster's No Saddle

**Aster's No Saddle** is a native UE4SS C++ mod for **Palworld** that allows rideable Pals to be mounted **without crafting their saddle**.

The mod preserves each Pal's native ride behavior, animations, controls, attacks, and dismount logic while removing the saddle requirement.

---

## Features

- Ride Pals without crafting their saddle
- Preserves each Pal's original ride type
  - HorseRide
  - BiggerHorseRide
  - SitRide
  - BackRide
- Restores all modified runtime values immediately after mounting
- Removes the in-game **"Locked"** interaction when a saddle is the only restriction
- Supports mounted attacks and abilities
- Supports normal dismounting
- No Blueprint edits
- No asset replacement
- No save editing

---

## Tested Versions

| Component | Version |
|-----------|---------|
| Palworld | **v1.0.0.100427** |
| UE4SS | **v3.0.1** |
| Platform | Windows (Steam) |

---

## Installation

Install UE4SS first.

Copy the mod folder into:

```
Palworld/
└── Mods/
    └── NativeMods/
        └── UE4SS/
            └── Mods/
                └── AstersNoSaddle/
```

The folder should contain:

```
AstersNoSaddle/
├── enabled.txt
└── dlls/
    └── main.dll
```

Restart Palworld.

---

## How It Works

The mod operates entirely at runtime using UE4SS C++ hooks.

When the player activates a rideable Pal, the mod:

1. Removes the saddle requirement from the active Partner Skill.
2. Allows Palworld's normal interaction flow to proceed.
3. Determines the Pal's native ride marker.
4. Preserves the marker's original ride type.
5. Performs the mount using the game's own ride logic.
6. Restores the original ride type immediately afterward.

No game assets are modified, and no permanent data is written to save files.

---

## Notes

- Only affects the **local player**.
- Only modifies the **currently active Partner Pal**.
- Original ride behavior is preserved.
- If the mod is removed, saves continue to function normally.

---

## Logging

On startup, the mod writes:

```
[AstersNoSaddle] DLL constructed. Version x.x.x.
[AstersNoSaddle] Unreal initialized.
[AstersNoSaddle] Ready. Partner restrictions will be removed on activation.
```

Additional messages are only written when an unexpected condition or compatibility issue is encountered.

---

## Source Layout

```
AstersNoSaddle/
├── README.md
├── src/
│   ├── CMakeLists.txt
│   └── dllmain.cpp
└── main/
    └── main.dll
```

---

## Building

This project is built as a native UE4SS C++ module.

Requirements:

- UE4SS C++ project
- CMake
- LLVM/Clang
- xwin SDK (Linux cross-compilation)

Compile normally using the UE4SS build system.

---

## Compatibility

The mod was written specifically for:

- Palworld v1.0.0.100427
- UE4SS v3.0.1

Future game updates may require changes if internal game structures change.

---

## License

MIT License

---

## Credits

Created by **Aster**

GitHub:
https://github.com/NullPrism

Special thanks to the UE4SS developers for making native runtime modding possible.
