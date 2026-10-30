#!/usr/bin/env python3
"""Genera los efectos de sonido de Road Fighter, Snake, Space Invaders y Breakout.

Sintetiza ondas cuadradas, triangulares y ruido (el sonido de las consolas retro) y
escribe archivos WAV mono de 16 bits a 44100 Hz en <juego>/assets/, el mismo formato
de los sonidos de Tetris. No necesita librerias: solo Python.

    python3 TOOLS/generar_sonidos.py

Se puede ejecutar las veces que haga falta: el ruido usa una semilla fija, asi que
siempre salen los mismos archivos.
"""
import math
import os
import random
import struct
import wave

RATE = 44100
RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def onda(forma, fase, duty=0.5):
    """fase en vueltas (0..1)."""
    f = fase % 1.0
    if forma == "cuadrada":
        return 1.0 if f < duty else -1.0
    if forma == "triangular":
        return 4.0 * abs(f - 0.5) - 1.0
    if forma == "sierra":
        return 2.0 * f - 1.0
    return math.sin(2 * math.pi * f)


def barrido(f0, f1, dur, forma="cuadrada", vol=0.5, caida=4.0, duty=0.5, ataque=0.004):
    """Tono que va de f0 a f1 Hz con una envolvente que decae (caida = rapidez)."""
    n = int(dur * RATE)
    out, fase = [], 0.0
    for i in range(n):
        t = i / n
        f = f0 + (f1 - f0) * t
        fase += f / RATE
        env = math.exp(-caida * t)
        if i < ataque * RATE:
            env *= i / (ataque * RATE)
        if t > 0.9:                               # cierre suave: evita el "clic" al final
            env *= (1.0 - t) / 0.1
        out.append(onda(forma, fase, duty) * env * vol)
    return out


def tono(f, dur, **kw):
    return barrido(f, f, dur, **kw)


def ruido(dur, vol=0.5, caida=5.0, tono_bajo=0.0, semilla=1):
    """Ruido blanco con envolvente; tono_bajo (0..0.95) lo vuelve mas grave."""
    rnd = random.Random(semilla)
    n = int(dur * RATE)
    out, previo = [], 0.0
    for i in range(n):
        t = i / n
        v = rnd.uniform(-1.0, 1.0)
        previo = tono_bajo * previo + (1.0 - tono_bajo) * v
        env = math.exp(-caida * t)
        if t > 0.9:
            env *= (1.0 - t) / 0.1
        out.append(previo * env * vol)
    return out


def silencio(dur):
    return [0.0] * int(dur * RATE)


def unir(*partes):
    out = []
    for p in partes:
        out.extend(p)
    return out


def mezclar(*partes):
    n = max(len(p) for p in partes)
    out = [0.0] * n
    for p in partes:
        for i, v in enumerate(p):
            out[i] += v
    return out


def notas(lista, forma="cuadrada", vol=0.45, caida=2.5, duty=0.5):
    """lista de (frecuencia, duracion); frecuencia 0 = silencio."""
    return unir(*[silencio(d) if f == 0 else tono(f, d, forma=forma, vol=vol, caida=caida, duty=duty)
                  for f, d in lista])


