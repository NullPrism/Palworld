# Aster's No Saddle

**Aster's No Saddle** is a native UE4SS C++ mod for **Palworld** that allows rideable Pals to be mounted **without crafting or equipping their saddle**.

The mod preserves each Pal's native ride behavior, animations, controls, attacks, and dismount logic while removing the saddle requirement.

---

## Features

- Ride Pals without crafting their saddle
- Preserves each Pal's original ride type
  - HorseRide
  - BiggerHorseRide
  - SitRide
  - BackRide
- Removes the in-game **"Locked"** interaction when a saddle is the only restriction
- Preserves normal riding controls, mounted attacks, and dismount behavior
- Restores all modified runtime values immediately after mounting
- No Blueprint edits
- No asset replacement
- No save editing

---

## Current Tested Versions

| Component | Version |
|-----------|---------|
| Palworld | **v1.0.0.100427** |
| UE4SS | **v3.0.1** |
| Platform | Windows (Steam) |

---

## Downloads

Pre-built releases are available in the repository's **release/** directory.

Each release contains a ZIP archive that is ready to install.

---

## Installation

1. Install **UE4SS v3.0.1** for Palworld.

2. Download the latest release ZIP from:

```
release/
```

3. Extract the ZIP.

After extraction you should have:

```
AstersNoSaddle/
├── enabled.txt
└── dlls/
    └── main.dll
```

4. Copy the **AstersNoSaddle** folder into:

```
Palworld/
└── Mods/
    └── NativeMods/
        └── UE4SS/
            └── Mods/
```

The final directory structure should look like:

```
Palworld/
└── Mods/
    └── NativeMods/
        └── UE4SS/
            └── Mods/
                └── AstersNoSaddle/
                    ├── enabled.txt
                    └── dlls/
                        └── main.dll
```

5. Start Palworld.

---

## Repository Layout

```
AstersNoSaddle/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── src/
│   ├── CMakeLists.txt
│   └── dllmain.cpp
└── release/
    ├── AstersNoSaddle-v1.0.2.zip
    └── ...
```

The repository stores:

- Complete source code
- Build configuration
- Ready-to-install release packages

The compiled DLL itself is distributed inside the release ZIPs rather than stored separately in the repository.

---

## How It Works

The mod operates entirely at runtime using UE4SS native C++ hooks.

When the player activates a rideable Pal, the mod:

1. Removes the saddle requirement from the active Partner Skill.
2. Allows the game's normal interaction flow to proceed.
3. Identifies the Pal's native ride marker.
4. Preserves the marker's original ride position.
5. Executes the game's own ride logic using the appropriate ride type.
6. Restores the original ride position immediately afterward.

Because the game's own ride implementation is used, mounted movement, attacks, animations, and dismount behavior remain unchanged.

No game assets are modified, and no permanent data is written to save files.

---

## Logging

Normal startup produces only three log messages:

```
[AstersNoSaddle] DLL constructed. Version x.x.x.
[AstersNoSaddle] Unreal initialized.
[AstersNoSaddle] Ready. Partner restrictions will be removed on activation.
```

Additional log messages are only written if an unexpected condition or compatibility issue is detected.

---

## Compatibility

The mod was written specifically for:

- Palworld **v1.0.0.100427**
- UE4SS **v3.0.1**

Future game updates may require changes if Palworld's internal runtime structures change.

---

## Building

The source code is included for anyone wishing to build the mod themselves.

Requirements:

- UE4SS C++ project
- CMake
- LLVM/Clang
- xwin SDK (Linux cross-compilation or Visual Studio toolchain on Windows)

Compile using the standard UE4SS native module build process.

---

## License

MIT License

---

## Credits

Created by **Aster**

GitHub: https://github.com/NullPrism

Special thanks to the UE4SS developers for creating the native C++ modding framework that made this project possible.
