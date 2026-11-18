<p align="center">
  <img src="gui/res/nlfork-logo.svg" alt="NLFork" width="420">
</p>

# NLFork

**NLFork** es un cliente de **PS5 Remote Play para Windows**, basado en [chiaki-ng](https://github.com/chiaki-ng/chiaki-ng). Su objetivo es menos input lag, una imagen más nítida y cero tirones.

## Descarga

👉 **[Descargar la última versión](https://github.com/CrisBuen/NLFork/releases/latest)**: baja el archivo `NLFork-...-windows-x64.zip`, descomprímelo completo y abre `NLFork.exe`. Las instrucciones paso a paso están en `LEEME.txt`, dentro del .zip.

> Si Windows muestra "Windows protegió tu PC", pulsa **Más información → Ejecutar de todas formas**. Aparece porque el programa no tiene firma digital de pago; todo el código está en este repositorio.

## Qué agrega a chiaki-ng

- **Perfiles de un clic**:
  - **Competitivo**: menor latencia, a 40 Mbps.
  - **Calidad**: escalado FSRCNNX, nitidez CAS suave y color más fino, a 100 Mbps.
- **Menos tirones**: un búfer de red de 4 MB evita las pérdidas a 100 Mbps, y un detector de tirones avisa si vienen de la red, del PC o del mando.
- **Estadísticas en pantalla** (botón *Estadísticas* en el menú del stream): ping, Mbps, pérdida de paquetes, tirones de red y tirones del PC.
- **Reconexión automática**: si la red se cae, NLFork reintenta hasta 3 veces. Nunca enciende la consola si está en reposo.
- **Shader `color-nitido`**: reconstruye el color (4:2:0) guiándose por la luminancia.
- **Corrección del congelamiento** que ocurría al pasar 10,7 días con el PC encendido: el reloj de Windows se desbordaba.
- **Interfaz rediseñada** al estilo consola, y **traducción al español** con cambio de idioma al instante.

## Requisitos

- Windows 10/11 de 64 bits y una GPU con Vulkan.
- PS5 con Remote Play activado, en la misma red que el PC (mejor con cable).

## Compilar desde el código

Se compila con MSYS2 (MINGW64). Los pasos son los mismos que en chiaki-ng; consulta [README-chiaki-ng.md](README-chiaki-ng.md) y la [documentación de chiaki-ng](https://chiaki-ng.github.io/chiaki-ng/).

## Créditos y licencia

- [chiaki-ng](https://github.com/chiaki-ng/chiaki-ng): Street Pea y colaboradores.
- [Chiaki](https://git.sr.ht/~thestr4ng3r/chiaki): Florian Markl.

NLFork se distribuye bajo la licencia **GNU AGPL-3.0** (ver [COPYING](COPYING) y [LICENSES](LICENSES)), igual que chiaki-ng.

NLFork no está afiliado a Sony Interactive Entertainment. "PlayStation" y "PS5" son marcas de Sony Interactive Entertainment.
