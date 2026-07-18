# Aster's No Saddle

[![Join the Null Prism Discord](https://img.shields.io/badge/Join%20the%20Null%20Prism%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/CC4psCt2y5)

**Aster's No Saddle** is a native UE4SS C++ mod for **Palworld** that allows compatible rideable Pals to be mounted without crafting their saddle.

The mod preserves each Pal's normal movement, animations, mounted attacks, controls, networking flow, and dismount behavior.

## Downloads

- [Steam Workshop](https://steamcommunity.com/sharedfiles/filedetails/?id=3764580866)
- [Nexus Mods](https://www.nexusmods.com/palworld/mods/3772)
- [Manual v1.0.5 release ZIP](release/AstersNoSaddle-v1.0.5.zip)
- [Release checksums](release/SHA256SUMS)

## Requirements

| Component | Supported configuration |
|---|---|
| Palworld | Windows Steam release |
| UE4SS | UE4SS Experimental for Palworld, v3.0.1 |
| Architecture | x86-64 |
| Mod version | v1.0.5 |

The mod was developed and validated against Palworld `v1.0.0.100427`.

Future Palworld or UE4SS updates may require compatibility changes if internal classes, reflected functions, or runtime structure offsets change.

## Features

- Ride compatible Pals without crafting their saddle
- Shows **Ride** immediately instead of an initial **Locked** prompt
- Preserves each Pal's native riding implementation
- Preserves mounted movement and attacks
- Preserves normal dismount behavior
- Supports repeated mounting and dismounting
- Supports single-player and multiplayer clients
- Makes no Blueprint or asset replacements
- Makes no permanent save-file changes

## Installation

### Steam Workshop

1. Subscribe to **UE4SS Experimental for Palworld**.
2. Subscribe to **Aster's No Saddle**.
3. Start Palworld.
4. Open **Options → Mod Management**.
5. Enable UE4SS and Aster's No Saddle.
6. Save the configuration and allow Palworld to restart.

### Manual installation

1. Install UE4SS Experimental for Palworld.
2. Download `AstersNoSaddle-v1.0.5.zip` from the [`release`](release/) directory.
3. Extract the archive.
4. Copy the extracted `AstersNoSaddle` directory into:

```text
Palworld/
└── Mods/
    └── NativeMods/
        └── UE4SS/
            └── Mods/
```

The final installation must be:

```text
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

## How it works

The mod operates entirely at runtime through UE4SS native C++ hooks.

When Palworld evaluates whether a Partner Skill is restricted by an item, the mod:

1. Detects the native `PalPartnerSkillParameterComponent:IsRestrictedByItems` query.
2. Confirms that the owning Pal has an unambiguous ride marker.
3. Removes the saddle restriction before Palworld evaluates the interaction state.
4. Allows the original game query and UI flow to continue normally.

When mounting requires a compatible ride-position fallback, the mod:

1. Locates the active Pal's `PalRideMarkerComponent`.
2. Preserves its original ride-position type.
3. Calls the game's native `UPalRiderComponent::Ride` function.
4. Allows Palworld to execute its normal authoritative ride and replication sequence.
5. Restores the original ride-position value after riding begins.

This preserves the game's own movement, animation, attack, networking, and dismount implementation.

## Compatibility and limitations

The mod supports Pals that expose a usable and unambiguous `UPalRideMarkerComponent`.

A Pal may not be supported when it:

- has no ride marker;
- has multiple ambiguous ride markers;
- uses a completely custom riding implementation;
- is affected by another mod that replaces the same Partner Skill or riding behavior.

Future Palworld or UE4SS updates may require a new mod build.

## Logging

A normal v1.0.5 startup produces only:

```text
[AstersNoSaddle] DLL constructed. Version 1.0.5.
[AstersNoSaddle] Unreal initialized.
[AstersNoSaddle] Ready. Partner restrictions will be removed when evaluated.
```

Additional messages are reserved for unexpected layouts, unresolved objects, incompatible reflected functions, or other safety checks.

The log is normally located at:

```text
Palworld/Mods/NativeMods/UE4SS/UE4SS.log
```

## Building

The checked-in source includes:

- `src/dllmain.cpp`
- `src/CMakeLists.txt`

The current project is built as a UE4SS native C++ mod using:

- CMake
- LLVM/Clang
- the Microsoft Windows SDK through xwin for Linux cross-compilation
- the RE-UE4SS C++ mod interface

The resulting module must be named:

```text
main.dll
```

## Repository layout

```text
AstersNoSaddle/
├── README.md
├── CHANGELOG.md
├── LICENSE
├── src/
│   ├── CMakeLists.txt
│   └── dllmain.cpp
└── release/
    ├── AstersNoSaddle-v1.0.2.zip
    ├── AstersNoSaddle-v1.0.5.zip
    └── SHA256SUMS
```

Compiled DLLs are distributed inside release ZIP archives rather than as loose repository files.

## Support and community

For support, testing discussion, and Null Prism development updates:

[![Join the Null Prism Discord](https://img.shields.io/badge/Join%20the%20Null%20Prism%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/CC4psCt2y5)

Bug reports should include:

- Palworld version
- UE4SS version
- whether the mod was installed manually, from Nexus, or from Workshop
- relevant `[AstersNoSaddle]` lines from `UE4SS.log`

## License

Aster's No Saddle is distributed under the MIT License. See [LICENSE](LICENSE).

## Credits

Created by **Aster** under the [NullPrism](https://github.com/NullPrism) project.

Special thanks to the UE4SS developers and the Palworld modding community.
