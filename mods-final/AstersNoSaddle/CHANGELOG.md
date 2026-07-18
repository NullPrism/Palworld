# Changelog

All notable changes to Aster's No Saddle are documented here.

## [1.0.5] - 2026-07-17

### Fixed

- Removed the initial `Locked` interaction prompt for compatible unsaddled Pals.
- Saddle restrictions are now removed immediately before Palworld evaluates `PalPartnerSkillParameterComponent:IsRestrictedByItems`.
- Preserved the existing first-interaction mount fallback.
- Preserved normal mounting, movement, mounted attacks, networking behavior, and dismounting.

### Changed

- Removed diagnostic restriction-query probe logging.
- Updated the normal startup status message.
- Added a clean Steam Workshop-compatible installation layout containing `enabled.txt`.

## [1.0.2] - 2026-07-16

### Changed

- Reduced normal runtime logging to three startup messages.
- Retained warning and compatibility messages for unexpected conditions.

## [1.0.1] - 2026-07-16

### Fixed

- Corrected the reflected `UPalRiderComponent::Ride` parameter buffer to exactly 10 bytes.
- Avoided native C++ structure padding that expanded the buffer to 16 bytes.

## [1.0.0] - 2026-07-16

### Added

- Initial native UE4SS C++ release.
- Saddle-restriction removal.
- Native ride invocation.
- Temporary ride-position compatibility handling.
- Original ride-position restoration after authoritative riding begins.
