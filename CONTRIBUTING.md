# Contributing to BFG-I

Thank you so much for taking the time to read this. It genuinely means a lot.
BFG-I is a small project born at a hackathon, and the fact that you are here
considering contributing to it is something we do not take for granted.

---

## A note on response times

We are university students with full academic schedules. We will do our best
to respond to issues and pull requests, but please set your expectations
accordingly: **reviews may take weeks, and during term time they may take
longer**. We plan to dedicate focused time to the project over the summer,
when academic pressure lifts. If your contribution sits open without a response
for a while, it is not being ignored — it is waiting for the right moment.

As a rough guide:

| Period | Expected response time |
|--------|----------------------|
| During term (Sep–Jan, Feb–Jun) | 2–6 weeks |
| Exam periods (Jan, Jun) | No guarantees |
| Summer (Jul–Sep) | 1–2 weeks |

We will always acknowledge receipt of a PR with a brief comment as soon as we
can, even if the full review takes longer.

---

## Development environment setup

### Prerequisites

**Satellite side:**
```bash
sudo apt-get install -y \
    libsdl2-dev libsdl2-mixer-dev libsdl2-net-dev \
    libx264-dev \
    libpng-dev libsamplerate-dev libfluidsynth-dev \
    build-essential pkg-config autoconf automake
```

**Ground side:**
```bash
sudo apt-get install -y \
    libsdl2-dev \
    libavcodec-dev libavutil-dev libswscale-dev \
    build-essential pkg-config
```

### Building

```bash
# 1. Clone the repository
git clone <repo-url>
cd HackUDC-2026

# 2. Build the DOOM engine (satellite only)
cd satellite/doom
./autogen.sh
make
cd ../..

# 3. Build both binaries
make

# 4. Build in debug mode (recommended for development)
make clean
make DEBUG=1
```

### Running locally

```bash
# Terminal 1 — ground station
./doom-ground

# Terminal 2 — satellite (requires freedoom1.wad in repo root)
./doom-satellite
```

### Recommended tools

- **GCC** (required — the Makefile forces `CC=gcc`)
- **GDB** for debugging: `gdb ./doom-satellite`
- **Valgrind** for memory errors: `valgrind --leak-check=full ./doom-ground`
- **Wireshark** with RTP dissector to inspect the video stream on the wire
- **tc netem** to simulate satellite link conditions (see the README)

---

## Coding standards

BFG-I is written in C99. All contributions must follow the conventions already
established in the codebase.

### Language and dialect

- **Standard:** C99 (`-std=c99`). No C11 or GNU extensions unless strictly necessary.
- **Compiler:** GCC. Code must compile cleanly with `-Wall -Wextra` and produce zero warnings.
- **Portability:** Linux only for now. POSIX APIs are fine; platform-specific Linux
  syscalls (`fcntl`, `clock_nanosleep`, CPU affinity) are acceptable and already in use.

### Formatting

- **Indentation:** 4 spaces. No tabs.
- **Braces:** Allman style — opening brace on its own line.
- **Line length:** Soft limit of 100 characters.
- **Naming:**
  - Functions and variables: `snake_case`
  - Macros and constants: `UPPER_SNAKE_CASE`
  - Module-private globals: `g_` prefix (e.g., `g_sock`, `g_bitfield`)
  - Module-private functions: `static`, no prefix required
- **Include guards:** All headers use `#ifndef / #define / #endif` guards.

### Structure

Each module (`.c` / `.h` pair) is self-contained and has a single clear
responsibility. Follow the same pattern:

```c
// SPDX-FileCopyrightText: 2026 BFG-I contributors (HackUDC 2026)
// SPDX-License-Identifier: GPL-2.0-only

/*
 * module.c — One-line description
 *
 * Responsibility:
 *   What this module does, in 2-3 sentences.
 */

#include "module.h"
/* ... other includes ... */

/* ------------------------------------------------------------------ */
/* Internal state                                                       */
/* ------------------------------------------------------------------ */

static int g_example = 0;

/* ------------------------------------------------------------------ */
/* Private helpers                                                      */
/* ------------------------------------------------------------------ */

static void helper(void) { ... }

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int module_init(void) { ... }
void module_shutdown(void) { ... }
```

### Thread safety

- State shared between threads must be protected. Use `pthread_mutex_t` for
  mutable shared state, `_Atomic` for simple counters and flags.
- The ring buffer uses lock-free atomic slot states — do not introduce a mutex
  in its critical path.
- Document thread-safety guarantees (or lack thereof) in the header comment of
  any function that touches shared state.

### Memory

- No dynamic allocation (`malloc`/`free`) in hot paths (encoding loop, RTP
  receive loop). Use static or stack allocation.
- If you must allocate dynamically, free it in the corresponding `_shutdown()`
  function and set the pointer to `NULL`.

### Error handling

- Functions that can fail return `int` (0 on success, -1 on error) or a
  negative errno value. Do not use exceptions or `exit()` from library code.
