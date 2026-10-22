#!/usr/bin/env python3
"""Convierte los PNG de pong/assets a pong/include/assets.h (RGB565).

Uso (desde cualquier carpeta):
    python tools/png2assets.py

Formato igual al de roadFighter: arreglos `const uint16_t Nombre[ancho*alto]`.
El pixel transparente se escribe como 0x0843, el color que sendSprite() omite.
Ademas se emiten las macros Nombre_W y Nombre_H con el tamano de cada sprite.
Requiere Pillow (pip install pillow).
"""
import sys
from pathlib import Path

from PIL import Image

TRANSPARENTE = 0x0843
RAIZ = Path(__file__).resolve().parent.parent
ASSETS = RAIZ / "assets"
SALIDA = RAIZ / "include" / "assets.h"
ORDEN = ["Ball", "Escenario", "Player1", "Player2"] + [f"Num{i}" for i in range(10)]


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def convertir(png):
    im = Image.open(png).convert("RGBA")
    ancho, alto = im.size
    pixeles = []
    for y in range(alto):
        for x in range(ancho):
            r, g, b, a = im.getpixel((x, y))
            if a < 128:
                pixeles.append(TRANSPARENTE)
                continue
            v = rgb565(r, g, b)
            if v == TRANSPARENTE:  # que un color real no se confunda con transparente
                v = TRANSPARENTE - 1
            pixeles.append(v)
    return ancho, alto, pixeles


def main():
    partes = [
        "// Generado por tools/png2assets.py a partir de assets/*.png. No editar a mano.\n",
        "#include <stdint.h>\n#pragma once\n\n",
    ]
    for nombre in ORDEN:
        ruta = ASSETS / f"{nombre}.png"
        if not ruta.exists():
            sys.exit(f"Falta {ruta}")
        ancho, alto, pixeles = convertir(ruta)
        partes.append(f"#define {nombre}_W {ancho}\n#define {nombre}_H {alto}\n")
        partes.append(f"const uint16_t {nombre}[{ancho * alto}] = {{\n")
        for i in range(0, len(pixeles), ancho):
            fila = ", ".join(f"0x{p:04X}" for p in pixeles[i:i + ancho])
            partes.append(f"    {fila},\n")
        partes.append("};\n\n")
    SALIDA.write_text("".join(partes), encoding="utf-8", newline="\n")
    print(f"Escrito {SALIDA} ({len(ORDEN)} sprites)")


if __name__ == "__main__":
    main()
