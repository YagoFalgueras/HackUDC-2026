# 🛰️ DOOM Satellite — Arquitectura MVP

## Visión General

Ejecutar el motor de DOOM en una plataforma ARM AARch64 de grado espacial (satélite LEO), transmitiendo vídeo comprimido por H.264 vía downlink satelital y recibiendo input del jugador por uplink, todo con ancho de banda extremadamente limitado y latencia espacial real.

---

## Diagrama de Bloques

```
┌─────────────────────────────────────────────────────────────────┐
│                    SATÉLITE (ARM Linux SoC)                     │
│                                                                 │
│  ┌──────────────┐    framebuffer    ┌──────────────────────┐    │
│  │  DOOM Engine  │ ──────────────▶  │  H.264 SW Encoder    │    │
│  │  (Headless)   │   (shared mem)   │  (libx264 ultrafast) │    │
│  │  Hilo 1       │                  │  Hilo 2              │    │
│  └──────┬───────┘                  └──────────┬───────────┘    │
│         │ consume                              │ produce        │
│         │ input                                │ NAL units      │
│         ▼                                      ▼                │
│  ┌──────────────┐                  ┌──────────────────────┐    │
│  │  Input        │                  │  Network / RTP       │    │
│  │  Receiver     │                  │  Transmitter         │    │
│  │  (UDP Uplink) │                  │  (UDP Downlink)      │    │
│  └──────┬───────┘                  └──────────┬───────────┘    │
│         │                                      │                │
└─────────┼──────────────────────────────────────┼────────────────┘
          │            ENLACE SATELITAL           │
          │◀──── Uplink (comandos) ──────────────│
          │───── Downlink (vídeo H.264) ─────────▶│
          │                                      │
┌─────────┼──────────────────────────────────────┼────────────────┐
│         ▼          ESTACIÓN TERRESTRE          ▼                │
│  ┌──────────────┐                  ┌──────────────────────┐    │
│  │  Input        │                  │  RTP Receiver &      │    │
│  │  Capture &    │                  │  H.264 Decoder       │    │
│  │  Transmitter  │                  │  (FFmpeg / libavcodec)│   │
│  └──────────────┘                  └──────────┬───────────┘    │
│                                                ▼                │
│                                    ┌──────────────────────┐    │
│                                    │  SDL2 Display         │    │
│                                    │  Window               │    │
│                                    └──────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Estructura del Proyecto

```
doom-satellite/
│
├── README.md
├── Makefile
├── protocol.h                       # Header compartido: estructuras, constantes, utilidades inline
│
├── satellite/                       # Todo lo que corre en el satélite
│   ├── main.c                       # Entry point: init, lanzamiento de hilos, shutdown
│   ├── encoder.h                    # Interfaz del codificador H.264
│   ├── encoder.c                    # Wrapper libx264: init, encode frame, shutdown
│   ├── downlink.h                   # Interfaz del transmisor de vídeo
│   ├── downlink.c                   # Empaquetado RTP, fragmentación FU-A, envío UDP
│   ├── uplink.h                     # Interfaz del receptor de input
│   ├── uplink.c                     # Recepción UDP, deserialización, cola de input
│   └── doom/                        # Motor DOOM (fork de doomgeneric, modificado headless)
│       ├── doomgeneric.h
│       ├── doomgeneric.c
│       ├── doomgeneric_satellite.c  # Backend headless: framebuffer RAM, input por cola, sin audio
│       └── ...                      # Resto de fuentes originales de doomgeneric
│
└── ground/                          # Todo lo que corre en la estación terrestre
    ├── main.c                       # Entry point: init, loop principal, shutdown
    ├── input.h                      # Interfaz de captura de input y uplink TX
    ├── input.c                      # Captura SDL2, mapeo de teclas, envío UDP
    ├── receiver.h                   # Interfaz de recepción RTP y decodificación
    ├── receiver.c                   # Recepción RTP, reensamblaje FU-A, decode H.264
    ├── display.h                    # Interfaz del display SDL2
    └── display.c                    # Ventana SDL2, escalado, presentación de frames
