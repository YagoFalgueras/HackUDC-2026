# Security Policy

This document describes the security policy for BFG-I, including how to report
vulnerabilities, what to expect after a report, and our obligations under the
**EU Cyber Resilience Act (CRA), Regulation (EU) 2024/2847**.

---

## CRA applicability

The Cyber Resilience Act entered into force on **10 December 2024**. Its
reporting obligations (Article 14) apply from **11 September 2026**; full
compliance is required from **11 December 2027**.

BFG-I is free, open-source software developed non-commercially by university
students. Under the CRA's open-source provisions, **non-monetized free and
open-source software is generally exempt** from the CRA's conformity
assessment and manufacturer obligations. However, we voluntarily align this
policy with CRA best practices — including coordinated vulnerability disclosure,
SBOM availability, and Article 14 reporting timelines — because we consider
them the right baseline regardless of legal obligation.

If BFG-I is integrated into a commercial product or distributed as part of a
monetized offering, **the integrating party becomes the manufacturer under the
CRA** and assumes the full set of obligations described in Articles 13 and 14.

---

## Supported versions

| Version / branch | Security fixes |
|-----------------|---------------|
| `main` | Yes — latest commit is always the supported version |
| Tagged releases | Best-effort backport for critical issues |
| Forks | Not our responsibility |

We do not maintain multiple release branches. Security fixes are applied to
`main` and a new tag is cut promptly.

---

## Scope

The following are **in scope** for this security policy:

- `satellite/` — encoder, downlink, uplink, ring buffer, and DOOM integration
- `ground/` — receiver, display, input handling
- `common/include/protocol.h` — the shared wire protocol
- The UDP communication channel between satellite and ground station

The following are **out of scope**:

- **Chocolate DOOM** (`satellite/doom/`) — report vulnerabilities upstream at
  [https://github.com/chocolate-doom/chocolate-doom/security](https://github.com/chocolate-doom/chocolate-doom/security)
- **FFmpeg**, **libx264**, **SDL2**, or any other system library — report to
  their respective maintainers
- Vulnerabilities that require physical access to the machine running either
  binary
- Denial-of-service via malformed UDP packets (accepted risk — the transport is
  UDP and the threat model assumes a trusted local or LAN network)
- The game data file (`freedoom1.wad`) — out of scope by definition

---

## Reporting a vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Report privately using one of the following channels, in order of preference:

1. **GitHub private vulnerability reporting** — go to the repository's
   _Security_ tab and select _Report a vulnerability_. This creates a
   confidential advisory visible only to maintainers.
2. **Email** — if GitHub private reporting is unavailable, contact the
   maintainers directly. Email addresses are available on the contributors'
   GitHub profiles.

### What to include in your report

Please provide as much of the following as possible:

- A clear description of the vulnerability and its potential impact.
- The affected component and file(s) (e.g., `ground/receiver.c`, RTP
  reassembly path).
- Steps to reproduce, ideally as a minimal test case or proof-of-concept.
  A working exploit is not required; a clear description is sufficient.
- The environment in which you reproduced the issue (OS, compiler version,
  library versions).
- Whether you believe the vulnerability is being actively exploited in the
  wild (this affects our CRA reporting obligations — see below).
- Your preferred disclosure timeline, if any.

---

## What happens after you report

We will handle your report according to the following timeline, aligned with
the CRA Article 14 deadlines:

| Step | Our commitment |
|------|---------------|
| **Acknowledgement** | Within **72 hours** of receipt |
| **Initial assessment** | Within **7 days** — we confirm scope, severity, and whether the issue is reproducible |
| **Status update** | Within **14 days** — we share our remediation plan or a fix |
| **Fix released** | Target **30 days** for critical issues; **90 days** for moderate/low |
| **Public disclosure** | Coordinated with the reporter — see below |

We are students with academic obligations (see [CONTRIBUTING.md](CONTRIBUTING.md)).
These timelines represent our firm commitment for security issues, which we
treat as higher priority than regular contributions. If we cannot meet a
deadline, we will communicate proactively.

---

## Coordinated Vulnerability Disclosure (CVD)

We follow a **coordinated disclosure** model:

1. The reporter and maintainers agree on a disclosure date (default: **90 days**
   from the initial report, or when a fix is available, whichever comes first).
2. We prepare a fix and a GitHub Security Advisory (GHSA) draft.
3. On the agreed date, we publish the advisory, release a patched version, and
   request a CVE identifier via GitHub's CVE numbering authority integration.
4. The reporter is credited in the advisory unless they prefer to remain
   anonymous.

If a vulnerability is being **actively exploited in the wild** at the time of
the report, we will move immediately to emergency disclosure without waiting
for the standard 90-day window.

We ask reporters not to publish details of unpatched vulnerabilities before
the agreed disclosure date. In return, we commit to the timelines above and
will never request an extension beyond 90 days without the reporter's consent.

---

## ENISA / CSIRT reporting (CRA Article 14)

CRA Article 14 reporting obligations apply from **11 September 2026** and are
triggered exclusively by **actively exploited vulnerabilities** — not by every
reported issue.

If we become aware that a vulnerability in BFG-I is being actively exploited
in the wild, we will:

1. Issue an **early warning** to the national CSIRT (CCN-CERT for Spain) and
   ENISA via the CRA Single Reporting Platform within **24 hours**.
2. Submit a **formal notification** within **72 hours**, including impact
   assessment and initial mitigation measures.
3. Submit a **final report** within **14 days** of a corrective measure being
   available.

Reporters who believe an issue is being actively exploited should flag this
explicitly in their report — it changes our response priority and our
regulatory obligations.

---

## Software Bill of Materials (SBOM)

CRA Annex I, Part II requires manufacturers to maintain an SBOM for products
with digital elements. BFG-I provides licensing and component metadata in
machine-readable form via:

- [`.reuse/dep5`](.reuse/dep5) — REUSE-compliant bulk copyright and license
  declarations for all components, including Chocolate DOOM and third-party
  libraries, in Debian DEP-5 format.
- [`LICENSES/GPL-2.0-only.txt`](LICENSES/GPL-2.0-only.txt) — canonical license
  text for the primary license.
- [`docs/COMPONENTS_LICENSE.md`](docs/COMPONENTS_LICENSE.md) — human-readable
  analysis of all component licenses and their compatibility.

A full SPDX SBOM (`.spdx.json`) is planned for a future release.

---

## Security design notes

These are known constraints in the current design that reporters should be
aware of before filing:

- **No transport encryption.** The RTP/UDP stream between satellite and ground
  is unencrypted and unauthenticated. This is intentional: the threat model
  assumes a trusted point-to-point or LAN link. Do not expose these ports to
  the public internet.
- **No input authentication.** The uplink UDP packets carry no authentication
  tag. An attacker with network access can inject arbitrary key events into the
  satellite. This is a known and accepted limitation for the current scope.
- **Fixed SSRC.** The RTP SSRC is hardcoded (`0x12345678`). This does not
  introduce a security vulnerability in the current deployment model.
- **Hardcoded WAD path.** The satellite binary has the IWAD path compiled in.
  This is a usability limitation, not a security boundary.

---

## Attribution

We are grateful to security researchers who take the time to responsibly
disclose vulnerabilities in BFG-I. Confirmed vulnerability reporters will be
credited in the corresponding GitHub Security Advisory unless they request
anonymity.

---

*This policy is reviewed whenever a vulnerability is processed or when
relevant regulatory requirements change. Last reviewed: February 2026.*
