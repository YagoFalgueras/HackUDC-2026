# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `satellite/downlink.h`: declare `downlink_send_raw_frame(const void *buffer,
  size_t size)` to send a raw 8 bpp framebuffer over a single UDP datagram.
- `satellite/downlink.c`: implement the downlink module:
  - `downlink_init(dest_ip, dest_port)` — creates the UDP socket and configures
    the destination sockaddr_in; replaces the former `InitUDPSocket()` in
    `i_video.c`.
  - `downlink_send_raw_frame(buffer, size)` — sends the framebuffer via
    `sendto(2)`; lazily calls `downlink_init` if the socket is not yet open.
  - `downlink_send_nals(nals, nal_sizes, num_nals, timestamp)` — stub reserved
    for future RTP/H.264 fragmentation (FU-A).
  - `downlink_shutdown()` — closes the UDP socket and resets internal state.

- `ground/receiver.c`: implement the receiver module:
  - `receiver_init(listen_port)` — creates and binds a non-blocking UDP socket.
  - `receiver_poll()` — calls `recvfrom(2)` and converts the raw 8 bpp
    framebuffer to RGB24 using the DOOM palette; returns a pointer to the
    internal RGB buffer or `NULL` if no complete frame was available.
  - `receiver_shutdown()` — closes the UDP socket.
- `ground/display.c`: implement the display module:
  - `display_init()` — initialises SDL2, creates the window
    (320×200 × scale 3), renderer (vsync + accelerated) and streaming texture
    (RGB24); renders an initial black frame.
  - `display_present_frame(rgb_buffer)` — uploads the RGB24 buffer to the GPU
    texture and presents it via the SDL renderer.
  - `display_shutdown()` — destroys texture, renderer and window, then calls
    `SDL_Quit()`.

### Changed

- `ground/main.c`: replace monolithic implementation with thin orchestration
  layer. All UDP socket code moved to `receiver.c`; all SDL code moved to
  `display.c`. `main()` now only sets up signal handlers, calls module init
  functions, runs the event/receive/render loop, and calls shutdown in reverse
  order.
- `satellite/doom/src/i_video.c`: decouple video streaming from the rendering
  layer.
  - Removed inline UDP socket code (`InitUDPSocket`, `sendto` call, socket
    variables and platform-specific socket headers).
  - `I_InitGraphics` now calls `downlink_init("127.0.0.1", 9666)` after
    display initialisation.
  - `I_FinishUpdate` now calls `downlink_send_raw_frame(I_VideoBuffer,
    SCREENWIDTH * SCREENHEIGHT)` instead of the inlined `sendto`.
  - `I_ShutdownGraphics` now calls `downlink_shutdown()` before releasing SDL
    resources.