```

---

## Responsabilidades por Archivo

---

### `protocol.h` — Protocolo y Constantes Compartidas

**Responsabilidad:** Header-only compartido entre `satellite/` y `ground/`. Define el protocolo binario de comunicación, las constantes del sistema y las estructuras de datos comunes. No tiene `.c` asociado; toda la lógica es inline o son macros/structs.

Contenido a definir:

- **Constantes del sistema:**
  - `FRAME_WIDTH` / `FRAME_HEIGHT` — Resolución QCIF (176 × 144).
  - `DOOM_TICK_RATE` — 35 Hz (tick rate nativo de DOOM).
  - `ENCODER_FPS` — 20 FPS objetivo del encoder.
  - `MAX_BITRATE` — Bitrate máximo del enlace (ej: 256 kbps).
  - `RTP_MTU` — Tamaño máximo de payload RTP antes de fragmentar.
  - `RING_BUFFER_SLOTS` — Capacidad del ring buffer de frames entre hilos.
  - `PORT_VIDEO` / `PORT_INPUT` — Puertos UDP para vídeo y comandos.

- **Estructuras de paquetes:**
  - `rtp_header_t` — Header RTP simplificado: version, payload type, sequence number (uint16), timestamp (uint32), marker bit (fin de frame).
  - `input_packet_t` — Paquete de input del jugador: bitfield compacto de acciones (avanzar, retroceder, girar, disparar, usar, strafe, arma) + timestamp del cliente (uint32) + sequence number (uint16).
  - `frame_slot_t` — Slot del ring buffer: array de píxeles RGB del frame + flag atómico de estado (libre/escrito/leído).

- **Funciones inline de utilidad:**
  - `pack_rtp_header()` / `unpack_rtp_header()` — Serialización/deserialización del header RTP a/desde buffer de bytes (network byte order).
  - `pack_input()` / `unpack_input()` — Serialización/deserialización del paquete de input.
  - `get_timestamp_ms()` — Wrapper de `clock_gettime(CLOCK_MONOTONIC)` que retorna milisegundos como uint32.

---

### `satellite/doom/` — Motor DOOM Headless

**Responsabilidad:** Fork de [doomgeneric](https://github.com/ozkl/doomgeneric) con las modificaciones mínimas para ejecutar sin display, sin audio, con framebuffer en RAM e inyección de input externa.

El único archivo nuevo es `doomgeneric_satellite.c`, que implementa los callbacks de la capa de abstracción de doomgeneric:

- `DG_Init()` — Reserva el framebuffer interno en resolución QCIF (176×144 × 4 bytes RGBA). Desactiva cualquier inicialización de audio o ventana.
- `DG_DrawFrame()` — Callback invocado cuando DOOM tiene un frame renderizado. Copia el framebuffer de DOOM al siguiente slot libre del ring buffer compartido con el encoder (señalizando con un flag atómico que el slot está listo para codificar).
- `DG_SleepMs(ms)` — Sleep preciso usando `clock_nanosleep(CLOCK_MONOTONIC)` para mantener el tick rate de 35 Hz.
- `DG_GetTicksMs()` — Retorna milisegundos transcurridos vía `clock_gettime(CLOCK_MONOTONIC)`.
- `DG_GetKey(pressed, key)` — Lee el siguiente evento de tecla de una cola thread-safe alimentada por `uplink.c`. Traduce del bitfield del protocolo a las keycodes internas de DOOM.
- `DG_SetWindowTitle(title)` — No-op (no hay ventana).

El resto de archivos de `doom/` son los fuentes originales de doomgeneric sin modificar (o con parches mínimos para compilar sin dependencias de audio/vídeo del sistema).

---

### `satellite/main.c` — Entry Point del Satélite

**Responsabilidad:** Orquesta la inicialización de todos los módulos del satélite, lanza los hilos y gestiona el ciclo de vida del proceso.

Funciones a implementar:

- `main()` — Inicializa los subsistemas en orden: `ring_buffer_init()` → `encoder_init()` → `downlink_init()` → `uplink_init()`. Lanza el game thread y el encoder thread con `pthread_create()`. Espera señales de shutdown (`SIGINT`/`SIGTERM`). Al recibir señal, activa flag de shutdown, hace `pthread_join()` de ambos hilos y llama a los shutdowns en orden inverso.
- `signal_handler()` — Captura señales para activar un flag de shutdown global (`volatile sig_atomic_t`).
- `game_thread_func()` — Función del hilo 1. Fija afinidad al núcleo 0 con `pthread_setaffinity_np()`. Llama a `doomgeneric_Create()` con el WAD configurado y entra en el loop `doomgeneric_Tick()` que corre a 35 Hz internamente. En cada iteración también llama a `uplink_poll()` para consumir input pendiente. El loop termina cuando el flag de shutdown se activa.
- `encoder_thread_func()` — Función del hilo 2. Fija afinidad al núcleo 1. Contiene el loop: `ring_buffer_pop()` para obtener frame → `encoder_encode_frame()` → `downlink_send_nals()` → `ring_buffer_release()`. Corre hasta el flag de shutdown.
- `ring_buffer_init()` — Reserva N slots de `frame_slot_t` en memoria. Inicializa los flags atómicos de cada slot a `SLOT_FREE`.
- `ring_buffer_push()` — Llamado desde `DG_DrawFrame()`. Busca el siguiente slot libre, copia el frame, marca como `SLOT_READY`. Si no hay slot libre, descarta el frame.
- `ring_buffer_pop()` — Llamado desde el encoder thread. Busca el siguiente slot `SLOT_READY`, retorna puntero, marca como `SLOT_READING`. Retorna NULL si no hay frame disponible.
- `ring_buffer_release()` — Marca el slot como `SLOT_FREE` para reutilización.

---

### `satellite/encoder.h` / `satellite/encoder.c` — Codificador H.264

**Responsabilidad:** Comprimir frames RAW a H.264 usando libx264 por software, optimizado para mínima latencia y bajo bitrate.

Funciones expuestas en `encoder.h`:

- `encoder_init()` — Configura libx264: `preset=ultrafast`, `tune=zerolatency`, `profile=baseline`, resolución QCIF, bitrate objetivo, sin B-frames, keyframe cada N frames. Reserva el buffer intermedio YUV420p para la conversión de color.
- `encoder_encode_frame()` — Recibe un frame RGB del ring buffer, lo convierte a YUV420p (conversión manual inline o con `swscale`), alimenta `x264_encoder_encode()`, retorna los NAL units producidos con sus tamaños en un struct de salida.
- `encoder_shutdown()` — Flush de frames pendientes en x264, liberación del contexto y buffers.

---

### `satellite/downlink.h` / `satellite/downlink.c` — Transmisor de Vídeo (Downlink TX)

**Responsabilidad:** Empaquetar NAL units H.264 en paquetes RTP/UDP y transmitirlos por el enlace de downlink hacia la estación terrestre.

Funciones expuestas en `downlink.h`:

- `downlink_init()` — Crea el socket UDP de salida, configura `struct sockaddr_in` de destino (IP/puerto de la estación terrestre). Inicializa el sequence number RTP a 0.
- `downlink_send_nals()` — Recibe los NAL units de un frame codificado. Para cada NAL: si cabe en un paquete RTP (≤ MTU), lo envía directamente con header RTP. Si excede el MTU, lo fragmenta en múltiples paquetes RTP con cabecera FU-A (fragmentation unit). Incrementa sequence number por paquete. Marca el último paquete del frame con marker bit = 1.
- `downlink_shutdown()` — Cierra el socket.

---

### `satellite/uplink.h` / `satellite/uplink.c` — Receptor de Input (Uplink RX)

**Responsabilidad:** Recibir los paquetes de input del jugador enviados desde la estación terrestre y alimentar la cola de eventos que DOOM consume.

Funciones expuestas en `uplink.h`:

- `uplink_init()` — Crea el socket UDP de escucha (non-blocking) para recibir paquetes de input, vincula al puerto configurado. Inicializa la cola de input con su mutex.
- `uplink_poll()` — Hace `recvfrom()` non-blocking. Si hay paquete, lo deserializa con `unpack_input()` y encola las teclas en la cola de input que `DG_GetKey()` consume vía `uplink_pop_key()`.
- `uplink_pop_key()` — Desencola el siguiente evento de tecla (key + pressed/released). Retorna 0 si la cola está vacía. Thread-safe (protegida con mutex). Llamada desde `DG_GetKey()` en el game thread.
- `uplink_shutdown()` — Cierra el socket, destruye el mutex.

---

### `ground/main.c` — Entry Point de la Estación Terrestre

**Responsabilidad:** Orquesta la inicialización de los tres módulos de ground, ejecuta el loop principal y gestiona el shutdown.

Funciones a implementar:

- `main()` — Inicializa los tres módulos en orden: `display_init()` → `receiver_init()` → `input_init()`. Entra en el loop principal que en cada iteración: `input_poll()` captura teclado y envía por uplink → `receiver_poll()` recibe y decodifica paquetes de vídeo → si hay frame nuevo, `display_present_frame()` lo muestra. Termina con ESC, cierre de ventana o `SIGINT`. Al salir, llama a los shutdowns en orden inverso.
- `signal_handler()` — Captura `SIGINT`/`SIGTERM` para activar flag de shutdown.

---

### `ground/input.h` / `ground/input.c` — Captura de Input y Uplink TX

**Responsabilidad:** Capturar el input del jugador via SDL2, mapearlo a acciones DOOM, y transmitirlo al satélite por UDP.

Funciones expuestas en `input.h`:

- `input_init()` — Crea el socket UDP de salida hacia el satélite (IP/puerto configurados desde argumentos o defines). SDL2 ya está inicializado por `display_init()`, así que el subsistema de eventos está disponible.
- `input_poll()` — Procesa todos los `SDL_Event` pendientes con `SDL_PollEvent()`. Actualiza un bitfield interno con las teclas mapeadas a acciones DOOM (flechas = movimiento, Ctrl = disparar, espacio = usar, shift = strafe, 1-7 = armas). Detecta eventos de quit (ESC, `SDL_QUIT`). Si el bitfield cambió respecto al frame anterior, empaqueta con `pack_input()` incluyendo timestamp y lo envía con `sendto()`. Retorna 1 si se solicitó quit, 0 en caso contrario.
- `input_shutdown()` — Cierra el socket UDP.

---

### `ground/receiver.h` / `ground/receiver.c` — Recepción RTP y Decodificación H.264

**Responsabilidad:** Recibir los paquetes RTP de vídeo del satélite por downlink, reensamblar NAL units fragmentados y decodificarlos a frames RGB usando FFmpeg.

Funciones expuestas en `receiver.h`:

- `receiver_init()` — Crea el socket UDP de escucha (non-blocking) para vídeo, vincula al puerto configurado. Inicializa el buffer de reensamblaje para fragmentos FU-A. Inicializa el decoder H.264 internamente: busca el codec con `avcodec_find_decoder(AV_CODEC_ID_H264)`, reserva `AVCodecContext`, `AVFrame`, `AVPacket`, buffer RGB de salida, e inicializa `SwsContext` para conversión YUV420p → RGB.
- `receiver_poll()` — Hace `recvfrom()` non-blocking en loop hasta que no haya más paquetes. Para cada paquete: extrae el header RTP con `unpack_rtp_header()`, si es un NAL unit completo (sin fragmentar) lo pasa directamente al decoder interno, si es un fragmento FU-A lo acumula en el buffer de reensamblaje hasta completar el NAL. El decoder interno envuelve cada NAL completo en un `AVPacket`, llama a `avcodec_send_packet()` + `avcodec_receive_frame()`, y si produce un frame lo convierte a RGB con `sws_scale()`. Retorna un puntero al buffer RGB del último frame decodificado, o NULL si no se completó ningún frame en esta llamada.
- `receiver_shutdown()` — Cierra el socket, libera el buffer de reensamblaje, libera `AVCodecContext`, `AVFrame`, `SwsContext` y buffers.

---

### `ground/display.h` / `ground/display.c` — Ventana SDL2 y Presentación de Vídeo

**Responsabilidad:** Crear y gestionar la ventana SDL2, escalar los frames QCIF y presentarlos en pantalla.

Funciones expuestas en `display.h`:

- `display_init()` — Llama a `SDL_Init(SDL_INIT_VIDEO)`. Crea `SDL_Window` (ej: 704×576, escalado x4 de QCIF), `SDL_Renderer` con flag `SDL_RENDERER_PRESENTVSYNC`, `SDL_Texture` en formato `SDL_PIXELFORMAT_RGB24` y tamaño nativo QCIF (176×144). Configura `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")` para nearest-neighbor y preservar la estética pixelada.
- `display_present_frame()` — Recibe un puntero al buffer RGB (176×144×3 bytes). Llama a `SDL_UpdateTexture()` para subir los píxeles a la textura, luego `SDL_RenderClear()` + `SDL_RenderCopy()` (SDL2 escala automáticamente al tamaño de la ventana) + `SDL_RenderPresent()`.
- `display_shutdown()` — Destruye textura, renderer y ventana. Llama a `SDL_Quit()`.

