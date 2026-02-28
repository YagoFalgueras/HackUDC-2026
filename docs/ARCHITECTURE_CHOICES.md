# Misión: DOOM en un Satélite (MVP)

Aceptar este reto es lidiar más con pura ingeniería que con código: la latencia y el ancho de banda son nuestros peores enemigos.

Asumimos:

- Física pura (Órbita): Ir más lejos (como a GEO) dispara el ping por encima de los 500 ms. LEO nos da un ping jugable.
  tiempo=distancia/c=500km/3000s~1,67ms
- Ancho de Banda Limitado: Asumiremos (Downlink: 64 kbps | Uplink: 9.6 kbps): Realismo puro. Nos forzamos escenario complicado para comunicaciones satelitales baratas, obligándonos a optimizar.

- Hardware AArch64 (Procesador ARM Cortex-A53 Quad-Core): Mentalidad MVP. Descartamos la FPGA; daba puntos extra, pero se nos iba de scope (somos Matemático-Informáticos). Usar los múltiples núcleos del ARM nos permite separar el juego y la compresión en hilos distintos sin perder rendimiento. Además, podríamos dedicar 2 cores exclusivamente a estas tareas para aprovechar la localidad de la caché al máximo (ha quedado como TO-DO).

El Muro de Ladrillo: Los Números del Ancho de Banda

Antes de escribir una sola línea de código, tuvimos que hacer las matemáticas del desastre para entender el tamaño de nuestro "tubo" de datos UDP.

    Los números: Un enlace de 64 kbps equivale a 8,000 bytes por segundo. Para jugar a 20 FPS, nuestro límite absoluto es de 400 bytes por frame.

    El Peso Original: Reduciendo la pantalla a 160x100 píxeles en escala de grises (4 bits), un frame crudo pesa 8,000 bytes.

    El Reto: Necesitamos una compresión extrema (mínimo 20:1) en tiempo real.

Por qué fracasó la idea inicial (El colapso del XOR + RLE)

Nuestra primera teoría era usar un algoritmo diferencial (XOR) más compresión de repeticiones (RLE). Sobre el papel, funcionaba. En la práctica, el motor de DOOM lo destrozó.

La Teoría:
Si el jugador está quieto, el 96% de la pantalla no cambia. El algoritmo RLE agrupa esos píxeles estáticos fácilmente (muchos ceros consecurivos tras XOR), resultando en un frame comprimido de unos 300 bytes. Hasta aqui todo bien.

La Realidad (El baño de sangre):
DOOM es un FPS frenético construido sobre un motor raycaster antiguo.

    Movimiento lateral: Al girar un solo grado, el motor desplaza horizontalmente todas las columnas de la pantalla. El 90% de los píxeles cambian de valor al instante.

    Texturas dinámicas: Al caminar hacia adelante, el suelo y el techo se escalan, rompiendo cualquier patrón previo.

    Inflación de datos: Cuando el 80% de la pantalla es ruido en movimiento, el RLE ya no encuentra repeticiones. En lugar de comprimir, añade bytes de control extra a cada píxel.

## Conclusión Crítica

Durante el combate, el peso del frame mediante RLE salta de 300 bytes a más de 5,000 bytes, reventando nuestro presupuesto de 400 bytes. Esto satura el búfer UDP, provoca pérdida masiva de paquetes y genera un lag insuperable. El RLE está descratadi; la única salida viable para nuestro MVP es usar compresión de vídeo real (H.264) por software.

Nota: se valoraron otras alternativas para la compresión como compresión neuronal especializada para el Doom, aprovechando la naturaleza determinista de sus mapas y su paleta de colores fija. Sin embargo, fue descartada: la inferencia de redes neuronales en tiempo real (20 FPS) exige hardware dedicado (NPU | GPU | TPU).

## Más sobre el ancho de banda

Reducir el ancho de banda se reduce a 3 preguntas: qué se envía, cuando y cómo. Este último punto ya lo hemos tratado así que vayamos con los primeros dos. Doom corre con una resolución de 320x200 píxeles: lo primero es reducir esto. El tamaño mínimo configurable es de 96x48 aunque no es el más jugable.

En este punto, como buenos ingenieros,y gracias a la asunción de 64kbps para el downlink, pudimos trazar una analoǵia con otra tecnología que vivía bajo esta restricción: los algoritmos para las videollamadas 3G en los 2000. Estes usaban la bien conocida 176x144 (QCIF). Si ellos pudieron, nosotros también.

## Arquitectura

### Visión General del Sistema

El proyecto implementa una arquitectura cliente-servidor distribuida en dos componentes físicamente separados que se comunican mediante protocolos de red optimizados para enlaces satelitales de alta latencia y bajo ancho de banda.

### Componentes Principales

#### 1. Satélite (ARM Linux SoC)

El componente satelital ejecuta DOOM de forma headless y transmite vídeo comprimido. Su arquitectura se basa en **multithreading con separación de responsabilidades**:

**Hilo 1 - Motor DOOM (35 Hz, Core 0)**

- Ejecuta el motor doomgeneric modificado
- Corre a 35 Hz (tick rate nativo de DOOM, inamovible)
- Escribe frames RGB888 (176×144×3 bytes) a un **ring buffer lock-free compartido**
- Recibe input del jugador desde una cola thread-safe alimentada por el módulo uplink

**Hilo 2 - Encoder/Transmisión (20 FPS, Core 1)**

