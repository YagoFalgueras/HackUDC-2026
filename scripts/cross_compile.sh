#!/bin/bash
# cross_compile.sh — Cross-compilation del binario satellite para ARM aarch64
#
# Responsabilidad:
#   Compilar el software del satélite para arquitectura ARM (aarch64-linux-gnu)
#   usando un toolchain de cross-compilation.
#
# Pasos a implementar:
#   1. Verificar que el toolchain aarch64-linux-gnu-gcc está instalado
#   2. Configurar variables de entorno: CC, AR, STRIP, CFLAGS, LDFLAGS
#   3. Compilar libx264 para ARM (si no está precompilada)
#   4. Compilar doomgeneric para ARM
#   5. Compilar common/ (protocol, net_common, ring_buffer)
#   6. Compilar satellite/ (todos los módulos)
#   7. Linkear todo en el binario final: doom-satellite
#   8. Strip del binario para reducir tamaño
#   9. Reportar tamaño del binario final
#
# Uso:
#   ./scripts/cross_compile.sh [--toolchain-prefix=aarch64-linux-gnu-]

echo "TODO: Implementar cross-compilation para ARM aarch64"
