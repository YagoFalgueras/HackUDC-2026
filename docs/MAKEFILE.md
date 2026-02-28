make # Compila satellite y ground para arquitectura nativa
make satellite # Solo satélite
make ground # Solo estación terrestre
make clean # Limpia todo
make info # Muestra configuración de compilación
make help # Ayuda completa
make test-local # Compila y muestra instrucciones para test local
make satellite CROSS=aarch64-linux-gnu-# Para compilar a ARM64/AARCH64
