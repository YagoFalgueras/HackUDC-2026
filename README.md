# DOOM Satellite

Streaming de DOOM entre un satélite (ejecuta el juego) y una estación terrestre (recibe y muestra el vídeo) mediante UDP.

## Arquitectura

```
┌──────────────┐    UDP :9666    ┌──────────────┐
│   Satélite   │ ─────────────► │    Ground     │
│ (chocolate-  │  framebuffer   │  (receptor +  │
│    doom)     │  320x200 8-bit │   SDL2)       │
└──────────────┘                └──────────────┘
```

- **Satélite**: Chocolate Doom modificado que envía el framebuffer crudo por UDP en vez de renderizar en pantalla.
- **Ground**: Receptor SDL2 que escucha en el puerto 9666 y muestra los frames en una ventana.

## Dependencias

### Satélite
- SDL2, SDL2_mixer, SDL2_net
- Dependencias estándar de Chocolate Doom

### Ground
- SDL2 (`libsdl2-dev`)

## Compilación

### Satélite

```bash
cd satellite/doom
mkdir build && cd build
cmake ..
make
```

### Ground

```bash
gcc -o ground ground/src/main.c $(sdl2-config --cflags --libs)
```

## Ejecución

Primero lanzar el receptor (ground), luego el satélite.

### Ground (estación terrestre)

```bash
./ground
```

Escucha en `127.0.0.1:9666` por defecto.

### Satélite

```bash
./satellite/doom/src/chocolate-doom -iwad freedoom1.wad
```

## Controles

- `ESC` — Cerrar ground
- `Ctrl+C` — Cerrar cualquiera de los dos desde terminal
