# Governance

This document describes how BFG-I is governed: who makes decisions, how those
decisions are made, and what is expected of the people who maintain the project.

---

## Project maintainers

BFG-I was created by three university students at HackUDC 2026. They are the
sole maintainers of the project and hold equal authority over it. There is no
hierarchy among them.

| Maintainer | GitHub | Email |
|------------|--------|-------|
| Carlos Cao | CarlosCaoLopez | carlos.cao@rai.usc.es |
| Yago Falgueras | YagoFalgueras | yago.falgueras@rai.usc.es |
| Ignacio Garbayo | igarbayo | ignacio.garbayo@rai.usc.es |

All three are active students with full academic schedules. This shapes
everything from response times to the pace of development. See
[CONTRIBUTING.md](CONTRIBUTING.md) for what that means in practice.

---

## Roles

### Maintainer

The three founders are the maintainers. Collectively they:

- Review and merge pull requests.
- Triage issues and vulnerability reports.
- Make decisions about project direction, releases, and dependencies.
- Enforce the [Code of Conduct](CODE_OF_CONDUCT.md).
- Cut releases and maintain the changelog.

There is no distinction between "lead maintainer" and "maintainer". All three
share identical rights and responsibilities.

### Contributor

Anyone who opens an issue or submits a pull request is a contributor.
Contributors do not have commit access. See [CONTRIBUTING.md](CONTRIBUTING.md)
for the full process.

---

## Decision-making

### Day-to-day decisions

Small decisions — bug fixes, documentation improvements, minor refactors,
dependency updates — can be decided and merged by any single maintainer
without consulting the others, provided the change is clearly within the
existing scope of the project.

### Significant decisions

The following require explicit agreement from at least two of the three
maintainers before being merged or acted upon:

- New features or capabilities that meaningfully change scope.
- Changes to the wire protocol (`protocol.h`) or the RTP transport.
- New runtime dependencies or changes to the build system.
- Changes to this document, `CONTRIBUTING.md`, `SECURITY.md`, or
  `CODE_OF_CONDUCT.md`.
- Any action taken in response to a Code of Conduct report.
- Releasing a new version.

### Disagreements

If two maintainers disagree, they discuss openly (in an issue, PR comment, or
direct conversation) until one changes their position or a compromise is found.
If all three disagree and no majority forms, the status quo is preserved — the
proposed change is not made until consensus is reached. We do not use formal
votes or tie-breaking mechanisms; we talk it out.

### Absence

If a maintainer is unreachable (exam period, illness, life), the remaining two
can make significant decisions together. No single maintainer can make
significant decisions alone.

---

## Maintainer expectations

Being a maintainer of BFG-I means:

- **Reviewing PRs in good faith.** Feedback should be specific, actionable, and
  respectful. We explain our reasoning; we do not just say "no".
- **Responding to security reports promptly.** Security issues take priority
  over everything else. See [SECURITY.md](SECURITY.md) for the timelines we
  commit to.
- **Being honest about availability.** If you cannot review something for weeks,
  say so early. Contributors deserve to know where their PR stands.
- **Not burning out.** This is a student project. Sustainability matters more
  than velocity. It is acceptable to deprioritise BFG-I during exams or
  difficult periods without explanation.
- **Keeping each other informed.** If you merge something significant, let the
  others know.

---

## Becoming a maintainer

There is currently no formal process for adding new maintainers. If the project
grows to the point where three people are not enough, we will define one then.
Any expansion of the maintainer group requires unanimous agreement from all
current maintainers.

---

## Point of contact

For anything that should not be discussed publicly — Code of Conduct reports,
security issues, or sensitive governance matters — reach out to the maintainers
directly by email:

- **Carlos Cao** — carlos.cao@rai.usc.es
- **Yago Falgueras** — yago.falgueras@rai.usc.es
- **Ignacio Garbayo** — ignacio.garbayo@rai.usc.es

For security vulnerabilities specifically, follow the process described in
[SECURITY.md](SECURITY.md). For everything else, a GitHub issue is preferred.

---

## Changes to this document

Amendments to this governance document require agreement from at least two
maintainers and must be recorded in [CHANGELOG.md](CHANGELOG.md).
