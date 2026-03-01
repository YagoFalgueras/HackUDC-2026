# DOOM Satellite Network Monitoring

Sistema de monitoreo en tiempo real para el tráfico de red del proyecto DOOM Satellite usando **Grafana**, **InfluxDB** y **Python**.

## Descripción

Este sistema captura y visualiza el tráfico de red en los puertos de comunicación del satélite y la estación terrestre:

- **Puerto 5000**: Uplink (comandos de input: Ground → Satellite)
- **Puerto 5001**: Downlink (stream de video H.264: Satellite → Ground)

## Características

- Captura de paquetes UDP en tiempo real usando `scapy`
- Cálculo de bandwidth (kbps), tasa de paquetes (pps) y totales acumulados
- Almacenamiento en InfluxDB (base de datos de series temporales)
- Visualización en dashboards interactivos de Grafana
- Gráficos en tiempo real con actualización cada 5 segundos
- Métricas separadas para uplink y downlink

## Requisitos

### Software

- **Python 3.8+**
- **Docker** y **Docker Compose**
- Privilegios de root (para captura de paquetes)

### Dependencias Python

Las dependencias se gestionan automáticamente con el script `monitoring.sh`:

- `influxdb==5.3.1` - Cliente de InfluxDB
- `scapy==2.5.0` - Captura y análisis de paquetes
- `psutil==5.9.8` - Utilidades del sistema

## Instalación

### Setup completo (recomendado)

El script `monitoring.sh` gestiona automáticamente:
- Creación del entorno virtual Python (venv)
- Instalación de dependencias
- Inicio del stack Docker (Grafana + InfluxDB)

```bash
cd monitoring
./monitoring.sh setup
```

Esto creará:
- Un entorno virtual en `monitoring/venv/`
- Contenedores Docker para InfluxDB y Grafana
- Base de datos `doom_satellite` en InfluxDB
- Dashboard precargado en Grafana

### Instalación manual (paso a paso)

Si prefieres hacerlo paso a paso:

```bash
# 1. Crear entorno virtual
./monitoring.sh venv

# 2. Instalar dependencias Python
./monitoring.sh install

# 3. Iniciar Docker stack
./monitoring.sh start

# 4. Verificar estado
./monitoring.sh status
```

## Uso

### Iniciar el monitor de red

El script `monitoring.sh` gestiona automáticamente el entorno virtual y los privilegios de root:

```bash
./monitoring.sh run
```

**Nota:** El script solicitará privilegios de sudo automáticamente para captura de paquetes.

### Parámetros avanzados

Para monitorizar una interfaz específica:

```bash
MONITOR_INTERFACE=eth0 ./monitoring.sh run
```

O modificar el script `monitor.py` directamente con el venv activo:

```bash
source venv/bin/activate
sudo -E venv/bin/python3 monitor.py -i eth0 --interval 0.5
```

### Abrir dashboard de Grafana

```bash
./monitoring.sh dashboard
```

O accede manualmente a: http://localhost:3000

**Credenciales por defecto:**
- Usuario: `admin`
- Contraseña: `admin`

El dashboard "DOOM Satellite Network Monitor" se cargará automáticamente.

## Dashboard de Grafana

El dashboard incluye:

### 📊 Gráficos principales

1. **Bandwidth (kbps)** - Serie temporal del ancho de banda
   - Línea azul: Downlink (video)
   - Línea verde: Uplink (input)
   - Incluye valores promedio, máximo y actual

2. **Packet Rate (packets/second)** - Tasa de paquetes
   - Muestra pps para uplink y downlink

3. **Current Downlink/Uplink** - Gauges en tiempo real
   - Indicadores visuales del ancho de banda actual
   - Umbrales de color (verde/amarillo/rojo)

4. **Total Data Transferred** - Gráfico de dona
   - Distribución porcentual del tráfico total

5. **Total Packets** - Contadores acumulados
   - Total de paquetes transmitidos desde el inicio

### 🎯 Valores esperados

Según las especificaciones del proyecto:

| Métrica | Valor esperado |
|---------|----------------|
| Downlink (video) | 128-256 kbps |
| Uplink (input) | < 5 kbps (solo cambios de teclas) |
| FPS de video | ~20 FPS |
| Resolución | 176×144 (QCIF) |

## Flujo de trabajo completo

### Escenario: Test local del proyecto DOOM Satellite

```bash
# Terminal 1: Setup inicial (primera vez)
cd monitoring
./monitoring.sh setup

# Terminal 2: Ejecutar monitor de red
./monitoring.sh run

# Terminal 3: Ejecutar satélite
cd ..
./doom-satellite

# Terminal 4: Ejecutar ground station
./doom-ground

# Navegador: Abrir Grafana
# Ejecuta en cualquier terminal:
cd monitoring
./monitoring.sh dashboard
```

Luego visualiza el tráfico en tiempo real en http://localhost:3000

## Comandos disponibles

El script `monitoring.sh` proporciona los siguientes comandos:

### Setup
```bash
./monitoring.sh setup         # Setup completo (venv + deps + docker)
./monitoring.sh venv          # Solo crear entorno virtual
./monitoring.sh install       # Solo instalar dependencias
./monitoring.sh start         # Solo iniciar Docker stack
```

