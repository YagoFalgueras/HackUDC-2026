# 🛰️ DOOM Satellite — Arquitectura del Proyecto

## Visión General

Ejecutar el motor de DOOM en una plataforma ARM de grado espacial (satélite LEO), transmitiendo vídeo comprimido por H.264 vía downlink satelital y recibiendo input del jugador por uplink, todo con ancho de banda extremadamente limitado y latencia espacial real.

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
├── README.md                          # Descripción general, build, ejecución
├── ARCHITECTURE.md                    # Este documento
├── Makefile                           # Build principal (orquesta todos los módulos)
├── CMakeLists.txt                     # Alternativa CMake para cross-compilation ARM
│
├── common/                            # Código compartido satélite ↔ tierra
│   ├── include/
│   │   ├── protocol.h                 # Definición del protocolo de comunicación
│   │   ├── net_common.h               # Utilidades de red compartidas
│   │   ├── ring_buffer.h              # Buffer circular lock-free
│   │   └── config.h                   # Constantes globales del proyecto
│   └── src/
│       ├── protocol.c                 # Serialización/deserialización de paquetes
│       ├── net_common.c               # Helpers de sockets UDP
│       └── ring_buffer.c              # Implementación del ring buffer
│
├── satellite/                         # Todo lo que corre en el satélite
│   ├── include/
│   │   ├── doom_headless.h            # Interfaz del motor DOOM modificado
│   │   ├── encoder.h                  # Interfaz del codificador H.264
│   │   ├── downlink_tx.h              # Transmisor de vídeo (downlink)
│   │   ├── uplink_rx.h               # Receptor de comandos (uplink)
│   │   ├── frame_manager.h            # Gestión del framebuffer compartido
│   │   └── thread_manager.h           # Orquestación de hilos
│   ├── src/
│   │   ├── main.c                     # Entry point del satélite
│   │   ├── doom_headless.c            # Adaptación headless del motor DOOM
│   │   ├── encoder.c                  # Wrapper de libx264 para codificación SW
│   │   ├── downlink_tx.c              # Empaquetado RTP y envío UDP
│   │   ├── uplink_rx.c               # Recepción y decodificación de input
│   │   ├── frame_manager.c            # Double-buffer con sincronización
│   │   └── thread_manager.c           # Creación, afinidad y ciclo de vida de hilos
│   └── doom/                          # Fuentes del motor DOOM (doomgeneric fork)
│       ├── doomgeneric.h
│       ├── doomgeneric.c
│       ├── doomgeneric_satellite.c    # Backend específico para satélite
│       └── ... (fuentes originales de doomgeneric)
│
├── ground/                            # Todo lo que corre en la estación terrestre
│   ├── include/
│   │   ├── decoder.h                  # Interfaz del decodificador H.264
│   │   ├── downlink_rx.h              # Receptor de vídeo (downlink)
│   │   ├── uplink_tx.h               # Transmisor de comandos (uplink)
│   │   ├── display.h                  # Interfaz de renderizado SDL2
│   │   └── input_handler.h            # Captura de teclado/gamepad
│   └── src/
│       ├── main.c                     # Entry point de la estación terrestre
│       ├── decoder.c                  # Wrapper de libavcodec (FFmpeg)
│       ├── downlink_rx.c              # Recepción RTP, jitter buffer, reordenamiento
│       ├── uplink_tx.c               # Serialización y envío de comandos
│       ├── display.c                  # Ventana SDL2, escalado, presentación
│       └── input_handler.c            # Mapeo de teclas a acciones DOOM
│
├── configs/                           # Archivos de configuración
│   ├── satellite.conf                 # Config del lado satélite
│   ├── ground.conf                    # Config del lado terrestre
│   └── encoder_profiles/
│       ├── ultrafast_qcif.conf        # Perfil QCIF 176x144 ultrafast
│       ├── low_bandwidth.conf         # Perfil para ancho de banda crítico
│       └── high_quality.conf          # Perfil para tests en local
│
├── scripts/                           # Scripts de automatización
│   ├── cross_compile.sh               # Cross-compilation para ARM aarch64
│   ├── deploy_satellite.sh            # Despliegue al target ARM
│   ├── run_local_test.sh              # Test completo en localhost
│   └── benchmark.sh                   # Benchmark de rendimiento
│
├── docs/                              # Documentación adicional
│   ├── PROTOCOL_SPEC.md              # Especificación detallada del protocolo
│   ├── ENCODING_GUIDE.md             # Guía de parámetros de codificación
│   ├── LATENCY_ANALYSIS.md           # Análisis del presupuesto de latencia
│   └── DEPLOYMENT.md                 # Guía de despliegue en hardware real
│
└── third_party/                       # Dependencias externas
    ├── doomgeneric/                   # Fork de doomgeneric (motor DOOM portable)
    ├── x264/                          # libx264 (codificador H.264)
    └── sdl2/                          # SDL2 (solo estación terrestre)