---

## Flujo de Datos

### Downlink (Satélite → Tierra): Vídeo

```
DOOM tick (35 Hz)
  → DG_DrawFrame() copia frame al ring buffer
    → Encoder thread hace pop del ring buffer
      → encoder_encode_frame(): RGB → YUV420p → x264 → NAL units
        → downlink_send_nals(): fragmentación RTP + sendto() UDP
          ~~~ enlace satelital ~~~
            → receiver_poll(): recvfrom() → reensamblaje → decode → RGB
              → display_present_frame(): SDL2 render
```

### Uplink (Tierra → Satélite): Input

```
Jugador presiona tecla
  → input_poll(): SDL_PollEvent → bitfield → pack_input() → sendto() UDP
    ~~~ enlace satelital ~~~
      → uplink_poll(): recvfrom() → unpack_input() → cola de input
        → DG_GetKey() → uplink_pop_key() → DOOM procesa
```

---

## Parámetros Clave

| Parámetro        | Valor          | Justificación                                            |
| ---------------- | -------------- | -------------------------------------------------------- |
| Resolución       | 176×144 (QCIF) | Mínima resolución estándar, reduce bitrate drásticamente |
| Game tick rate   | 35 Hz          | Rate nativo de DOOM, inamovible                          |
| Encoder FPS      | 20 FPS         | Suficiente para jugabilidad, reduce carga ~43% vs 35 FPS |
| Codec            | H.264 Baseline | Sin B-frames, mínima latencia, amplio soporte            |
| Preset x264      | ultrafast      | Mínimo uso de CPU a costa de eficiencia de compresión    |
| Tune x264        | zerolatency    | Desactiva lookahead y buffers internos                   |
| Bitrate objetivo | 128–256 kbps   | Viable para enlace satelital                             |
| Transporte       | RTP sobre UDP  | Sin retransmisiones (TCP sería letal con esta latencia)  |
| RTT objetivo     | 30–40 ms (LEO) | ~550 km órbita, 2×propagación + procesamiento            |

