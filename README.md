# BFG-I

Real-time streaming of a DOOM game session from a satellite to a ground station over UDP, with bidirectional input and H.264-compressed video.

```
┌────────────────────────────────────────────────────────────────┐
│                    SATÉLITE (ARM Linux SoC)                    │
│                                                                │
│  ┌──────────────┐    framebuffer   ┌──────────────────────┐    │
│  │  DOOM Engine │ ──────────────▶  │  H.264 SW Encoder    │    │
│  │  (Headless)  │   (shared mem)   │  (libx264 ultrafast) │    │
│  │  Hilo 1      │                  │  Hilo 2              │    │
│  └──────▲───────┘                  └──────────┬───────────┘    │
│         │ consume                             │ produce        │
│         │ input                               │ NAL units      │
│         │                                     ▼                │
│  ┌──────┴───────┐                  ┌──────────────────────┐    │
│  │  Input       │                  │  Network / RTP       │    │
│  │  Receiver    │                  │  Transmitter         │    │
│  │  (UDP Uplink)│                  │  (UDP Downlink)      │    │
│  └──────▲───────┘                  └──────────┬───────────┘    │
│         │                                     │                │
└─────────┼─────────────────────────────────────┼────────────────┘
          │            ENLACE SATELITAL         │
          │◀──── Uplink (comandos) ─────────────│
          │───── Downlink (vídeo H.264) ───────▶│
          │                                     │
┌─────────┼─────────────────────────────────────┼────────────────┐
│         │          ESTACIÓN TERRESTRE         ▼                │
│  ┌──────┴───────┐                  ┌──────────────────────┐    │
│  │  Input       │                  │  RTP Receiver &      │    │
│  │  Capture &   │                  │  H.264 Decoder       │    │
│  │  Transmitter │                  │ (FFmpeg / libavcodec)│    │
│  └──────────────┘                  └──────────┬───────────┘    │
│                                               ▼                │
│                                    ┌──────────────────────┐    │
│                                    │  SDL2 Display        │    │
│                                    │  Window              │    │
│                                    └──────────────────────┘    │
└────────────────────────────────────────────────────────────────┘
```

---

## Purpose

Standard satellite communication links impose severe constraints: high latency (20–40 ms one-way for LEO orbits), limited bandwidth, and non-negligible packet loss. Running an interactive application — let alone a real-time game — under these conditions requires careful engineering at every layer.

BFG-I solves this by demonstrating a full-stack approach to the problem:

- **Headless game execution on the satellite.** Chocolate DOOM runs without a display, writing raw frames to a lock-free ring buffer instead of rendering to screen.
- **Aggressive compression before transmission.** libx264 encodes frames to H.264 at QCIF resolution (176×144) and 20 FPS using the `ultrafast`/`zerolatency` profile, keeping video bitrate in the 128–256 kbps range — well within a realistic downlink budget.
- **Standard transport protocol.** Video is packetized as RTP over UDP with FU-A fragmentation (RFC 3984), enabling interoperability with standard media tools and graceful handling of packet loss without retransmission stalls.
- **Bidirectional control loop.** Player keyboard input is captured at the ground station, serialized into compact 8-byte packets, and sent back to the satellite via a separate UDP uplink, closing the control loop.

The result is a system that can play DOOM end-to-end across a link that simulates real LEO satellite conditions.

---

## Features

- **H.264 video downlink** — x264 encoding at QCIF (176×144), 20 FPS, ~192 kbps. `ultrafast` preset and `zerolatency` tune minimize encoder delay to ~10–20 ms.
- **RTP/UDP transport with FU-A fragmentation** — NAL units larger than the MTU (~1460 bytes) are split into Fragmentation Units (RFC 3984). The marker bit signals frame boundaries. No retransmission: packet loss results in frame corruption, not stall.
- **Lock-free ring buffer** — Atomic slot states (EMPTY → WRITING → READY → READING) decouple the DOOM game thread (35 Hz) from the encoder thread (20 FPS) with no mutex in the critical path.
- **Headless DOOM via doomgeneric** — Chocolate DOOM is adapted through a thin `doomgeneric_satellite.c` shim. `DG_DrawFrame()` pushes frames to the ring buffer; `DG_GetKey()` drains the uplink input queue.
- **FFmpeg H.264 decoder at ground** — libavcodec decodes the incoming H.264 stream; libswscale converts YUV420p to RGB24 for SDL2.
- **3× upscaled SDL2 display** — The 176×144 decoded frame is displayed in a 528×432 window with nearest-neighbor scaling (SDL_HINT_RENDER_SCALE_QUALITY=0) and vsync.
- **Compact input uplink** — 8-byte packets (`uint16_t bitfield + uint32_t timestamp + uint16_t seq`) sent only on state change. Uplink traffic stays below 200 bytes/sec.
- **Bandwidth reporting** — On exit (SIGINT/SIGTERM), both processes print total bytes sent/received per channel.
- **Cross-compilation support** — The Makefile accepts a `CROSS=` prefix (e.g., `aarch64-linux-gnu-`) for building the satellite binary on ARM64 targets.
- **Satellite link simulation** — Compatible with Linux `tc netem` for injecting delay and packet loss on loopback or real interfaces.