- Print meaningful error messages to `stderr` with the module name as prefix:
  `fprintf(stderr, "module_init: reason\n");`
- On fatal errors in `main()`, clean up all resources before returning.

### SPDX headers

Every new source file must begin with:

```c
// SPDX-FileCopyrightText: <year> <your name or "BFG-I contributors">
// SPDX-License-Identifier: GPL-2.0-only
```

---

## Tests

BFG-I does not yet have an automated test suite. This is a known gap and
contributions that add tests are especially welcome.

Until a test framework is in place, every PR must include **manual verification
steps** in the description: what you ran, on what environment, and what you
observed. Screenshots or terminal output are appreciated.

If you are adding a new feature, please include at minimum:

1. A loopback test (`./doom-ground` + `./doom-satellite` on the same machine).
2. A latency simulation test using `tc netem delay 15ms loss 1%`.
3. Confirmation that the build succeeds with `make DEBUG=1` and produces no new
   warnings under `-Wall -Wextra`.

---

## Commit conventions

We follow a lightweight version of [Conventional Commits](https://www.conventionalcommits.org/).

### Format

```
<type>(<scope>): <short description>

[optional body]

[optional footer]
```

### Types

| Type | When to use |
|------|-------------|
| `feat` | New feature or capability |
| `fix` | Bug fix |
| `refactor` | Code restructuring without behavior change |
| `perf` | Performance improvement |
| `docs` | Documentation only |
| `build` | Build system or dependency changes |
| `test` | Adding or fixing tests |
| `chore` | Maintenance (license headers, formatting, etc.) |

### Scopes

Use the affected component: `satellite`, `ground`, `encoder`, `downlink`,
`uplink`, `ringbuffer`, `display`, `receiver`, `input`, `protocol`, `build`,
`docs`.

### Examples

```
feat(encoder): reduce keyframe interval to 30 frames for faster recovery

fix(uplink): handle EAGAIN correctly on non-blocking recvfrom

perf(ringbuffer): replace mutex with atomic CAS in write path

docs(readme): update build instructions for ARM64 cross-compilation
```

### Rules

- Subject line: 72 characters max, imperative mood, no period at the end.
- Body: wrap at 100 characters, explain *why* not *what*.
- One logical change per commit. Do not bundle unrelated fixes.
- Do not amend commits that have already been pushed to a shared branch.

---

## Pull request process

1. **Fork** the repository and create a branch from `main`.
2. **Name your branch** descriptively: `feat/encoder-adaptive-bitrate`,
   `fix/uplink-eagain`, `docs/deployment-guide`.
3. **Keep the scope focused.** A PR that touches one module is easier to review
   than one that touches five. If you have multiple unrelated changes, open
   multiple PRs.
4. **Fill in the PR description:**
   - What problem does this solve?
   - What approach did you take, and why?
   - What alternatives did you consider?
   - Manual testing steps and results.
   - Any known limitations or follow-up work.
5. **Ensure the build is clean:** `make clean && make` must succeed with zero
   warnings. `make DEBUG=1` must also succeed.
6. **Check license compliance:** new files must have SPDX headers; new
   dependencies must be GPL-2.0-compatible (see
   [docs/COMPONENTS_LICENSE.md](docs/COMPONENTS_LICENSE.md)).
7. **Open the PR against `main`.**

We may ask for changes. Please do not take this personally — it is how we make
sure the codebase stays coherent. We will always explain our reasoning.

---

## Review expectations

Here is what you can expect from us during a review, and what we will expect
from you.

### What we will do

- Leave clear, specific comments explaining any requested changes.
- Distinguish between blocking issues (must fix before merge) and suggestions
  (take or leave).
- Approve and merge promptly once the PR meets the bar — we will not drag out
  a review indefinitely once things look good.
- Credit all contributors in the repository history.

### What we ask of you

- **Respond to review comments.** If you disagree with a comment, say so and
  explain why — disagreement is fine, silence is not.
- **Keep the branch up to date** with `main` if the review takes a long time
  and there are conflicts.
- **Do not push unrelated changes** to a PR branch while it is under review.
- **Be patient.** As noted above, we are students first. We will get there.

### Merge criteria

A PR will be merged when:

- [ ] It compiles cleanly with zero warnings on both `make` and `make DEBUG=1`.
- [ ] It has been manually tested and the results are described in the PR.
- [ ] All review comments are resolved (either addressed or explicitly rejected
      with justification).
- [ ] New files carry SPDX headers and any new dependencies are license-compatible.
- [ ] At least one maintainer has approved it.

---

## Questions and discussions

If you are unsure about anything before opening a PR, feel free to open a
**GitHub Issue** with the `question` label. We would rather answer a question
upfront than review a PR that went in the wrong direction.

Thank you again for your interest in BFG-I. We hope to hear from you — even if
it takes us a little while to write back.
