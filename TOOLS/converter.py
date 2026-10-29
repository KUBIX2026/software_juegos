#!/usr/bin/env python3
"""Convierte los PNG de una carpeta en arreglos C RGB565 para include/assets.h.

Uso, desde la raiz del repositorio (requiere Pillow: pip install pillow):

    python3 TOOLS/converter.py -i assets/snake -o snake/include/assets.h
    python3 TOOLS/converter.py -i Tetris/assets -o Tetris/include/assets.h --macros

Cada PNG genera `const uint16_t Nombre[ancho*alto]`, en orden alfabetico. El
archivo de salida se reescribe completo (cabecera incluida), asi que se puede
ejecutar las veces que haga falta.

Transparencia: un pixel con alfa < 128, o del color #0F081F de la convencion del
repositorio, se escribe como 0x0843, el color que sendSprite() omite.

Con --macros se emiten ademas `Nombre_W` y `Nombre_H` (formato de Tetris y Pong).
Si algun PNG falla, o la carpeta no existe, no se escribe nada y el programa
termina con codigo distinto de cero.
"""
import argparse
import os
import re
import sys

from PIL import Image

TRANSPARENTE = 0x0843
MAX_LADO = 255  # sendSprite() recibe ancho y alto como uint8_t
IDENTIFICADOR = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
CABECERA = "// Generado por TOOLS/converter.py a partir de assets/*.png. No editar a mano.\n"


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def convertir(ruta):
    img = Image.open(ruta).convert("RGBA")
    ancho, alto = img.size
    if ancho > MAX_LADO or alto > MAX_LADO:
        raise ValueError(f"{ancho}x{alto} supera el maximo de {MAX_LADO} pixeles por lado")
    pixeles = []
    for y in range(alto):
        for x in range(ancho):
            r, g, b, a = img.getpixel((x, y))
            pixeles.append(TRANSPARENTE if a < 128 else rgb565(r, g, b))
    return ancho, alto, pixeles


def generar(carpeta, macros):
    """Devuelve (texto, errores, cantidad de sprites)."""
    partes = [CABECERA, "#include <stdint.h>\n#pragma once\n\n"]
    errores = []
    cantidad = 0
    for archivo in sorted(os.listdir(carpeta)):
        if not archivo.lower().endswith(".png"):
            continue
        nombre = os.path.splitext(archivo)[0]
        if not IDENTIFICADOR.match(nombre):
            errores.append(f"{archivo}: el nombre no es un identificador valido de C")
            continue
        try:
            ancho, alto, pixeles = convertir(os.path.join(carpeta, archivo))
        except Exception as error:
            errores.append(f"{archivo}: {error}")
            continue
        cantidad += 1
        if macros:
            partes.append(f"#define {nombre}_W {ancho}\n#define {nombre}_H {alto}\n")
        partes.append(f"const uint16_t {nombre}[{ancho * alto}] = {{\n")
        for i in range(0, len(pixeles), ancho):
            fila = pixeles[i:i + ancho]
            if macros:
                partes.append("    " + ", ".join(f"0x{p:04X}" for p in fila) + ",\n")
            else:
                partes.append("    " + "".join(f"0x{p:04X}, " for p in fila) + "\n")
        partes.append("};\n\n")
    return "".join(partes), errores, cantidad


def escribir(ruta, texto):
    """Reescribe el archivo conservando sus saltos de linea (CRLF o LF)."""
    salto = "\n"
    if os.path.exists(ruta):
        with open(ruta, "rb") as f:
            if b"\r\n" in f.read():
                salto = "\r\n"
    with open(ruta, "w", encoding="utf-8", newline="") as f:
        f.write(texto.replace("\n", salto))


def main():
    parser = argparse.ArgumentParser(description="Convierte los PNG de una carpeta en arreglos C RGB565.")
    parser.add_argument("-i", "--input", required=True, help="Carpeta con los PNG.")
    parser.add_argument("-o", "--output", default="./include/assets.h",
                        help="Archivo .h de salida (por defecto: ./include/assets.h).")
    parser.add_argument("--macros", action="store_true", help="Agregar las macros Nombre_W y Nombre_H.")
    args = parser.parse_args()

    if not os.path.isdir(args.input):
        print(f"ERROR: la carpeta '{args.input}' no existe.", file=sys.stderr)
        return 2
    texto, errores, cantidad = generar(args.input, args.macros)
    if cantidad == 0 and not errores:
        errores.append(f"'{args.input}' no tiene archivos PNG")
    if errores:
        for error in errores:
            print(f"ERROR: {error}", file=sys.stderr)
        print(f"No se escribio '{args.output}'.", file=sys.stderr)
        return 1
    escribir(args.output, texto)
    print(f"Escrito {args.output} ({cantidad} sprites)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