### Runtime
```bash
./monitoring.sh run           # Ejecutar monitor (gestiona venv y sudo)
./monitoring.sh stop          # Detener Docker stack
./monitoring.sh status        # Ver estado de servicios
./monitoring.sh logs          # Ver logs de contenedores
./monitoring.sh dashboard     # Abrir Grafana en navegador
```

### Cleanup
```bash
./monitoring.sh clean         # Limpiar datos de monitoreo
./monitoring.sh clean-venv    # Eliminar entorno virtual
./monitoring.sh clean-all     # Limpiar todo (datos + venv)
```

### Ayuda
```bash
./monitoring.sh help          # Mostrar ayuda completa
```

## Estructura de archivos

```
monitoring/
├── monitoring.sh                     # Script principal de gestión
├── README.md                         # Esta documentación
├── requirements.txt                  # Dependencias Python
├── docker-compose.yml                # Configuración de Docker
├── monitor.py                        # Script de captura de tráfico
├── influxdb-init.sh                  # Script de inicialización de InfluxDB
├── grafana-dashboard.json            # Dashboard preconfigurable
├── grafana-provisioning/             # Configuración de Grafana
│   ├── datasources/
│   │   └── influxdb.yml              # Datasource de InfluxDB
│   └── dashboards/
│       └── dashboard.yml             # Provisioning de dashboards
└── venv/                             # Entorno virtual (creado por setup)
```

## Métricas capturadas

El script `monitor.py` captura las siguientes métricas y las exporta a InfluxDB:

### Measurement: `bandwidth`

| Campo | Tipo | Descripción |
|-------|------|-------------|
| `bytes_per_sec` | float | Bytes transmitidos por segundo |
| `kbps` | float | Kilobits por segundo |
| `packets_per_sec` | float | Paquetes por segundo |
| `total_bytes` | float | Total acumulado de bytes |
| `total_packets` | int | Total acumulado de paquetes |

### Tags

- `direction`: `uplink` o `downlink`
- `port`: `5000` (uplink) o `5001` (downlink)

## Troubleshooting

### Error: "Permission denied" al ejecutar monitor

El script `monitoring.sh run` gestiona automáticamente los privilegios de sudo. Si aún así falla:

```bash
# El script debería solicitar sudo automáticamente
./monitoring.sh run

# Si fallas persisten, ejecuta manualmente con venv:
source venv/bin/activate
sudo -E venv/bin/python3 monitor.py -i lo
```

### Error: "Cannot connect to InfluxDB"

Asegúrate de que los contenedores estén corriendo:

```bash
./monitoring.sh status
./monitoring.sh start
```

### Error: "Virtual environment not found"

Ejecuta el setup para crear el entorno virtual:

```bash
./monitoring.sh setup
# O solo crear el venv:
./monitoring.sh venv
```

### Los contenedores no inician

Verifica que Docker esté corriendo y que los puertos 3000 y 8086 estén libres:

```bash
docker ps
sudo netstat -tulpn | grep -E ':(3000|8086)'
```

### No se ven datos en Grafana

1. Verifica que el monitor esté corriendo: `./monitoring.sh run`
2. Verifica que satellite y ground estén transmitiendo datos
3. Comprueba que el datasource de InfluxDB esté configurado correctamente en Grafana
4. Revisa los logs: `./monitoring.sh logs`
5. Verifica el estado: `./monitoring.sh status`

### Limpiar todo y empezar de cero

```bash
./monitoring.sh stop
./monitoring.sh clean-all
./monitoring.sh setup
```

## Ejemplos de consultas InfluxDB

Puedes consultar InfluxDB directamente desde la línea de comandos:

```bash
# Acceder al contenedor de InfluxDB
docker exec -it doom-satellite-influxdb influx

# Dentro de InfluxDB CLI
USE doom_satellite;

# Ver últimas métricas de downlink
SELECT * FROM bandwidth WHERE direction='downlink' ORDER BY time DESC LIMIT 10;

# Promedio de bandwidth en los últimos 5 minutos
SELECT MEAN(kbps) FROM bandwidth WHERE direction='downlink' AND time > now() - 5m;

# Total de bytes transferidos
SELECT LAST(total_bytes) FROM bandwidth GROUP BY direction;
```

## Notas de desarrollo

- El script usa **scapy** para captura de paquetes a nivel raw
- Los contadores son thread-safe usando `threading.Lock`
- El intervalo de muestreo es configurable (default: 1 segundo)
- Los datos se retienen durante 7 días (configurable en `influxdb-init.sh`)
- El dashboard se actualiza automáticamente cada 5 segundos

## Referencias

- [Documentación del proyecto](../CLAUDE.md)
- [Arquitectura completa](../docs/PROJECT_STRUCTURE.md)
- [Grafana Documentation](https://grafana.com/docs/)
- [InfluxDB 1.8 Documentation](https://docs.influxdata.com/influxdb/v1.8/)
- [Scapy Documentation](https://scapy.readthedocs.io/)
