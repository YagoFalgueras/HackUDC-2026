# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `README.md` rewritten in English: purpose, features, install, usage,
  examples, configuration, compatibility, troubleshooting, and support.
- `CONTRIBUTING.md`: how to set up the dev environment, coding standards,
  commit conventions, PR process, and review expectations. Includes an honest
  note about response times given the team's academic schedule.
- `CODE_OF_CONDUCT.md`: Contributor Covenant 2.1 with enforcement contacts
  and explicit response-time commitments.
- `SECURITY.md`: vulnerability reporting instructions and disclosure timeline,
  aligned with EU Cyber Resilience Act (Regulation (EU) 2024/2847) Article 14
  reporting obligations (effective September 2026).
- `docs/COMPONENTS_LICENSE.md`: analysis of every dependency's license and a
  detailed justification of why GPL v2 (and not v3) is the only legally
  correct choice for this project.
- `LICENSE` and `LICENSES/GPL-2.0-only.txt`: canonical GPL-2.0-only text for
  GitHub detection and REUSE compliance.
- `.reuse/dep5`: machine-readable copyright and license declarations for all
  files, including Chocolate Doom and third-party assets.
- SPDX license headers added to all project source files.

### Changed

- Project renamed from "DOOM Satellite" to **BFG-I**.

---

## [0.1.0] - 2026-02-28

Initial release developed at HackUDC 2026.

### Added

- **End-to-end video pipeline.** Chocolate Doom runs headless on the satellite,
  captures its framebuffer, encodes it to H.264 (QCIF 176×144, 20 FPS,
  ~192 kbps), and streams it to the ground station over RTP/UDP. The ground
  station decodes the stream and displays it in a 3× upscaled SDL2 window.

- **Lock-free ring buffer** between the DOOM game thread (35 Hz) and the
  encoder thread (20 FPS), so neither thread blocks the other.

- **RTP transport with FU-A fragmentation.** Video is packetised following
  RFC 3550 and RFC 3984. NAL units larger than the MTU are split into
  Fragmentation Units; the receiver reassembles them before decoding.

- **Bidirectional control.** The ground station captures keyboard input and
  sends it back to the satellite over a separate UDP uplink. The full
  Chocolate Doom key mapping is supported, including movement, weapons 1–7,
  strafe, run, and menu navigation.

- **Bandwidth reporting.** Both processes print total bytes sent and received
  per channel when they exit (Ctrl+C or SIGTERM).

- **Cross-compilation support.** The Makefile accepts a `CROSS=` prefix (e.g.,
  `CROSS=aarch64-linux-gnu-`) to build the satellite binary for ARM64.

### Fixed

- Keyboard events were being injected twice into the DOOM engine due to the
  SDL2 event queue being drained in two separate places.
- The DOOM engine failed to start when launched from `satellite/main.c` due
  to a missing `D_DoomMain()` linkage and incorrect link order.
- Compilation errors in `satellite/doom/src/i_video.c` after the ring buffer
  replaced the direct UDP send path.

[Unreleased]: https://github.com/hackudc/BFG-I/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/hackudc/BFG-I/releases/tag/v0.1.0