---

## Install

### Dependencies

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

### Build

Build both binaries from the repository root:

```bash
make
```

Or build each component separately:

```bash
make satellite   # produces ./doom-satellite
make ground      # produces ./doom-ground
```

The DOOM engine itself must be compiled separately before linking the satellite binary:

```bash
cd satellite/doom
./autogen.sh
make
cd ../..
make satellite
```

**Debug build** (disables optimizations, enables debug symbols):

```bash
make DEBUG=1
```

**Cross-compile for ARM64:**

```bash
make satellite CROSS=aarch64-linux-gnu-
```

---

## Usage

Always start the ground station before the satellite, so the receiver socket is ready when the first packets arrive.

**Terminal 1 — Ground station:**
```bash
./doom-ground
```

**Terminal 2 — Satellite:**
```bash
./doom-satellite
```

The satellite requires `freedoom1.wad` to be present in the repository root. The path is hardcoded in the binary. [FreeDOOM](https://freedoom.github.io/) is a free, GPL-compatible replacement for the original DOOM IWAD.

### Controls (at the ground station)

Controls replicate the original Chocolate DOOM key bindings.

| Key | Action |
|-----|--------|
| Arrow up / down | Move forward / backward |
| Arrow left / right | Turn left / right |
| Ctrl | Fire |
| Space | Use / open door |
| Shift | Run (hold with movement) |
| Alt + left / right | Strafe left / right |
| , (comma) | Strafe left |
| . (period) | Strafe right |
| 1–7 | Select weapon |
| Enter | Confirm menu selection |
| ESC | Back in menu |
| Y | Confirm quit when prompted |
| Ctrl+C | Terminate process |

---

## Examples

### Local test (loopback)

```bash
# Terminal 1
./doom-ground 127.0.0.1 5001 5000

# Terminal 2
./doom-satellite 127.0.0.1 5000 5001
```

### Remote test (two machines on the same LAN)

```bash
# On the ground machine (e.g., 192.168.1.100)
./doom-ground 192.168.1.50 5001 5000

# On the satellite machine (e.g., 192.168.1.50)
./doom-satellite 192.168.1.100 5000 5001
```

### Simulating a LEO satellite link with tc netem

Add 15 ms one-way delay (≈30 ms RTT) and 1% packet loss on loopback:

```bash
sudo tc qdisc add dev lo root netem delay 15ms loss 1%
```

Run both processes normally. To remove the simulation:

```bash
sudo tc qdisc del dev lo root
```

For a real network interface (e.g., `eth0`):

```bash
sudo tc qdisc add dev eth0 root netem delay 15ms loss 1%
sudo tc qdisc del dev eth0 root
```

---

## Configuration

Both binaries accept positional CLI arguments. All parameters have defaults for local testing.

### `doom-satellite`

```
./doom-satellite [GROUND_IP] [UPLINK_PORT] [DOWNLINK_PORT] [DOOM_ARGS...]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `GROUND_IP` | `127.0.0.1` | IP address of the ground station |
| `UPLINK_PORT` | `5000` | UDP port to receive input from ground |
| `DOWNLINK_PORT` | `5001` | UDP port to send video to ground |
| `DOOM_ARGS` | — | Any additional arguments forwarded to Chocolate DOOM (e.g., `-skill`) |

### `doom-ground`

```
./doom-ground [SATELLITE_IP] [VIDEO_PORT] [INPUT_PORT]
```

| Argument | Default | Description |
|----------|---------|-------------|
| `SATELLITE_IP` | `127.0.0.1` | IP address of the satellite |
| `VIDEO_PORT` | `5001` | UDP port to receive video from satellite |
| `INPUT_PORT` | `5000` | UDP port to send input to satellite |

### Encoder parameters (compile-time, `satellite/encoder.c`)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Resolution | 176×144 (QCIF) | Downsampled from DOOM's native 320×200 |
| Frame rate | 20 FPS | Encoder thread sleep: 50 ms |
| Bitrate | ~192 kbps | Configurable in encoder_init() |
| x264 preset | `ultrafast` | Minimum CPU, minimum latency |
| x264 tune | `zerolatency` | No lookahead, no frame buffering |
| H.264 profile | `baseline` | No B-frames |
| Keyframe interval | 60 frames | ≈3 s at 20 FPS; allows stream recovery |
| Threads | 1 | Deterministic, single-threaded encode |

### Network / RTP parameters (compile-time, `satellite/downlink.c`)

| Parameter | Value |
|-----------|-------|
| RTP payload type | 96 (dynamic) |
| RTP SSRC | `0x12345678` |
| RTP clock rate | 90 kHz |
| Timestamp increment | 4500 per frame (at 20 FPS) |
| MTU payload size | ~1460 bytes |
| Fragmentation | FU-A (RFC 3984) for NALs > MTU |

---

## Compatibility

### Operating system

Linux only. The following kernel features are used directly:

- `SO_REUSEADDR` / `SO_RCVBUF` for UDP sockets
- `clock_nanosleep(CLOCK_MONOTONIC)` for precise sleeping
- `pthread` with CPU affinity (`pthread_setaffinity_np`)
- `tc netem` (optional, for link simulation)

### Compilers

GCC is required. The Makefile forces `CC=gcc`. Clang is not tested.

### Architectures

| Architecture | Satellite | Ground | Notes |
|-------------|-----------|--------|-------|
| x86\_64 | Yes | Yes | Native target |
| ARM64 (AArch64) | Yes | Yes | Cross-compile with `CROSS=aarch64-linux-gnu-` |
| ARM32 | Untested | Untested | May work with appropriate cross-compiler |

### Library versions

| Library | Minimum tested | Notes |
|---------|---------------|-------|
| SDL2 | 2.0.14 | Required by Chocolate DOOM |
| SDL2\_mixer | 2.0.2 | Required by Chocolate DOOM |
| SDL2\_net | 2.0.0 | Required by Chocolate DOOM |
| libx264 | any stable | Must be GPL build |
| libavcodec (FFmpeg) | 4.x | `AV_CODEC_ID_H264` required |
| libswscale | 4.x | `SWS_BILINEAR` used for YUV→RGB |
| libfluidsynth | 2.2.0 | Optional; only needed if audio is enabled |

### IWAD files

BFG-I requires a DOOM IWAD. The following have been tested:

| File | Source | License |
|------|--------|---------|
| `freedoom1.wad` | [freedoom.github.io](https://freedoom.github.io/) | BSD-3-Clause |
| `doom.wad` | Commercial (id Software) | Proprietary |
| `doom2.wad` | Commercial (id Software) | Proprietary |

---

## Troubleshooting

**No video appears on the ground station.**
Start the ground station before the satellite. The receiver socket must be bound before the first UDP packets arrive. If the issue persists, verify that no firewall is blocking ports 5000–5001:
```bash
sudo ufw allow 5000:5001/udp
```

**"Failed to create socket" or "Address already in use".**
Another process is bound to the port. Find and stop it:
```bash
sudo lsof -i :5000
sudo lsof -i :5001
```
Or pick different ports via CLI arguments.

**Segmentation fault at startup on the satellite.**
`freedoom1.wad` is missing from the repository root. The path is hardcoded in the binary — place the file there before running.

**SDL window fails to open on the ground station.**
No display is available or `DISPLAY` is not set. Set it explicitly:
```bash
DISPLAY=:0 ./doom-ground
```

**High packet loss / corrupted frames at high latency.**
This is expected behavior for UDP over a lossy link. Keyframes (every 60 frames, ≈3 s) allow the decoder to recover. If corruption is too frequent, reduce the keyframe interval in `satellite/encoder.c` (`i_keyint_max`).

**Encoder crashes or produces no output.**
Verify that the installed libx264 is a GPL build (not a stub). On Debian/Ubuntu:
```bash
dpkg -l | grep libx264
x264 --version
```

**Decoder crashes on the ground station.**
Verify FFmpeg was compiled with H.264 support:
```bash
ffmpeg -codecs | grep h264
```
The line should show `D.V...` (decode) or `DEV...` (decode + encode).

**Game is unresponsive to input.**
The uplink path (ground → satellite) may be blocked. Check that `INPUT_PORT` (default 5000) is reachable from the ground machine to the satellite machine. Test with:
```bash
nc -u -z <SATELLITE_IP> 5000
```

**Bandwidth reporting shows 0 bytes on exit.**
The process was killed with `SIGKILL` instead of `SIGINT`/`SIGTERM`. Use Ctrl+C or `kill -SIGTERM <pid>` to trigger the signal handler and print stats.

---

## Support

This project was developed as part of [HackUDC 2026](https://hackudc.gpul.org/).

- **Bug reports and feature requests:** Open an issue at the repository on GitHub.
- **Documentation:** See the [`docs/`](docs/) folder:
  - [PROTOCOL_SPEC.md](docs/PROTOCOL_SPEC.md) — Binary protocol format
  - [ENCODING_GUIDE.md](docs/ENCODING_GUIDE.md) — H.264 parameter tuning
  - [LATENCY_ANALYSIS.md](docs/LATENCY_ANALYSIS.md) — End-to-end latency budget
  - [DEPLOYMENT.md](docs/DEPLOYMENT.md) — Hardware deployment guide
  - [INTERFACES.md](docs/INTERFACES.md) — Module interface specifications
  - [COMPONENTS_LICENSE.md](docs/COMPONENTS_LICENSE.md) — License analysis and GPL v2 justification
- **License:** GNU General Public License v2. See [satellite/doom/COPYING.md](satellite/doom/COPYING.md).