def guardar(ruta, muestras, pico=0.8):
    mayor = max(1e-9, max(abs(v) for v in muestras))
    k = min(1.0, pico / mayor)
    datos = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v * k)) * 32767)) for v in muestras)
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with wave.open(ruta, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(datos)
    print(f"  {os.path.relpath(ruta, RAIZ)}  ({len(muestras) / RATE:.2f} s)")


# Notas (Hz)
DO4, RE4, MI4, FA4, SOL4, LA4, SI4 = 262, 294, 330, 349, 392, 440, 494
DO5, RE5, MI5, FA5, SOL5, LA5, SI5 = 523, 587, 659, 698, 784, 880, 988
DO6 = 1047


def menu_y_aceptar(prefijo, carpeta):
    guardar(f"{RAIZ}/{carpeta}/assets/{prefijo}_menu.wav", barrido(MI5, MI5 * 1.06, 0.05, vol=0.35, caida=3))
    guardar(f"{RAIZ}/{carpeta}/assets/{prefijo}_aceptar.wav", notas([(LA5, 0.05), (DO6, 0.09)], vol=0.38))


def road_fighter():
    c = "roadFighter"
    print(c)
    menu_y_aceptar("roadfighter", c)
    # semaforo de salida: tres pitidos graves y uno agudo
    guardar(f"{RAIZ}/{c}/assets/roadfighter_arranque.wav",
            notas([(DO5, 0.16), (0, 0.10), (DO5, 0.16), (0, 0.10), (DO5, 0.16), (0, 0.10), (DO6, 0.40)],
                  vol=0.45, caida=1.2))
    # un golpe de motor: se repite mas seguido cuanto mas rapido va el auto
    guardar(f"{RAIZ}/{c}/assets/roadfighter_motor.wav",
            mezclar(tono(58, 0.09, forma="sierra", vol=0.5, caida=3.5),
                    tono(116, 0.09, forma="triangular", vol=0.25, caida=4.0)))
    # choque: ruido grave y un golpe que cae
    guardar(f"{RAIZ}/{c}/assets/roadfighter_choque.wav",
            mezclar(ruido(0.55, vol=0.8, caida=4.0, tono_bajo=0.55, semilla=11),
                    barrido(180, 40, 0.5, forma="cuadrada", vol=0.5, caida=4.5)))
    # meta: fanfarria que sube
    guardar(f"{RAIZ}/{c}/assets/roadfighter_meta.wav",
            unir(notas([(DO5, 0.11), (MI5, 0.11), (SOL5, 0.11), (DO6, 0.22)], vol=0.42, caida=1.5),
                 notas([(SOL5, 0.11), (DO6, 0.50)], vol=0.42, caida=1.2)))


def snake():
    c = "snake"
    print(c)
    menu_y_aceptar("snake", c)
    # comer: dos notas rapidas hacia arriba
    guardar(f"{RAIZ}/{c}/assets/snake_comer.wav", notas([(SOL5, 0.05), (DO6, 0.09)], vol=0.42, caida=3.0))
    # morir: cae de agudo a grave
    guardar(f"{RAIZ}/{c}/assets/snake_morir.wav",
            unir(barrido(520, 90, 0.55, forma="cuadrada", vol=0.45, caida=1.4, duty=0.25),
                 ruido(0.12, vol=0.35, caida=6.0, tono_bajo=0.6, semilla=3)))
    # tablero lleno: la serpiente gano
    guardar(f"{RAIZ}/{c}/assets/snake_ganar.wav",
            notas([(DO5, 0.1), (MI5, 0.1), (SOL5, 0.1), (DO6, 0.1), (SOL5, 0.1), (DO6, 0.35)], vol=0.42, caida=1.5))


def space_invaders():
    c = "space_invaders"
    print(c)
    menu_y_aceptar("invaders", c)
    guardar(f"{RAIZ}/{c}/assets/invaders_disparo.wav",
            barrido(1500, 280, 0.16, forma="cuadrada", vol=0.4, caida=2.5, duty=0.3))
    guardar(f"{RAIZ}/{c}/assets/invaders_alien.wav",
            mezclar(ruido(0.16, vol=0.55, caida=6.0, tono_bajo=0.25, semilla=5),
                    barrido(400, 90, 0.16, vol=0.3, caida=5.0)))
    guardar(f"{RAIZ}/{c}/assets/invaders_ovni_golpe.wav",
            unir(barrido(300, 1200, 0.12, vol=0.4, caida=0.5),
                 barrido(1200, 200, 0.28, vol=0.4, caida=2.0),
                 ruido(0.1, vol=0.3, caida=6.0, semilla=9)))
    # el ovni: un gorjeo que sube y baja; suena cada pocos cuadros mientras cruza
    n = int(0.22 * RATE)
    fase, ovni = 0.0, []
    for i in range(n):
        t = i / n
        f = 700 + 260 * math.sin(2 * math.pi * 4.0 * t)
        fase += f / RATE
        env = min(1.0, i / (0.01 * RATE)) * (1.0 if t < 0.9 else (1.0 - t) / 0.1)
        ovni.append(onda("cuadrada", fase, 0.4) * env * 0.28)
    guardar(f"{RAIZ}/{c}/assets/invaders_ovni.wav", ovni)
    guardar(f"{RAIZ}/{c}/assets/invaders_nave.wav",
            mezclar(ruido(0.8, vol=0.8, caida=3.0, tono_bajo=0.7, semilla=21),
                    barrido(260, 40, 0.8, forma="cuadrada", vol=0.45, caida=3.0)))
    # la marcha de los aliens: cuatro golpes graves que bajan de tono, en orden
    for i, f in enumerate((62, 58, 54, 50), start=1):
        guardar(f"{RAIZ}/{c}/assets/invaders_marcha{i}.wav",
                mezclar(tono(f, 0.09, forma="cuadrada", vol=0.55, caida=4.0, duty=0.35),
                        tono(f * 2, 0.06, forma="triangular", vol=0.25, caida=6.0)))
    guardar(f"{RAIZ}/{c}/assets/invaders_oleada.wav",
            notas([(DO5, 0.08), (MI5, 0.08), (SOL5, 0.08), (DO6, 0.2)], vol=0.4, caida=1.8))
    guardar(f"{RAIZ}/{c}/assets/invaders_fin.wav",
            notas([(SOL4, 0.2), (MI4, 0.2), (DO4, 0.2), (SOL4 / 2, 0.55)], vol=0.45, caida=1.3, duty=0.25))


def breakout():
    c = "breakout"
    print(c)
    menu_y_aceptar("breakout", c)
    guardar(f"{RAIZ}/{c}/assets/breakout_paleta.wav", tono(330, 0.07, vol=0.45, caida=4.0))
    guardar(f"{RAIZ}/{c}/assets/breakout_pared.wav", tono(220, 0.05, vol=0.4, caida=4.0))
    # bloques: mas agudo cuanto mas arriba (mas puntos) esta la fila
    for i, f in enumerate((587, 740, 988), start=1):
        guardar(f"{RAIZ}/{c}/assets/breakout_bloque{i}.wav", tono(f, 0.08, vol=0.42, caida=3.0, duty=0.4))
    guardar(f"{RAIZ}/{c}/assets/breakout_lanzar.wav", barrido(250, 700, 0.1, vol=0.4, caida=1.5))
    guardar(f"{RAIZ}/{c}/assets/breakout_perder.wav",
            barrido(600, 70, 0.6, forma="cuadrada", vol=0.45, caida=1.8, duty=0.3))
    guardar(f"{RAIZ}/{c}/assets/breakout_nivel.wav",
            notas([(DO5, 0.09), (MI5, 0.09), (SOL5, 0.09), (DO6, 0.09), (MI5 * 2, 0.3)], vol=0.4, caida=1.5))
    guardar(f"{RAIZ}/{c}/assets/breakout_fin.wav",
            notas([(SOL4, 0.18), (MI4, 0.18), (DO4, 0.18), (SOL4 / 2, 0.5)], vol=0.45, caida=1.3, duty=0.25))


if __name__ == "__main__":
    print("Generando sonidos en", RAIZ)
    road_fighter()
    snake()
    space_invaders()
    breakout()
    print("Listo.")
