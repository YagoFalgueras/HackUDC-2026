# Makefile para DOOM Satellite Project
# Compila tanto el componente del satélite como el de la estación terrestre
# NOTA: DOOM se compila por separado

# ============================================================================
# Configuración del compilador y flags
# ============================================================================

# Compilador forzado a gcc
CC = gcc
CROSS ?=

# Flags del compilador (optimización O3)
CFLAGS = -Wall -Wextra -std=c99 -O3 -g
CPPFLAGS = -I.

# Flags específicas de debug/release
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS = -Wall -Wextra -std=c99 -DDEBUG -O0 -g
endif

# Flags del linker
LDFLAGS =
LIBS_PTHREAD = -lpthread
LIBS_MATH = -lm

# ============================================================================
# Directorios
# ============================================================================

BUILD_DIR = build
SATELLITE_DIR = satellite
GROUND_DIR = ground

# Directorios de salida
SATELLITE_BUILD = $(BUILD_DIR)/satellite
GROUND_BUILD = $(BUILD_DIR)/ground

# ============================================================================
# Archivos fuente - SATELLITE
# ============================================================================

SATELLITE_SOURCES = \
	$(SATELLITE_DIR)/main.c \
	$(SATELLITE_DIR)/encoder.c \
	$(SATELLITE_DIR)/downlink.c \
	$(SATELLITE_DIR)/uplink.c \
	$(SATELLITE_DIR)/ringbuffer.c

SATELLITE_OBJECTS = $(patsubst $(SATELLITE_DIR)/%.c,$(SATELLITE_BUILD)/%.o,$(SATELLITE_SOURCES))

# ============================================================================
# Archivos fuente - GROUND
# ============================================================================

GROUND_SOURCES = \
	$(GROUND_DIR)/main.c \
	$(GROUND_DIR)/display.c \
	$(GROUND_DIR)/receiver.c \
	$(GROUND_DIR)/input.c

GROUND_OBJECTS = $(patsubst $(GROUND_DIR)/%.c,$(GROUND_BUILD)/%.o,$(GROUND_SOURCES))

# ============================================================================
# Dependencias externas
# ============================================================================

# Librerías para SATELLITE
# Incluye librerías estáticas de DOOM (Chocolate Doom)
DOOM_DIR = $(SATELLITE_DIR)/doom

