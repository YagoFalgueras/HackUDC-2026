## Summary

<!-- What does this PR do and why? One paragraph is enough. -->

## Related issue

<!-- Link the issue this PR addresses, if any: "Fixes #123" or "Closes #123". -->

## Changes

<!-- Bullet list of what changed. Focus on the *what* and *why*, not the *how*. -->

-
-

## Testing

<!--
Describe how you verified the change works. At minimum:
  - loopback test (./doom-ground + ./doom-satellite on the same machine)
  - make clean && make produces zero warnings
  - make DEBUG=1 also succeeds

Add any additional test cases relevant to your change.
-->

- [ ] Loopback test: `./doom-ground` + `./doom-satellite` running together without errors.
- [ ] `make clean && make` — zero warnings under `-Wall -Wextra`.
- [ ] `make DEBUG=1` — also compiles cleanly.

**Test environment:**

```
OS:
GCC:
libx264:
FFmpeg (libavcodec):
SDL2:
```

## Checklist

### Code quality

- [ ] My changes follow the coding conventions in [CONTRIBUTING.md](../CONTRIBUTING.md).
- [ ] No new warnings introduced under `-Wall -Wextra`.
- [ ] No dynamic allocation in hot paths (encoding loop, RTP receive loop).
- [ ] Shared state accessed from multiple threads is protected by a mutex or atomic.

### License and copyright (FSF/REUSE compliance)

- [ ] Every new source file begins with the SPDX header:
      `// SPDX-FileCopyrightText: <year> <name>`
      `// SPDX-License-Identifier: GPL-2.0-only`
- [ ] No new dependency has been introduced with a license incompatible with
      GPL-2.0-only. (See [docs/COMPONENTS_LICENSE.md](../docs/COMPONENTS_LICENSE.md)
      for the compatibility analysis.)
- [ ] If a new dependency was added, `.reuse/dep5` has been updated accordingly.

### Developer Certificate of Origin

By submitting this pull request I certify, under the terms of the
[Developer Certificate of Origin 1.1](../DCO), that:

- [ ] The contribution was created in whole or in part by me, and I have the
      right to submit it under the **GNU General Public License, version 2
      only** (`GPL-2.0-only`); **or**
- [ ] The contribution is based on prior work covered by a compatible open
      source license and I have the right to submit it under GPL-2.0-only.

I understand that this project and my contribution are public, and that a
record of the contribution is maintained indefinitely.

<!-- Sign off with: git commit -s (adds "Signed-off-by: Name <email>") -->
