# Component Licenses and Justification for the GPL v2 License

## Why this project is distributed under GNU GPL v2

BFG-I is a project that integrates multiple third-party software components.
The license choice is not arbitrary: it is **imposed by the most restrictive
dependencies** in the system, specifically by **Chocolate Doom** and **libx264**, both
distributed under the GNU General Public License version 2.

GPL v2 is a *strong copyleft* license: any derived work or work that links against
GPL v2 code must also be distributed under GPL v2. Since the core of this project
is precisely those two dependencies, adopting GPL v2 is the only legally sound option.

---

## Why GPL v2 and not GPL v3

Both Chocolate Doom and libx264 are licensed under **GPL v2 only** — not "v2 or
later". This distinction is critical.

GPL v3 (released in 2007) introduced several new clauses not present in v2:

- **Anti-tivoization clause (section 6):** prevents distributing GPL software on
  hardware that cryptographically blocks the user from running modified versions
  (as TiVo did). This is relevant in embedded and satellite contexts.
- **Patent retaliation clause (section 11):** terminates the license for any party
  that initiates patent litigation against GPL v3 software.
- **Compatibility with Apache 2.0:** GPL v3 is compatible with Apache 2.0, whereas
  GPL v2 is not.

The FSF (Free Software Foundation) considers GPL v2 and GPL v3 to be
**mutually incompatible**: you cannot combine code that is "GPL v2 only" with code
that is "GPL v3 only" in a single binary. Since Chocolate Doom explicitly states
GPL v2 (without the "or later" clause), and libx264 does the same, **adopting GPL v3
would be legally impossible** — it would require relicensing upstream code we do not
own.

Furthermore, all other dependencies in this project (SDL2, FFmpeg, libfluidsynth)
are licensed under LGPL v2.1, which is also compatible with GPL v2 but **not
automatically compatible with GPL v3** unless explicitly stated. Using GPL v3 would
therefore create additional compatibility risks with no practical benefit.

In short: **GPL v2 is the only version legally permitted by our dependency chain.**
GPL v3 is not an option, not a preference.

---

## Components and their licenses

### 1. Chocolate Doom — GPL v2 (core component)

| Field | Detail |
|-------|--------|
| Path | `satellite/doom/` |
| Version | 3.1.1 |
| License | GNU General Public License v2 |
| License file | `satellite/doom/COPYING.md` |
| Main authors | Simon Howard, James Haley, Samuel Villarreal, Fabian Greffrath, and others |

Chocolate Doom is the game engine running on the satellite side. The upstream project
explicitly states in its `PHILOSOPHY.md`:

> *"Chocolate Doom is and will always remain Free Software.
> It will never include code that is not compatible with the GNU GPL."*

BFG-I modifies Chocolate Doom to redirect the raw framebuffer (320×200, 8-bit) to a
UDP socket instead of rendering it locally on screen. Being a *derived work* of
Chocolate Doom, the GPL v2 propagates mandatorily to the entire satellite binary
(`doom-satellite`).

---

### 2. libx264 — GPL v2 (core component)

| Field | Detail |
|-------|--------|
| Library | `libx264` |
| Usage | H.264 encoding in the satellite component |
| Compiler flag | `-lx264` |
| License | GNU General Public License v2 or later |

libx264 is the reference implementation of the H.264/AVC codec developed by
VideoLAN. Its license is GPL v2, which means **any binary that links against libx264
must also be distributed under GPL v2**. This restriction applies to both static and
dynamic linking when the resulting binary is distributed.

In BFG-I, libx264 is used to compress game frames before transmitting them over the
downlink channel, optimizing the bandwidth of the simulated communication link.

---

### 3. FFmpeg (libavcodec, libavutil, libswscale) — LGPL v2.1 / GPL v2

| Field | Detail |
|-------|--------|
| Libraries | `libavcodec`, `libavutil`, `libswscale` |
| Usage | H.264 decoding and color conversion at the ground station |
| Compiler flags | `-lavcodec -lavutil -lswscale` |
| Base license | LGPL v2.1 |
| License with GPL encoders | **GPL v2** (if the build includes x264, x265, etc.) |

FFmpeg is the most widely used multimedia framework on Linux. Its core libraries are
licensed under LGPL v2.1 by default, which would allow dynamic linking without
triggering GPL obligations on the consuming project. However, **if the system
installation of FFmpeg was compiled with `--enable-gpl`** (as is common in most Linux
distributions to enable native H.264, HEVC, and other codec support), the entire
FFmpeg build becomes GPL v2.

Since BFG-I is already subject to GPL v2 through Chocolate Doom and libx264, the GPL
variant of FFmpeg is perfectly compatible and introduces no additional conflict.

---

### 4. SDL2 — LGPL v2.1

| Field | Detail |
|-------|--------|
| Library | `libSDL2` |
| Usage | Video rendering, keyboard input (both components) |
| Compiler flags | `-lSDL2` |
| License | GNU Lesser General Public License v2.1 |

SDL2 (Simple DirectMedia Layer) uses LGPL v2.1, a *weak copyleft* license that allows
use in GPL projects without conflict. LGPL v2.1 is explicitly compatible with GPL v2.

