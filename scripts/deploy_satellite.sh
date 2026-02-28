#!/bin/bash
# deploy_satellite.sh — Despliegue al target ARM (satélite o placa de desarrollo)
#
# Responsabilidad:
#   Copiar el binario compilado y archivos necesarios al hardware ARM
#   vía SSH/SCP y opcionalmente ejecutar.
#
# Pasos a implementar:
#   1. Verificar que el binario doom-satellite existe (compilado con cross_compile.sh)
#   2. Verificar conectividad SSH con el target
#   3. Crear directorio de destino en el target
#   4. Copiar: binario doom-satellite, WAD file, satellite.conf, perfil de encoder
#   5. Configurar permisos de ejecución
#   6. Opcionalmente: ejecutar el binario en el target vía SSH
#   7. Opcionalmente: configurar como servicio systemd para inicio automático
#
# Uso:
#   ./scripts/deploy_satellite.sh --target=usuario@ip --wad=/path/to/doom1.wad

echo "TODO: Implementar despliegue al target ARM"
