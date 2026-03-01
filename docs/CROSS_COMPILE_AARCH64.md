# Cross-Compilación para AArch64

Guía para compilar y ejecutar el componente del satélite en arquitectura ARM64 (AArch64) desde un host x86_64, usando el cross-compiler de GNU y QEMU para emulación.

## Requisitos previos

- `gcc-aarch64-linux-gnu` — cross-compiler para AArch64
- `qemu-user-static` — emulador de user-mode para ejecutar binarios ARM64 en x86_64
- Librerías de desarrollo en versión arm64 (ver siguiente sección)

## 1. Configurar repositorios arm64

Ubuntu no incluye paquetes arm64 en los repositorios por defecto. Es necesario añadir los repos de Ubuntu Ports y actualizar:

```bash
sudo dpkg --add-architecture arm64

sudo tee /etc/apt/sources.list.d/arm64-ports.list << 'EOF'
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble main restricted universe multiverse
deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ noble-updates main restricted universe multiverse
EOF

sudo apt update
```

> **Nota:** Sustituye `noble` por tu versión de Ubuntu si es diferente (consulta con `lsb_release -cs`).

## 2. Instalar dependencias arm64

```bash
sudo apt install libsdl2-dev:arm64 libsdl2-mixer-dev:arm64 libsdl2-net-dev:arm64 \
                 libx264-dev:arm64 libpng-dev:arm64
```

## 3. Compilar el motor DOOM para AArch64

El motor DOOM usa autotools. Hay que reconfigurarlo apuntando al cross-compiler y al `pkg-config` de arm64:

```bash
cd satellite/doom
make clean
export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig

./configure --host=aarch64-linux-gnu \
  CC=aarch64-linux-gnu-gcc \
  CFLAGS="-O3" \
  --without-libsamplerate \
  --without-libpng \
  --without-fluidsynth

make
cd ../..
```

- `--host=aarch64-linux-gnu` indica a autotools que el target es ARM64.
- `PKG_CONFIG_PATH` apunta a las librerías arm64 instaladas en el paso anterior.
- Las opciones `--without-*` desactivan dependencias opcionales no necesarias para el satélite.

## 4. Compilar el componente del satélite

El Makefile principal soporta cross-compilación mediante la variable `CROSS`:

```bash
make clean
make satellite CROSS=aarch64-linux-gnu-
```

Esto prefija todas las invocaciones del compilador con `aarch64-linux-gnu-` (e.g., `aarch64-linux-gnu-gcc`).

## 5. Verificar el binario

```bash
file doom-satellite
```

Debe mostrar: `ELF 64-bit LSB executable, ARM aarch64, ...`

## 6. Ejecutar con QEMU

Para ejecutar el binario ARM64 en un host x86_64 usando emulación user-mode:

```bash
qemu-aarch64 -L /usr/aarch64-linux-gnu ./doom-satellite
```

- `-L` indica el sysroot donde QEMU buscará el linker dinámico (`ld-linux-aarch64.so.1`) y las librerías compartidas de arm64.

## Solución de problemas

### `configure` falla con "C compiler cannot create executables"

Es probable que `binfmt_misc` esté intentando ejecutar binarios aarch64 automáticamente pero falle. Desactívalo temporalmente:

```bash
sudo sh -c 'echo 0 > /proc/sys/fs/binfmt_misc/qemu-aarch64'
```

Y vuelve a ejecutar `./configure`.

### `apt update` no encuentra paquetes arm64

Verifica que el archivo `arm64-ports.list` usa la codename correcta de tu distribución (`lsb_release -cs`).

### Error de enlazado "Relocations in generic ELF (EM: 62)"

Los `.o` de DOOM están compilados para x86_64. Limpia y recompila DOOM para aarch64 (paso 3):

```bash
cd satellite/doom
find . -name "*.o" -delete
find . -name "*.a" -delete
```