---

## Dependencias Externas

| Librería                             | Uso                                     | Lado     |
| ------------------------------------ | --------------------------------------- | -------- |
| **doomgeneric**                      | Motor DOOM portable en C                | Satélite |
| **libx264**                          | Codificación H.264 por software         | Satélite |
| **libavcodec / libswscale (FFmpeg)** | Decodificación H.264 + conversión color | Tierra   |
| **SDL2**                             | Ventana, rendering, input de teclado    | Tierra   |
| **pthreads**                         | Multithreading POSIX (2 hilos)          | Satélite |

---

## Compilación

```makefile
make satellite    # Compila satellite/*.c + satellite/doom/ → binario doom-sat
make ground       # Compila ground/*.c → binario doom-ground
make all          # Ambos

# Cross-compilation para ARM
make satellite CROSS=aarch64-linux-gnu-

# Test local (ambos procesos en localhost)
make test-local
```

---

## Notas de Diseño

1. **Un .c+.h por bloque:** Tanto `satellite/` como `ground/` siguen la misma estructura: un `main.c` que orquesta y un par `.c`/`.h` por cada bloque funcional del diagrama. La única excepción es `doom/` que es una carpeta con el motor completo.

2. **Ring buffer con flags atómicos:** La comunicación entre el game thread y el encoder thread usa `_Atomic int` por slot, evitando mutex en el path crítico. Solo la cola de input en `uplink.c` necesita mutex.

3. **Sin audio:** La banda sonora de DOOM se sacrifica por el ancho de banda. El espacio es silencioso de todos modos.

4. **Testeable en localhost:** Ejecutar `doom-sat` y `doom-ground` en la misma máquina con `tc netem` de Linux para simular latencia y pérdida de paquetes del enlace.