- Lee frames del ring buffer cuando están disponibles
- Codifica RGB888 → YUV420p → H.264 usando libx264 ([satellite/encoder.c](satellite/encoder.c))
- Empaqueta NAL units en RTP y transmite por UDP ([satellite/downlink.c](satellite/downlink.c))
- Corre a 20 FPS (reduce carga ~43% vs 35 FPS manteniendo jugabilidad)

**Ring Buffer Lock-Free** ([satellite/ringbuffer.c](satellite/ringbuffer.c))

- Comunicación productor-consumidor sin mutex en path crítico
- 3-4 slots de tamaño fijo (balance latencia/pérdida de frames)
- Estados atómicos: EMPTY → WRITING → READY → READING → EMPTY
- Permite que DOOM y encoder corran a velocidades diferentes sin bloqueos

**Módulo Uplink** ([satellite/uplink.h](satellite/uplink.h))

- Socket UDP non-blocking en puerto 5000 (configurable)
- Recibe paquetes de input de 8 bytes (bitfield de teclas serializado)
- Deserializa y alimenta cola consumida por DOOM

#### 2. Estación Terrestre (Ground Station)

El componente terrestre captura input del jugador y reconstruye/muestra el vídeo recibido:

**Módulo Input** ([ground/input.c](ground/input.c))

- Captura eventos SDL2 (teclado)
- Mapea teclas a bitfield de 16 bits (movimiento, disparo, cambio de arma, menús)
- Serializa input_packet_t (8 bytes: bitfield + timestamp + seq_number)
- Transmite por UDP solo cuando cambia el estado (optimización ancho de banda)

**Módulo Receiver** ([ground/receiver.c](ground/receiver.c))

- Socket UDP non-blocking en puerto 5001 (configurable)
- **Reensamblaje de fragmentos FU-A (RTP Fragmentation Unit type A)**:
  - NALs pequeños (≤ MTU): procesados directamente
  - NALs grandes (> MTU): acumulados en buffer hasta marker bit = 1
- **Decodificación H.264 con FFmpeg (libavcodec)**:
  - Wrapping de NAL completo en AVPacket
  - avcodec_send_packet() → avcodec_receive_frame()
  - Conversión YUV420p → RGB24 con libswscale (sws_scale)
- Retorna puntero a frame RGB decodificado (válido hasta próximo poll)

**Módulo Display** ([ground/display.c](ground/display.c))

- Ventana SDL2 escalada (704×576, 4x de QCIF para mantener aspect ratio)
- Textura RGB con nearest-neighbor para estética pixelada retro
- SDL_RENDERER_PRESENTVSYNC para sincronización vertical

### Protocolo de Comunicación

#### Downlink (Satélite → Tierra): Vídeo H.264 sobre RTP/UDP

**RTP (Real-time Transport Protocol)** se eligió sobre TCP por las siguientes razones críticas en entornos satelitales:

1. **Latencia predecible**: TCP retransmite paquetes perdidos, multiplicando RTT (30-40 ms LEO → 200+ ms con retransmisiones). RTP sobre UDP acepta pérdidas menores a cambio de latencia constante.

2. **Sin head-of-line blocking**: En TCP, un paquete perdido bloquea todos los subsecuentes hasta su retransmisión. En streaming de vídeo, un frame antiguo retransmitido es inútil (ya llegó el siguiente frame).

3. **Control de congestión inadecuado**: Los algoritmos de congestion control de TCP (CUBIC, BBR) asumen redes terrestres y degradan throughput innecesariamente ante la alta latencia satelital.

**Estructura del paquete RTP** ([common/include/protocol.h](common/include/protocol.h)):

```
[ RTP Header (12 bytes) ][ NAL Unit Payload (variable) ]
```

**RTP Header**:

- Sequence number (16 bits): detección de pérdidas y reordenamiento
- Timestamp (32 bits): reconstrucción de timing en receptor (90 kHz para H.264)
- Marker bit: señala fin de frame (último NAL del frame)
- SSRC: identificador de fuente (stateless, no requiere sesión)

**Fragmentación FU-A** (para NALs > MTU ~1400 bytes):

- Primer fragmento: FU indicator + FU header (bit S=1, Start) + payload
- Fragmentos intermedios: FU indicator + FU header + payload
- Último fragmento: FU indicator + FU header (bit E=1, End) + payload

Esto permite transmitir NALs grandes (típicamente I-frames) sin exceder MTU de Ethernet/IP, evitando fragmentación a nivel IP (que causaría pérdida total del NAL si se pierde un solo fragmento IP).

#### Uplink (Tierra → Satélite): Input del jugador

**Paquete de input** ([common/include/protocol.h](common/include/protocol.h:69-95)):

```c
typedef struct {
    uint16_t bitfield;    // Teclas activas (16 flags)
    uint32_t timestamp;   // ms monótonos desde inicio
    uint16_t seq_number;  // Contador de paquetes
} input_packet_t;  // Total: 8 bytes, big-endian
```

**Bitfield de 16 bits** codifica todas las acciones DOOM:

- Bits 0-9: Movimiento (forward, backward, turn left/right, strafe, run, strafe mode)
- Bit 4: Fire (Ctrl)
- Bit 5: Use/Open (Espacio)
- Bits 10-12: Weapon select (0-6)
- Bits 13-15: Menu navigation (Enter, Escape, Y)

**Optimizaciones**:

- Solo se transmite cuando cambia el bitfield (no flooding continuo)
- 8 bytes fijos permiten parsing ultra-rápido sin heap allocation
- Big-endian serialization para compatibilidad cross-platform

## Microoptimizaciones

## Resultados y conclusiones finales