---

### 5. SDL2_mixer — LGPL v2.1

| Field | Detail |
|-------|--------|
| Library | `libSDL2_mixer` |
| Usage | In-game audio on the satellite side (inherited from Chocolate Doom) |
| Compiler flag | `-lSDL2_mixer` |
| License | GNU Lesser General Public License v2.1 |

SDL audio mixing library. Fully compatible with GPL v2.

---

### 6. SDL2_net — LGPL v2.1

| Field | Detail |
|-------|--------|
| Library | `libSDL2_net` |
| Usage | Networking on the satellite side (inherited from Chocolate Doom) |
| Compiler flag | `-lSDL2_net` |
| License | GNU Lesser General Public License v2.1 |

SDL networking library. Fully compatible with GPL v2.

---

### 7. libpng — PNG License (BSD-style)

| Field | Detail |
|-------|--------|
| Library | `libpng` |
| Usage | Loading graphical assets on the satellite side |
| Compiler flag | `-lpng` |
| License | PNG Reference Library License (permissive, similar to BSD-2-Clause) |

Permissive license with no copyleft clauses. Compatible with any license, including
GPL v2.

---

### 8. libsamplerate — BSD 2-Clause

| Field | Detail |
|-------|--------|
| Library | `libsamplerate` |
| Usage | Audio resampling on the satellite side |
| Compiler flag | `-lsamplerate` |
| License | BSD 2-Clause ("Simplified BSD") |

Permissive license with no copyleft restrictions. Fully compatible with GPL v2.

---

### 9. libfluidsynth — LGPL v2.1

| Field | Detail |
|-------|--------|
| Library | `libfluidsynth` |
| Usage | MIDI synthesis on the satellite side (game music) |
| Compiler flag | `-lfluidsynth` |
| License | GNU Lesser General Public License v2.1 |

SoundFont-based audio synthesizer. Fully compatible with GPL v2.

---

### 10. quickcheck (Git submodule) — GPL v2

| Field | Detail |
|-------|--------|
| Path | `quickcheck/` |
| URL | https://github.com/chocolate-doom/quickcheck |
| License | GPL v2 (Chocolate Doom team project) |

Testing tool from the Chocolate Doom ecosystem. Not part of the final binary but
licensed under GPL v2, consistent with the rest of the project.

---

### 11. Third-party assets in Chocolate Doom

| Asset | Origin | Original license | Status |
|-------|--------|-----------------|--------|
| `textscreen/fonts/normal.png` | DOSBox Project | GPL v2 | Compatible |
| `textscreen/fonts/small.png` | Tom Fine (Atari-Small font, 1999) | Permissive (free commercial and non-commercial use) | Compatible with GPL v2 |
| `textscreen/fonts/large.png` | Simon Howard, based on normal.png | GPL v2 | Compatible |
| Chocolate Doom icon | Chris Metcalf | GPL (with explicit permission from the author) | Compatible |

---

## License compatibility tree

```
GPL v2 (full project)
├── Chocolate Doom ──────────────────── GPL v2        ✓ (source of the constraint)
├── libx264 ─────────────────────────── GPL v2        ✓ (source of the constraint)
├── FFmpeg (build --enable-gpl) ─────── GPL v2        ✓
├── SDL2 ────────────────────────────── LGPL v2.1  →  compatible with GPL v2  ✓
├── SDL2_mixer ──────────────────────── LGPL v2.1  →  compatible with GPL v2  ✓
├── SDL2_net ────────────────────────── LGPL v2.1  →  compatible with GPL v2  ✓
├── libpng ──────────────────────────── PNG License →  compatible  ✓
├── libsamplerate ───────────────────── BSD 2-Clause → compatible  ✓
├── libfluidsynth ───────────────────── LGPL v2.1  →  compatible with GPL v2  ✓
└── quickcheck ──────────────────────── GPL v2        ✓
```

No dependency carries a license incompatible with GPL v2. There are no proprietary
components or "non-commercial only" licenses that would create a conflict.

---

## Conclusion

The adoption of **GNU GPL v2** for BFG-I is not an optional choice but a **direct
legal consequence** of integrating:

1. **Chocolate Doom** (GPL v2 only): the game engine on which the satellite component
   is built. By modifying and redistributing it, the derived code mandatorily inherits
   GPL v2.

2. **libx264** (GPL v2 only): the H.264 encoding library used to compress the video
   stream. Linking it into the final binary carries the GPL v2 obligation.

Adopting **GPL v3** would also be illegal: both Chocolate Doom and libx264 are
licensed under "GPL v2 only" (not "v2 or later"), and the FSF considers GPL v2 and
GPL v3 to be mutually incompatible. No other version of the GPL is legally available.

The remaining dependencies (SDL2, FFmpeg, libpng, libsamplerate, libfluidsynth) carry
LGPL or permissive licenses that are fully compatible with GPL v2 and add no further
restrictions.

Adopting any other license (MIT, Apache 2.0, proprietary) would be **illegal**, as it
would amount to redistributing GPL v2 code under incompatible terms. GPL v2 is
therefore the only legally correct license for this project.