# Recoger todos los .o de DOOM excepto i_main.o (que contiene su propio main())
# y z_zone.o (que conflictúa con z_native.o)
DOOM_OBJS = $(filter-out %/i_main.o %/z_zone.o %/w_file_win32.o %/d_dedicated.o %/setup_*.o, $(wildcard $(DOOM_DIR)/src/*.o))
# Orden correcto: objetos primero, luego librerías estáticas
# Usar --start-group/--end-group para resolver referencias circulares
DOOM_STATIC_LIBS = \
	-Wl,--start-group \
	$(DOOM_OBJS) \
	$(DOOM_DIR)/src/doom/libdoom.a \
	$(DOOM_DIR)/textscreen/libtextscreen.a \
	$(DOOM_DIR)/pcsound/libpcsound.a \
	$(DOOM_DIR)/opl/libopl.a \
	-Wl,--end-group

# Librerías del sistema necesarias para DOOM
SATELLITE_LIBS = $(LIBS_PTHREAD) $(LIBS_MATH) -lx264 -lSDL2 -lSDL2_mixer -lSDL2_net -lpng -lsamplerate -lfluidsynth
SATELLITE_CFLAGS = $(CFLAGS) $(CPPFLAGS) -I$(SATELLITE_DIR) -I$(DOOM_DIR)/src

# Librerías para GROUND (SDL2 y FFmpeg)
GROUND_LIBS = -lSDL2 -lavcodec -lavutil -lswscale $(LIBS_MATH)
GROUND_CFLAGS = $(CFLAGS) $(CPPFLAGS) -I$(GROUND_DIR) \
	$(shell pkg-config --cflags sdl2 libavcodec libavutil libswscale 2>/dev/null)

# Nota: Si pkg-config no funciona, se pueden especificar manualmente:
# GROUND_CFLAGS += -I/usr/include/SDL2 -I/usr/include/ffmpeg

# ============================================================================
# Targets principales
# ============================================================================

.PHONY: all clean satellite ground test-local help dirs info

# Default target
all: satellite ground

# Target de ayuda
help:
	@echo "DOOM Satellite - Makefile"
	@echo ""
	@echo "Targets disponibles:"
	@echo "  all          - Compila satellite y ground (default)"
	@echo "  satellite    - Compila solo el componente del satélite"
	@echo "  ground       - Compila solo la estación terrestre"
	@echo "  clean        - Limpia todos los archivos compilados"
	@echo "  test-local   - Ejecuta prueba local (ambos procesos en localhost)"
	@echo "  info         - Muestra información de configuración"
	@echo "  help         - Muestra esta ayuda"
	@echo ""
	@echo "Variables de configuración:"
	@echo "  DEBUG=1            - Compilar en modo debug (O0)"
	@echo "  CROSS=<prefix>     - Cross-compilación (ej: CROSS=aarch64-linux-gnu-)"
	@echo ""
	@echo "Compilador: gcc (forzado)"
	@echo "Optimización: -O3 (o -O0 si DEBUG=1)"
	@echo "Arquitectura por defecto: Nativa (x86_64, ARM, etc. según tu sistema)"
	@echo ""
	@echo "NOTA: DOOM se compila por separado en satellite/doom/"
	@echo ""
	@echo "Ejemplos:"
	@echo "  make                                    # Compila todo para arquitectura nativa"
	@echo "  make satellite                          # Solo satélite"
	@echo "  make DEBUG=1                            # Modo debug con -O0"
	@echo "  make satellite CROSS=aarch64-linux-gnu- # Cross-compile para ARM64/AARCH64"
	@echo "  make clean                              # Limpia todo"

# ============================================================================
# Binarios finales
# ============================================================================

SATELLITE_BIN = doom-satellite
GROUND_BIN = doom-ground

satellite: dirs $(SATELLITE_BIN)

ground: dirs $(GROUND_BIN)

# ============================================================================
# Reglas de compilación - SATELLITE
# ============================================================================

$(SATELLITE_BIN): $(SATELLITE_OBJECTS)
	@echo "Linking satellite binary: $@"
	$(CROSS)$(CC) $(LDFLAGS) -o $@ $^ $(DOOM_STATIC_LIBS) $(SATELLITE_LIBS)
	@echo "✓ Satellite binary created: $@"

$(SATELLITE_BUILD)/%.o: $(SATELLITE_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling satellite: $<"
	$(CROSS)$(CC) $(SATELLITE_CFLAGS) -c $< -o $@

# ============================================================================
# Reglas de compilación - GROUND
# ============================================================================

$(GROUND_BIN): $(GROUND_OBJECTS)
	@echo "Linking ground binary: $@"
	$(CROSS)$(CC) $(LDFLAGS) -o $@ $^ $(GROUND_LIBS)
	@echo "✓ Ground binary created: $@"

$(GROUND_BUILD)/%.o: $(GROUND_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling ground: $<"
	$(CROSS)$(CC) $(GROUND_CFLAGS) -c $< -o $@

# ============================================================================
# Utilidades
# ============================================================================

# Crear directorios de build
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(SATELLITE_BUILD)
	@mkdir -p $(GROUND_BUILD)

# Limpiar archivos compilados
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f $(SATELLITE_BIN) $(GROUND_BIN)
	@echo "✓ Clean complete"

# ============================================================================
# Testing local
# ============================================================================

# Test local: ejecuta satellite y ground en localhost
# Usa 127.0.0.1 para comunicación
test-local: all
	@echo "========================================="
	@echo "Test Local - DOOM Satellite"
	@echo "========================================="
	@echo ""
	@echo "Instrucciones:"
	@echo "1. En una terminal, ejecuta: ./$(SATELLITE_BIN)"
	@echo "2. En otra terminal, ejecuta: ./$(GROUND_BIN)"
	@echo ""
	@echo "Para simular latencia satelital, usa tc netem:"
	@echo "  sudo tc qdisc add dev lo root netem delay 20ms"
	@echo ""
	@echo "Para eliminar la simulación de latencia:"
	@echo "  sudo tc qdisc del dev lo root"
	@echo ""
	@echo "Binarios creados:"
	@echo "  - ./$(SATELLITE_BIN)"
	@echo "  - ./$(GROUND_BIN)"
	@echo "========================================="

# ============================================================================
# Información del sistema
# ============================================================================

info:
	@echo "============================================"
	@echo "DOOM Satellite - Build Configuration"
	@echo "============================================"
	@echo ""
	@echo "Compilador:"
	@echo "  CC           = $(CROSS)$(CC)"
	@echo "  CFLAGS       = $(CFLAGS)"
	@echo "  CPPFLAGS     = $(CPPFLAGS)"
	@echo "  DEBUG        = $(DEBUG)"
	@echo ""
	@echo "Arquitectura:"
	@echo "  Target       = $(shell $(CROSS)$(CC) -dumpmachine 2>/dev/null || echo 'unknown')"
	@echo "  Cross-prefix = $(if $(CROSS),$(CROSS),ninguno - compilación nativa)"
	@echo ""
	@echo "Satellite:"
	@echo "  Fuentes      = $(words $(SATELLITE_SOURCES)) archivos"
	@echo "  CFLAGS       = $(SATELLITE_CFLAGS)"
	@echo "  LIBS         = $(SATELLITE_LIBS)"
	@echo "  Binary       = $(SATELLITE_BIN)"
	@echo ""
	@echo "Ground:"
	@echo "  Fuentes      = $(words $(GROUND_SOURCES)) archivos"
	@echo "  CFLAGS       = $(GROUND_CFLAGS)"
	@echo "  LIBS         = $(GROUND_LIBS)"
	@echo "  Binary       = $(GROUND_BIN)"
	@echo ""
	@echo "NOTA: DOOM se compila por separado"
	@echo "============================================"

# ============================================================================
# Dependencias automáticas (opcional, para rebuild automático)
# ============================================================================

# Genera archivos .d con las dependencias de headers
-include $(SATELLITE_OBJECTS:.o=.d)
-include $(GROUND_OBJECTS:.o=.d)

# Regla para generar dependencias
%.d: %.c
	@$(CROSS)$(CC) $(CPPFLAGS) -MM -MT $(@:.d=.o) $< > $@