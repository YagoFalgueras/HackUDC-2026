#!/bin/bash
# run_local_test.sh — Test completo del pipeline en localhost
#
# Responsabilidad:
#   Ejecutar satélite + estación terrestre en la misma máquina,
#   usando tc/netem de Linux para simular condiciones del enlace satelital.
#
# Pasos a implementar:
#   1. Compilar satélite y ground (make all)
#   2. Configurar tc/netem en interfaz loopback para simular:
#      - Latencia: 30-40ms (LEO round-trip)
#      - Jitter: 5ms
#      - Pérdida de paquetes: 1-2%
#      - Ancho de banda limitado: 512 kbps
#   3. Lanzar proceso satélite en background (localhost, con WAD de prueba)
#   4. Lanzar proceso ground en foreground (conectando a localhost)
#   5. Al cerrar ground (Ctrl+C), matar proceso satélite
#   6. Limpiar reglas tc/netem
#   7. Mostrar estadísticas de ambos procesos
#
# Uso:
#   ./scripts/run_local_test.sh [--latency=35] [--loss=1] [--bandwidth=512]

echo "TODO: Implementar test local con simulación de enlace satelital"