```

---

## Responsabilidades por Archivo

---

### 📁 `common/` — Código Compartido

#### `common/include/protocol.h` / `common/src/protocol.c`
**Responsabilidad:** Definir y gestionar el protocolo binario de comunicación entre satélite y tierra.

Funciones a implementar:
- `protocol_pack_video_packet()` — Serializa un NAL unit H.264 en un paquete con header RTP simplificado (sequence number, timestamp, marker bit para fin de frame).
- `protocol_unpack_video_packet()` — Deserializa un paquete de vídeo recibido, extrayendo el NAL unit y los metadatos de transporte.
- `protocol_pack_input_packet()` — Serializa un comando de input del jugador (teclas presionadas, timestamp del cliente) en un paquete binario compacto.
- `protocol_unpack_input_packet()` — Deserializa un paquete de input recibido en el satélite.
- `protocol_compute_checksum()` — Calcula un CRC-16 o CRC-32 ligero para detección de errores en cada paquete.
- `protocol_validate_packet()` — Valida integridad del paquete (checksum, versión del protocolo, longitud coherente).

#### `common/include/net_common.h` / `common/src/net_common.c`
**Responsabilidad:** Abstraer operaciones de sockets UDP comunes a ambos lados.

Funciones a implementar:
- `net_create_udp_socket()` — Crea y configura un socket UDP (non-blocking, buffer sizes, SO_REUSEADDR).
- `net_bind_socket()` — Vincula el socket a una dirección y puerto específicos.
- `net_send_packet()` — Envía un buffer de datos por UDP a una dirección destino.
- `net_recv_packet()` — Recibe un paquete UDP con timeout configurable (poll/select).
- `net_set_socket_options()` — Configura opciones avanzadas: TOS/DSCP para QoS, tamaños de buffer del kernel.
- `net_close_socket()` — Cierra el socket y libera recursos.
- `net_resolve_address()` — Resuelve hostname a `struct sockaddr_in`.

#### `common/include/ring_buffer.h` / `common/src/ring_buffer.c`
**Responsabilidad:** Buffer circular lock-free para comunicación entre hilos (productor-consumidor) sin bloqueos mutex en el path crítico.

Funciones a implementar:
- `ring_buffer_create()` — Reserva memoria alineada para N slots de tamaño fijo, inicializa índices atómicos de lectura/escritura.
- `ring_buffer_destroy()` — Libera toda la memoria del buffer.
- `ring_buffer_push()` — Escribe un elemento en el buffer (retorna error si está lleno). Usa `atomic_store` en el índice de escritura.
- `ring_buffer_pop()` — Lee y consume el siguiente elemento disponible (retorna error si está vacío). Usa `atomic_load` en el índice de lectura.
- `ring_buffer_peek()` — Lee el siguiente elemento sin consumirlo.
- `ring_buffer_count()` — Retorna el número de elementos actualmente en el buffer.
- `ring_buffer_is_full()` — Comprueba si el buffer está lleno.
- `ring_buffer_is_empty()` — Comprueba si el buffer está vacío.
- `ring_buffer_reset()` — Resetea los índices a cero (no thread-safe, para reinicio).

#### `common/include/config.h`
**Responsabilidad:** Centralizar todas las constantes y parámetros configurables del sistema.

Definiciones clave:
- `DOOM_TICK_RATE` — Frecuencia del game loop (35 Hz).
- `ENCODER_TARGET_FPS` — FPS objetivo del encoder (20 FPS).
- `FRAME_WIDTH` / `FRAME_HEIGHT` — Resolución QCIF (176 × 144).
- `MAX_BITRATE` — Bitrate máximo del enlace (ej: 256 kbps).
- `RTP_PAYLOAD_MAX_SIZE` — MTU efectivo para fragmentación de NAL units.
- `INPUT_PACKET_SIZE` — Tamaño fijo del paquete de input del jugador.
- `RING_BUFFER_CAPACITY` — Número de slots del ring buffer de frames.
- `JITTER_BUFFER_MS` — Tamaño del jitter buffer en la estación terrestre.

---

### 📁 `satellite/` — Software del Satélite

#### `satellite/src/main.c`
**Responsabilidad:** Entry point del proceso en el satélite. Orquesta la inicialización y el ciclo de vida completo.

Funciones a implementar:
- `main()` — Parsea configuración, inicializa subsistemas (DOOM, encoder, red), lanza hilos via `thread_manager`, espera señales de shutdown, ejecuta cleanup ordenado.
- `signal_handler()` — Captura SIGINT/SIGTERM para shutdown graceful.
- `load_config()` — Lee el archivo `satellite.conf` y llena la estructura de configuración global.

#### `satellite/src/doom_headless.c`
**Responsabilidad:** Adaptar el motor DOOM para ejecución sin display. El motor renderiza frames a un buffer en RAM en lugar de a una pantalla.

Funciones a implementar:
- `doom_init()` — Inicializa el motor DOOM con el WAD especificado, configura el framebuffer interno en resolución QCIF, desactiva el audio.
- `doom_tick()` — Ejecuta un tick de la lógica del juego (35 Hz). Procesa el input pendiente, actualiza física, IA de enemigos, y renderiza el frame actual al buffer interno.
- `doom_inject_input()` — Inyecta un comando de input del jugador (recibido por uplink) en la cola de eventos de DOOM. Traduce del formato del protocolo al formato de eventos interno de DOOM.
- `doom_get_framebuffer()` — Retorna un puntero al framebuffer actual (array de píxeles RGB/RGBA en resolución QCIF). El frame está listo para ser consumido por el encoder.
- `doom_shutdown()` — Libera todos los recursos del motor DOOM.
- `doom_is_running()` — Retorna si el motor está activo (no ha terminado el nivel ni el jugador ha salido).

#### `satellite/src/encoder.c`
**Responsabilidad:** Comprimir frames RAW a H.264 usando libx264 por software, optimizado para mínima latencia y bajo bitrate.

Funciones a implementar:
- `encoder_init()` — Inicializa libx264 con los parámetros del perfil seleccionado: `preset=ultrafast`, `tune=zerolatency`, `profile=baseline`, resolución QCIF, bitrate objetivo, sin B-frames, keyframe interval configurable.
- `encoder_encode_frame()` — Recibe un frame RAW (RGB), lo convierte a YUV420p (colorspace de x264), codifica y produce uno o más NAL units H.264. Retorna los NAL units en un buffer de salida con sus tamaños.
- `encoder_request_keyframe()` — Fuerza un I-frame en el siguiente frame codificado (para recuperación tras pérdida de paquetes o reconexión del ground).
- `encoder_update_bitrate()` — Ajusta el bitrate objetivo en caliente (rate control adaptativo) sin reiniciar el encoder, para adaptarse a condiciones cambiantes del enlace.
- `encoder_get_stats()` — Retorna estadísticas del encoder: bitrate actual, QP promedio, frames codificados, bytes generados.
- `encoder_shutdown()` — Flush de frames pendientes, liberación de contexto x264.

#### `satellite/src/downlink_tx.c`
**Responsabilidad:** Empaquetar NAL units H.264 en paquetes RTP/UDP y transmitirlos por el enlace de downlink hacia la estación terrestre.

Funciones a implementar:
- `downlink_tx_init()` — Crea el socket UDP de salida, configura la dirección destino (ground station), inicializa el sequence number RTP.
- `downlink_tx_send_frame()` — Recibe los NAL units de un frame codificado. Si un NAL unit excede el MTU, lo fragmenta en múltiples paquetes RTP (FU-A fragmentation). Envía cada paquete con su header RTP (sequence, timestamp, marker).
- `downlink_tx_get_stats()` — Retorna estadísticas de transmisión: paquetes enviados, bytes enviados, tasa de envío actual.
- `downlink_tx_shutdown()` — Cierra el socket y libera recursos.

#### `satellite/src/uplink_rx.c`
**Responsabilidad:** Recibir y procesar los paquetes de input del jugador enviados desde la estación terrestre.

Funciones a implementar:
- `uplink_rx_init()` — Crea el socket UDP de escucha para recibir comandos del jugador, vincula al puerto configurado.
- `uplink_rx_poll()` — Sondea el socket (non-blocking) por paquetes de input. Deserializa cada paquete recibido y lo encola para ser consumido por el game thread.
- `uplink_rx_get_latest_input()` — Retorna el input más reciente recibido. Si hay múltiples paquetes acumulados, descarta los obsoletos y retorna solo el último (para minimizar input lag).
- `uplink_rx_get_stats()` — Retorna estadísticas: paquetes recibidos, paquetes descartados (fuera de orden o duplicados), último timestamp recibido.
- `uplink_rx_shutdown()` — Cierra el socket de escucha.

#### `satellite/src/frame_manager.c`
**Responsabilidad:** Gestionar el framebuffer compartido entre el hilo del juego (productor) y el hilo del encoder (consumidor) con sincronización eficiente.

Funciones a implementar:
- `frame_manager_init()` — Reserva memoria para un esquema de double-buffering o triple-buffering. Inicializa mutexes y variables de condición.
- `frame_manager_submit_frame()` — Llamado por el game thread: copia el frame actual del motor DOOM al buffer de escritura y señaliza al encoder que hay un frame nuevo disponible.
- `frame_manager_acquire_frame()` — Llamado por el encoder thread: bloquea hasta que haya un frame nuevo disponible, retorna un puntero al buffer de lectura.
- `frame_manager_release_frame()` — Llamado por el encoder thread cuando ha terminado de codificar el frame, libera el buffer para que pueda ser reutilizado.
- `frame_manager_get_drop_count()` — Retorna el número de frames descartados (el game thread produjo un frame nuevo antes de que el encoder consumiera el anterior).
- `frame_manager_destroy()` — Libera buffers, mutexes y condiciones.

#### `satellite/src/thread_manager.c`
**Responsabilidad:** Crear, configurar y supervisar los hilos del sistema con afinidad a núcleos específicos del ARM.

Funciones a implementar:
- `thread_manager_init()` — Detecta el número de núcleos disponibles del CPU ARM.
- `thread_manager_launch_game_thread()` — Crea el hilo del game loop (DOOM), lo fija al núcleo 0, configura prioridad SCHED_FIFO alta.
- `thread_manager_launch_encoder_thread()` — Crea el hilo del encoder, lo fija al núcleo 1, configura prioridad alta pero menor que el game thread.
- `thread_manager_launch_network_thread()` — Crea el hilo de red (TX downlink + RX uplink), lo fija a un núcleo separado si está disponible.
- `thread_manager_wait_all()` — Espera (join) a que todos los hilos terminen.
- `thread_manager_request_shutdown()` — Señaliza a todos los hilos que deben terminar su ciclo actual y salir limpiamente.
- `thread_manager_shutdown()` — Espera la finalización y libera recursos de hilos.

#### `satellite/doom/doomgeneric_satellite.c`
**Responsabilidad:** Implementar el backend de la capa de abstracción de `doomgeneric` para el entorno del satélite.

Funciones a implementar (callbacks requeridos por doomgeneric):
- `DG_Init()` — Inicialización del backend: reserva del framebuffer QCIF, configuración sin audio.
- `DG_DrawFrame()` — Callback invocado cuando DOOM tiene un frame listo. Copia el framebuffer de DOOM al frame_manager para que el encoder lo consuma.
- `DG_SleepMs()` — Implementación de sleep preciso usando `clock_nanosleep()` para mantener los 35 Hz.
- `DG_GetTicksMs()` — Retorna el tiempo en milisegundos usando `clock_gettime(CLOCK_MONOTONIC)`.
- `DG_GetKey()` — Lee el siguiente evento de tecla de la cola de input (alimentada por uplink_rx).
- `DG_SetWindowTitle()` — No-op (no hay ventana en modo headless).

---

### 📁 `ground/` — Software de la Estación Terrestre

#### `ground/src/main.c`
**Responsabilidad:** Entry point de la estación terrestre. Inicializa display, red, input y el loop principal.

Funciones a implementar:
- `main()` — Parsea configuración, inicializa SDL2, decodificador, sockets de red, lanza el loop principal que: captura input → envía uplink → recibe downlink → decodifica → muestra en pantalla.
- `signal_handler()` — Captura SIGINT/SIGTERM para shutdown graceful.
- `load_config()` — Lee `ground.conf`.

#### `ground/src/decoder.c`
**Responsabilidad:** Decodificar el stream H.264 recibido a frames RAW para su visualización.

Funciones a implementar:
- `decoder_init()` — Inicializa libavcodec con el decoder H.264, reserva el `AVCodecContext` y los buffers de salida.
- `decoder_decode_packet()` — Recibe un paquete conteniendo un NAL unit (o fragmento), lo alimenta al decoder. Si el decoder produce un frame completo, lo convierte de YUV420p a RGB y lo retorna.
- `decoder_flush()` — Flush del decoder para obtener frames en pipeline.
- `decoder_get_stats()` — Retorna estadísticas: frames decodificados, tiempo promedio de decodificación, errores.
- `decoder_shutdown()` — Libera el contexto de AVCodec y todos los buffers.

#### `ground/src/downlink_rx.c`
**Responsabilidad:** Recibir los paquetes RTP de vídeo del satélite, manejar el jitter y reensamblar los frames.

Funciones a implementar:
- `downlink_rx_init()` — Crea el socket UDP de escucha para vídeo, configura el jitter buffer con el tamaño especificado.
- `downlink_rx_poll()` — Sondea el socket por paquetes RTP. Inserta cada paquete en el jitter buffer ordenado por sequence number.
- `downlink_rx_get_next_nal()` — Extrae el siguiente NAL unit completo del jitter buffer (reensamblando fragmentos FU-A si es necesario). Retorna NULL si aún no se han recibido todos los fragmentos.
- `downlink_rx_request_keyframe()` — Envía un mensaje de control al satélite solicitando un I-frame (usado tras detectar pérdida de paquetes significativa).
- `downlink_rx_get_stats()` — Retorna estadísticas: paquetes recibidos, paquetes perdidos (gaps en sequence number), jitter calculado, NAL units reensamblados.
- `downlink_rx_shutdown()` — Cierra el socket y libera el jitter buffer.

#### `ground/src/uplink_tx.c`
**Responsabilidad:** Enviar los comandos del jugador al satélite por el enlace de uplink.

Funciones a implementar:
- `uplink_tx_init()` — Crea el socket UDP de salida hacia el satélite, configura la dirección destino.
- `uplink_tx_send_input()` — Recibe el estado actual del input del jugador, lo serializa con el protocolo y lo envía por UDP. Incluye timestamp del cliente para medición de RTT.
- `uplink_tx_send_control()` — Envía mensajes de control (solicitud de keyframe, cambio de bitrate, ping).
- `uplink_tx_get_stats()` — Retorna estadísticas de envío.
- `uplink_tx_shutdown()` — Cierra el socket.

#### `ground/src/display.c`
**Responsabilidad:** Renderizar los frames decodificados en una ventana SDL2 con escalado.

Funciones a implementar:
- `display_init()` — Crea la ventana SDL2, el renderer y la textura de streaming. Configura el escalado de QCIF (176×144) a la resolución de la ventana (ej: 704×576, x4) con interpolación nearest-neighbor para preservar la estética pixelada.
- `display_present_frame()` — Recibe un frame RGB decodificado, lo sube a la textura SDL2 y ejecuta `SDL_RenderPresent()`.
- `display_toggle_fullscreen()` — Alterna entre modo ventana y fullscreen.
- `display_set_title()` — Actualiza el título de la ventana con info de conexión.
- `display_shutdown()` — Destruye textura, renderer y ventana SDL2.

#### `ground/src/input_handler.c`
**Responsabilidad:** Capturar el input del jugador (teclado y/o gamepad) y mapearlo a acciones de DOOM.

Funciones a implementar:
- `input_handler_init()` — Inicializa el subsistema de eventos SDL2 para teclado y opcionalmente gamepad.
- `input_handler_poll()` — Procesa los eventos SDL2 pendientes. Actualiza el estado interno de teclas presionadas/soltadas.
- `input_handler_get_state()` — Retorna el estado completo del input como un bitfield compacto (avanzar, retroceder, girar izquierda/derecha, disparar, usar, strafe, weapon switch). Diseñado para caber en el paquete de input mínimo del protocolo.
- `input_handler_is_quit_requested()` — Retorna true si el jugador ha solicitado salir (ESC o cierre de ventana).
- `input_handler_shutdown()` — Cierra el subsistema de eventos SDL2.

---

## Flujo de Datos Detallado

### Downlink (Satélite → Tierra): Vídeo

```
DOOM tick (35 Hz)
  → Renderiza frame 176×144 RGB al framebuffer
    → frame_manager copia al buffer compartido
      → Encoder thread adquiere el frame
        → Conversión RGB → YUV420p
          → libx264 codifica → NAL units H.264
            → Fragmentación si NAL > MTU
              → Empaquetado RTP con sequence number
                → Envío UDP por downlink
                  ~~~ enlace satelital ~~~
                    → Recepción en ground, jitter buffer
                      → Reensamblaje de fragmentos
                        → libavcodec decodifica → frame YUV
                          → Conversión YUV → RGB
                            → SDL2 presenta en pantalla
