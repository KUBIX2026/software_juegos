# TOOLS

Herramientas del repositorio.

## converter.py

Convierte los PNG de una carpeta de assets en el archivo `include/assets.h` de un juego: un arreglo `const uint16_t` en formato RGB565 por cada sprite. La consola no lee PNG, lee esos números.

Requiere Python y Pillow (`pip install pillow`). Se ejecuta desde la raíz del repositorio:

```bash
python3 TOOLS/converter.py -i assets/snake -o snake/include/assets.h
python3 TOOLS/converter.py -i Tetris/assets -o Tetris/include/assets.h --macros
```

| Opción | Uso |
|---|---|
| `-i`, `--input` | Carpeta con los PNG (obligatoria). |
| `-o`, `--output` | Archivo `.h` de salida (por defecto `./include/assets.h`). |
| `--macros` | Agrega `Nombre_W` y `Nombre_H` con el tamaño de cada sprite (formato de Tetris y Pong). |

Cómo se comporta:

- **Reescribe el archivo completo**, cabecera incluida, así que se puede ejecutar las veces que haga falta.
- **Transparencia:** un píxel con alfa menor que 128, o del color `#0F081F` de la convención del repositorio, se escribe como `0x0843`, el color que `sendSprite()` omite.
- **Los sprites salen en orden alfabético** y el nombre del arreglo es el del archivo, que debe ser un identificador válido de C.
- **Si algo falla, no escribe nada:** una carpeta que no existe, un PNG ilegible, un nombre no válido, una carpeta sin PNG o un sprite de más de 255 píxeles por lado. Avisa con `ERROR:` y termina con código distinto de cero.
- **Conserva los saltos de línea** (CRLF o LF) del archivo de salida si ya existe, para que `git diff` solo muestre los sprites que cambiaron.
