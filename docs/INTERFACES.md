# Interfaces del Sistema DOOM Satellite

Este documento describe las interfaces definidas entre los distintos bloques del diagrama de arquitectura del proyecto DOOM Satellite.

## Índice

1. [Visión General](#visión-general)
2. [Interfaces del Satélite](#interfaces-del-satélite)
3. [Interfaces de la Estación Terrestre](#interfaces-de-la-estación-terrestre)
4. [Protocolo de Comunicación](#protocolo-de-comunicación)
5. [Interfaz con el Motor DOOM](#interfaz-con-el-motor-doom)
6. [Estado de Implementación](#estado-de-implementación)

---

## Visión General

El sistema está dividido en dos subsistemas principales:
- **Satélite**: Ejecuta DOOM, codifica vídeo y gestiona comunicaciones
- **Estación Terrestre**: Captura input, decodifica vídeo y presenta en pantalla

Cada subsistema está compuesto por módulos independientes que se comunican a través de interfaces bien definidas.

---

## Interfaces del Satélite

### 1. Encoder (`satellite/encoder.h`)

**Responsabilidad**: Codificar frames RGB a H.264 usando libx264

#### Estructuras de Datos

```c
typedef struct {
    uint8_t **nals;        // Array de punteros a NAL units
    size_t *nal_sizes;     // Array de tamaños de cada NAL unit
    int num_nals;          // Número de NAL units en este frame
} encoder_output_t;
```

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `encoder_init()` | Inicializa libx264 con preset=ultrafast, tune=zerolatency, profile=baseline | 0 éxito, -1 error |
| `encoder_encode_frame(rgb_data, output)` | Convierte RGB→YUV420p y codifica a H.264 | Número de NALs o -1 |
| `encoder_shutdown()` | Flush de frames pendientes y liberación de recursos | void |

#### Parámetros de Configuración
- Preset: `ultrafast` (mínimo CPU)
- Tune: `zerolatency` (sin buffering)
- Profile: `baseline` (sin B-frames)
- Resolución: QCIF (176×144)
- Bitrate: definido en `protocol.h`

---

### 2. Downlink (`satellite/downlink.h`)

**Responsabilidad**: Transmisión de vídeo H.264 empaquetado en RTP/UDP

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `downlink_init(dest_ip, dest_port)` | Crea socket UDP y configura destino | 0 éxito, -1 error |
| `downlink_send_nals(nals, sizes, num, timestamp)` | Empaqueta NAL units en RTP y transmite por UDP | Número de paquetes enviados o -1 |
| `downlink_send_raw_frame(buffer, size)` | Envía framebuffer raw sin codificar (fallback) | Bytes enviados o -1 |
| `downlink_shutdown()` | Cierra socket | void |

#### Características RTP
- **Fragmentación FU-A**: NAL units > MTU se fragmentan automáticamente
  - Primer fragmento: bit S=1
  - Último fragmento: bit E=1
- **Marker bit**: Último paquete del frame con marker=1
- **Sequence number**: Incremento automático por paquete

---

### 3. Uplink (`satellite/uplink.h`)

**Responsabilidad**: Recepción de comandos del jugador por UDP

#### Estructuras de Datos

```c
typedef struct {
    uint16_t key;      // Código de tecla DOOM
    bool pressed;      // true=presionada, false=liberada
} key_event_t;
```

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `uplink_init(listen_port)` | Crea socket UDP non-blocking, inicializa cola de input | 0 éxito, -1 error |
| `uplink_poll()` | Recibe paquetes UDP y alimenta cola de eventos | Paquetes procesados o -1 |
| `uplink_pop_key(key, pressed)` | Desencola evento de tecla (thread-safe) | 1 si hay evento, 0 si vacío |
| `uplink_shutdown()` | Cierra socket y destruye mutex | void |

#### Detalles de Implementación
- **Cola thread-safe**: Mutex protege acceso concurrente entre `uplink_poll()` y `uplink_pop_key()`
- **Detección de cambios**: Solo genera eventos para teclas que cambiaron de estado
- **Non-blocking**: `recvfrom()` no bloquea el game loop

---

### 4. Orquestador del Satélite (`satellite/main.c`)

**Responsabilidad**: Coordinar inicialización, hilos y ciclo de vida

#### Arquitectura de Hilos

| Hilo | Afinidad | Función | Frecuencia |
|------|----------|---------|------------|
| Hilo 1 (Game) | Core 0 | `game_thread_func()` | 35 Hz (tick rate DOOM) |
| Hilo 2 (Encoder) | Core 1 | `encoder_thread_func()` | 20 FPS (encoding) |

#### Ring Buffer (Comunicación entre Hilos)

```c
typedef struct {
    uint8_t rgb_data[FRAME_WIDTH * FRAME_HEIGHT * 3];
    _Atomic int status;  // SLOT_FREE, SLOT_READY, SLOT_READING
} frame_slot_t;
```

**Funciones del Ring Buffer**:
- `ring_buffer_init()`: Reserva N slots en memoria
- `ring_buffer_push()`: Llamado por `DG_DrawFrame()` desde game thread
- `ring_buffer_pop()`: Llamado por encoder thread
- `ring_buffer_release()`: Marca slot como libre tras encoding

**Estados de Slot**:
- `SLOT_FREE`: Disponible para escritura
- `SLOT_READY`: Frame listo para encoding
- `SLOT_READING`: Siendo procesado por encoder

---

## Interfaces de la Estación Terrestre

### 1. Display (`ground/display.h`)

**Responsabilidad**: Ventana SDL2 y presentación de frames

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `display_init()` | Crea ventana SDL2 (704×576), renderer, textura RGB24 QCIF | 0 éxito, -1 error |
| `display_present_frame(rgb_buffer)` | Actualiza textura y presenta frame (176×144×3 bytes) | void |
| `display_shutdown()` | Destruye textura, renderer, ventana y llama SDL_Quit() | void |

#### Características de Rendering
- **Escalado**: x4 de QCIF (176×144 → 704×576)
- **Filtrado**: Nearest-neighbor (`SDL_HINT_RENDER_SCALE_QUALITY=0`) para estética pixelada
- **VSync**: `SDL_RENDERER_PRESENTVSYNC` para evitar tearing
- **Formato**: RGB24 (3 bytes por píxel)

---

### 2. Input (`ground/input.h`)

**Responsabilidad**: Captura de teclado SDL2 y transmisión por UDP

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `input_init(satellite_ip, satellite_port)` | Crea socket UDP hacia satélite, inicializa bitfield | 0 éxito, -1 error |
| `input_poll()` | Procesa eventos SDL2, actualiza bitfield, transmite si hay cambios | 1 si quit, 0 normal |
| `input_shutdown()` | Cierra socket UDP | void |

#### Mapeo de Teclas a Acciones DOOM

| Tecla(s) | Acción DOOM |
|----------|-------------|
| Flecha Arriba/Abajo | Avanzar/Retroceder |
| Flecha Izq/Der | Girar |
| Ctrl | Disparar |
| Espacio | Usar/Abrir |
| Shift | Strafe |
| 1-7 | Cambio de arma |
| ESC | Quit |

#### Optimización de Transmisión
- **Solo envía cuando cambia**: Compara bitfield actual con anterior
- **Incluye timestamp**: Timestamp del cliente en el paquete
- **Sequence number**: Incremento automático para detección de pérdidas

---

### 3. Receiver (`ground/receiver.h`)

**Responsabilidad**: Recepción RTP, reensamblaje y decodificación H.264

#### Funciones Públicas

| Función | Descripción | Retorno |
|---------|-------------|---------|
| `receiver_init(listen_port)` | Socket UDP non-blocking, inicializa FFmpeg decoder | 0 éxito, -1 error |
| `receiver_poll()` | Recibe RTP, reensamble FU-A, decodifica H.264→RGB | Puntero a RGB o NULL |
| `receiver_shutdown()` | Cierra socket, libera AVCodecContext/AVFrame/SwsContext | void |

#### Pipeline de Decodificación

```
recvfrom() → unpack_rtp_header()
           ↓
      ¿NAL completo o FU-A?
           ↓
    ┌──────┴──────┐
    │             │
 Completo      FU-A → Buffer reensamblaje → NAL completo
    │             │
    └──────┬──────┘
           ↓
    AVPacket → avcodec_send_packet()
           ↓
    avcodec_receive_frame() → AVFrame (YUV420p)
           ↓
    sws_scale() → RGB24 buffer interno
           ↓
    return rgb_buffer
```

#### Componentes FFmpeg Utilizados
- `AVCodecContext`: Contexto del decoder H.264
- `AVFrame`: Frame YUV decodificado
- `AVPacket`: Paquete H.264 a decodificar
- `SwsContext`: Conversión YUV420p → RGB24

---

### 4. Orquestador de la Estación (`ground/main.c`)

**Responsabilidad**: Loop principal y coordinación de módulos

#### Secuencia de Inicialización

```
1. display_init()
2. receiver_init()
3. input_init() (actualmente no integrado en main.c)
```

#### Loop Principal

```c
while (running) {
    // Procesar eventos SDL
    SDL_PollEvent() → detectar QUIT/ESC

    // Recibir y decodificar frame
    frame = receiver_poll()

    // Presentar si hay frame nuevo
    if (frame)
        display_present_frame(frame)
    else
        SDL_Delay(1)  // Espera activa suave
}
```

#### Secuencia de Shutdown

```
1. receiver_shutdown()
2. display_shutdown()
```

---

## Protocolo de Comunicación

### Ubicación
`common/include/protocol.h` (compartido entre satélite y tierra)

### Estado Actual
**PENDIENTE DE IMPLEMENTACIÓN** - Solo contiene estructura esqueleto con TODOs

### Estructuras a Definir

#### 1. Header RTP

```c
typedef struct {
    uint8_t version;       // Versión RTP (2)
    uint8_t payload_type;  // Tipo de payload (H.264)
    uint16_t seq_number;   // Número de secuencia
    uint32_t timestamp;    // Timestamp (90 kHz para vídeo)
    uint8_t marker;        // Marker bit (fin de frame)
} rtp_header_t;
```

#### 2. Paquete de Input

```c
typedef struct {
    uint16_t bitfield;     // Teclas presionadas (bit flags)
    uint32_t timestamp;    // Timestamp del cliente
    uint16_t seq_number;   // Número de secuencia
} input_packet_t;
```

**Bitfield de Input**:
```
Bit 0: Avanzar
Bit 1: Retroceder
Bit 2: Girar izquierda
Bit 3: Girar derecha
Bit 4: Disparar
Bit 5: Usar/Abrir
Bit 6: Strafe izquierda
Bit 7: Strafe derecha
Bits 8-10: Número de arma (0-7)
```

#### 3. Slot del Ring Buffer (Satélite)

```c
typedef struct {
    uint8_t rgb_data[FRAME_WIDTH * FRAME_HEIGHT * 3];
    _Atomic int status;  // SLOT_FREE/SLOT_READY/SLOT_READING
} frame_slot_t;
```

### Constantes del Sistema

| Constante | Valor | Descripción |
|-----------|-------|-------------|
| `FRAME_WIDTH` | 176 | Ancho QCIF |
| `FRAME_HEIGHT` | 144 | Alto QCIF |
| `DOOM_TICK_RATE` | 35 Hz | Tick rate nativo de DOOM |
| `ENCODER_FPS` | 20 FPS | Frame rate del encoder |
| `MAX_BITRATE` | 128-256 kbps | Bitrate objetivo |
| `RTP_MTU` | ~1400 bytes | MTU para fragmentación |
| `RING_BUFFER_SLOTS` | 4-8 | Capacidad del ring buffer |
| `PORT_VIDEO` | 9666 | Puerto downlink (vídeo) |
| `PORT_INPUT` | 9667 | Puerto uplink (input) |

### Funciones de Utilidad (inline)

```c
// Serialización/deserialización RTP
void pack_rtp_header(const rtp_header_t *hdr, uint8_t *buf);
void unpack_rtp_header(const uint8_t *buf, rtp_header_t *hdr);

// Serialización/deserialización Input
void pack_input(const input_packet_t *pkt, uint8_t *buf);
void unpack_input(const uint8_t *buf, input_packet_t *pkt);

// Timestamp monotónico
uint32_t get_timestamp_ms(void);
```

---

## Interfaz con el Motor DOOM

### Backend doomgeneric (`satellite/doom/doomgeneric_satellite.c`)

**Responsabilidad**: Adaptar DOOM al entorno headless del satélite

#### Estado Actual
**PENDIENTE DE IMPLEMENTACIÓN** - Solo contiene estructura esqueleto con TODOs

#### Callbacks Requeridos por doomgeneric

| Callback | Responsabilidad | Retorno |
|----------|-----------------|---------|
| `DG_Init()` | Reserva framebuffer QCIF, configura sin audio | void |
| `DG_DrawFrame()` | Copia framebuffer a ring buffer vía `ring_buffer_push()` | void |
| `DG_SleepMs(ms)` | Sleep preciso con `clock_nanosleep(CLOCK_MONOTONIC)` | void |
| `DG_GetTicksMs()` | Retorna milisegundos desde inicio con `clock_gettime()` | uint32_t |
| `DG_GetKey(pressed, key)` | Lee evento de cola vía `uplink_pop_key()` | int |
| `DG_SetWindowTitle(title)` | No-op (sin ventana) | void |

#### Flujo de Datos con Ring Buffer

```
DOOM (35 Hz) → DG_DrawFrame()
                    ↓
             Copia framebuffer
                    ↓
         ring_buffer_push() (thread-safe con atomic flags)
                    ↓
         Marca slot como SLOT_READY
                    ↓
    Encoder thread lee con ring_buffer_pop()
```

#### Flujo de Input

```
Estación terrestre → UDP uplink → uplink_poll()
                                        ↓
                                 Cola thread-safe
                                        ↓
                    DG_GetKey() → uplink_pop_key()
                                        ↓
                                DOOM procesa tecla
```

---

## Estado de Implementación

### ✅ Interfaces Completamente Definidas

| Módulo | Header | Estado |
|--------|--------|--------|
| Encoder | [satellite/encoder.h](../satellite/encoder.h) | ✅ Definido |
| Downlink | [satellite/downlink.h](../satellite/downlink.h) | ✅ Definido |
| Uplink | [satellite/uplink.h](../satellite/uplink.h) | ✅ Definido |
| Display | [ground/display.h](../ground/display.h) | ✅ Definido |
| Input | [ground/input.h](../ground/input.h) | ✅ Definido |
| Receiver | [ground/receiver.h](../ground/receiver.h) | ✅ Definido |

### ⚠️ Interfaces Parcialmente Definidas

| Módulo | Ubicación | Estado | Pendiente |
|--------|-----------|--------|-----------|
| Ring Buffer | `satellite/main.c` | ⚠️ Parcial | Implementar funciones en `.c` |
| Game/Encoder Threads | `satellite/main.c` | ⚠️ Parcial | Implementar funciones de hilo |

### ❌ Interfaces Pendientes de Implementación

| Módulo | Ubicación | Estado | Razón |
|--------|-----------|--------|-------|
| Protocol | [common/include/protocol.h](../common/include/protocol.h) | ❌ Solo esqueleto | Solo tiene TODOs, sin estructuras ni funciones |
| DOOM Backend | [satellite/doom/doomgeneric_satellite.c](../satellite/doom/doomgeneric_satellite.c) | ❌ Solo esqueleto | Solo comentarios, sin implementación |

---

## Diagrama de Interfaces

```
┌─────────────────────── SATÉLITE ───────────────────────┐
│                                                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │         DOOM Engine (doomgeneric)               │   │
│  │  Callbacks: DG_Init, DG_DrawFrame, DG_GetKey    │   │
│  └──────┬──────────────────────────────┬───────────┘   │
│         │ framebuffer RGB              │ consume input │
│         ▼                              ▼               │
│  ┌─────────────────┐          ┌─────────────────┐      │
│  │  Ring Buffer    │          │  Uplink RX      │      │
│  │  (atomic flags) │          │  uplink.h       │      │
│  └────────┬────────┘          └────────▲────────┘      │
│           │ frame                      │ UDP packets   │
│           ▼                            │               │
│  ┌─────────────────┐                  │               │
│  │  Encoder        │          ┌───────┴─────────┐      │
│  │  encoder.h      │          │   protocol.h    │      │
│  │  (libx264)      │          │   (TODO)        │      │
│  └────────┬────────┘          └───────┬─────────┘      │
│           │ NAL units                 │               │
│           ▼                            ▼               │
│  ┌─────────────────┐          ┌─────────────────┐      │
│  │  Downlink TX    │          │                 │      │
│  │  downlink.h     │          │                 │      │
│  │  (RTP/UDP)      │          │                 │      │
│  └────────┬────────┘          └─────────────────┘      │
│           │                                            │
└───────────┼────────────────────────────────────────────┘
            │ UDP downlink (vídeo H.264)
            ▼
    ~~~ ENLACE SATELITAL ~~~
            │
            ▼ UDP uplink (input)
┌───────────┼────────────── ESTACIÓN TERRESTRE ──────────┐
│           │                             ▲              │
│  ┌────────▼────────┐          ┌─────────┴────────┐    │
│  │  Receiver       │          │  Input Capture   │    │
│  │  receiver.h     │          │  input.h         │    │
│  │  (RTP/FFmpeg)   │          │  (SDL2 Events)   │    │
│  └────────┬────────┘          └──────────────────┘    │
│           │ RGB buffer                                │
│           ▼                                           │
│  ┌─────────────────┐                                  │
│  │  Display        │                                  │
│  │  display.h      │                                  │
│  │  (SDL2 Window)  │                                  │
│  └─────────────────┘                                  │
│                                                        │
└────────────────────────────────────────────────────────┘
```

---

## Resumen de Interfaces por Bloque

### Satélite
1. **DOOM Engine** → Ring Buffer (via `DG_DrawFrame()`)
2. **Ring Buffer** → Encoder (via `ring_buffer_pop()`)
3. **Encoder** → Downlink (via `encoder_output_t`)
4. **Downlink** → Red (RTP/UDP)
5. **Red** → Uplink (UDP)
6. **Uplink** → DOOM Engine (via cola thread-safe)

### Estación Terrestre
1. **Jugador** → Input Capture (SDL2 Events)
2. **Input Capture** → Red (UDP)
3. **Red** → Receiver (RTP/UDP)
4. **Receiver** → Display (RGB buffer)
5. **Display** → Pantalla (SDL2)

### Comunicación Entre Sistemas
- **Downlink (Satélite → Tierra)**: RTP/UDP con H.264 NAL units (fragmentación FU-A)
- **Uplink (Tierra → Satélite)**: UDP con bitfield de input serializado

---

## Notas Adicionales

### Thread-Safety
- **Ring buffer**: Flags atómicos (`_Atomic int`) evitan mutex en path crítico
- **Cola de input**: Mutex protege acceso concurrente

### Optimizaciones de Latencia
- **Encoder**: preset=ultrafast, tune=zerolatency, sin B-frames
- **Transporte**: UDP (sin retransmisiones TCP)
- **Sockets non-blocking**: `recvfrom()` no bloquea loops principales

### Gestión de Recursos
- **Inicialización**: Orden fijo con verificación de errores
- **Shutdown**: Orden inverso a inicialización
- **Buffers internos**: Retornados como const punteros, válidos hasta siguiente llamada