```

### Uplink (Tierra → Satélite): Input

```
Jugador presiona tecla
  → SDL2 event capturado
    → input_handler genera bitfield de estado
      → Serialización con timestamp
        → Envío UDP por uplink
          ~~~ enlace satelital ~~~
            → uplink_rx recibe y deserializa
              → doom_inject_input() encola el evento
                → DOOM procesa en el siguiente tick
```

---

## Parámetros Clave del Sistema

| Parámetro | Valor | Justificación |
|---|---|---|
| Resolución | 176×144 (QCIF) | Mínima resolución estándar, reduce bitrate drásticamente |
| Game tick rate | 35 Hz | Rate nativo de DOOM, inamovible |
| Encoder FPS | 20 FPS | Suficiente para jugabilidad, reduce carga del encoder ~43% vs 35 FPS |
| Codec | H.264 Baseline | Sin B-frames, mínima latencia, amplio soporte |
| Preset x264 | ultrafast | Mínimo uso de CPU a costa de eficiencia de compresión |
| Tune x264 | zerolatency | Desactiva lookahead y buffers internos, minimiza latencia |
| Bitrate objetivo | 128–256 kbps | Viable para enlace satelital, suficiente para QCIF |
| Transporte | RTP sobre UDP | Sin retransmisiones (TCP sería letal con esta latencia) |
| RTT objetivo | 30–40 ms (LEO) | ~550 km órbita, 2×propagación + procesamiento |
| Jitter buffer | 20–50 ms | Absorbe variaciones del enlace sin añadir latencia excesiva |

---

## Dependencias Externas

| Librería | Uso | Lado |
|---|---|---|
| **doomgeneric** | Motor DOOM portable en C | Satélite |
| **libx264** | Codificación H.264 por software | Satélite |
| **libavcodec (FFmpeg)** | Decodificación H.264 | Tierra |
| **SDL2** | Ventana, rendering, input | Tierra |
| **pthreads** | Multithreading POSIX | Satélite |

---

## Compilación y Targets

```makefile
# Targets principales
make satellite          # Compila el binario del satélite (native o cross-compile)
make ground             # Compila el cliente de la estación terrestre
make all                # Todo

# Cross-compilation para ARM
make satellite CROSS=aarch64-linux-gnu-

# Testing local
make test-local         # Lanza satélite + ground en localhost con tc/netem para simular latencia
```

---

## Notas de Diseño

1. **Zero-copy donde sea posible:** El frame_manager usa double-buffering con intercambio de punteros, no copias de memoria innecesarias.

2. **Graceful degradation:** Si el encoder no puede mantener 20 FPS, se reduce automáticamente el target. Si el enlace se degrada, se baja el bitrate dinámicamente.

3. **Recuperación ante errores:** Ante pérdida de paquetes significativa, el ground solicita un keyframe. El decoder puede resincronizarse desde cualquier I-frame.

4. **Sin dependencia de audio:** El audio se elimina completamente en el MVP. La banda sonora de DOOM se sacrifica por el ancho de banda. El espacio es silencioso de todos modos.

5. **Testeable sin hardware espacial:** Todo el pipeline se puede ejecutar en dos procesos en localhost usando `tc`/`netem` de Linux para simular latencia y pérdida de paquetes del enlace satelital.
