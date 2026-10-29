# Compila en Windows los 6 juegos, el menu de KUBIX y la app de escritorio (KUBIX.exe).
# No necesita CMake: solo gcc (MinGW) y la biblioteca SDL2.
#
# Uso (doble clic en compilar_windows.bat, o desde PowerShell en la raiz del repositorio):
#   .\TOOLS\compilar_windows.ps1
#   .\TOOLS\compilar_windows.ps1 -Sdl2Dir C:\ruta\SDL2-2.32.10
#   .\TOOLS\compilar_windows.ps1 -AccesoDirecto      # tambien crea el icono KUBIX en el escritorio
param(
    [string]$Sdl2Dir = $env:SDL2_DIR,
    [switch]$AccesoDirecto,
    [switch]$SinLanzador,
    [switch]$NoPausa
)

$raiz = Split-Path -Parent $PSScriptRoot
$resultados = @()

function Terminar($codigo) {
    if (-not $NoPausa) { Write-Host ""; Read-Host "Presiona Enter para cerrar" | Out-Null }
    exit $codigo
}

# --- gcc
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
if (-not $gcc) {
    Write-Host "ERROR: no se encontro gcc. Instala MinGW (o MSYS2) y agrega su carpeta bin al PATH." -ForegroundColor Red
    Terminar 1
}
$maquina = (& gcc -dumpmachine).Trim()
$arq = if ($maquina -match '^x86_64') { 'x86_64-w64-mingw32' } else { 'i686-w64-mingw32' }
Write-Host "Compilador: $($gcc.Source) ($maquina)"

# --- SDL2: carpeta que contiene include\SDL2\SDL.h, lib\ y bin\SDL2.dll
function Buscar-Sdl2 {
    $bases = @()
    if ($Sdl2Dir) { $bases += $Sdl2Dir }
    foreach ($patron in @("$HOME\dev\SDL2-*", "$HOME\SDL2*", "C:\SDL2*", "C:\dev\SDL2*", "C:\libs\SDL2*")) {
        $bases += @(Get-ChildItem -Path $patron -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | ForEach-Object { $_.FullName })
    }
    foreach ($b in $bases) {
        foreach ($c in @((Join-Path $b $arq), $b)) {
            if (Test-Path (Join-Path $c 'include\SDL2\SDL.h')) { return $c }
        }
    }
    return $null
}
$sdl = Buscar-Sdl2
if (-not $sdl) {
    Write-Host "ERROR: no se encontro SDL2 para $arq." -ForegroundColor Red
    Write-Host "Descarga SDL2-devel-...-mingw.zip de libsdl.org, descomprimelo y ejecuta:"
    Write-Host "  .\TOOLS\compilar_windows.ps1 -Sdl2Dir C:\ruta\a\SDL2-2.x.x"
    Terminar 1
}
Write-Host "SDL2:       $sdl"
Write-Host ""
$sdlFw = $sdl.Replace('\', '/')

# --- juegos: carpeta, ejecutable, fuentes
$proyectos = @(
    @{ Nombre = 'Road Fighter';   Dir = 'roadFighter';    Exe = 'roadFighter';    Fuentes = @('src/main.c') },
    @{ Nombre = 'Space Invaders'; Dir = 'space_invaders'; Exe = 'space_invaders'; Fuentes = @('src/main.c') },
    @{ Nombre = 'Snake';          Dir = 'snake';          Exe = 'snake';          Fuentes = @('src/main.c') },
    @{ Nombre = 'Pong';           Dir = 'pong';           Exe = 'pong';           Fuentes = @('src/main.c', 'src/pong_logica.c') },
    @{ Nombre = 'Tetris';         Dir = 'Tetris';         Exe = 'tetris';         Fuentes = @('src/main.c', 'src/tetris_logica.c') },
    @{ Nombre = 'Breakout';       Dir = 'breakout';       Exe = 'breakout';       Fuentes = @('src/main.c') },
    @{ Nombre = 'Menu KUBIX';     Dir = 'menu';           Exe = 'kubix';          Fuentes = @('src/main.c'); Extra = @('-DKUBIX_ROOT=\"../..\"') }
)

foreach ($p in $proyectos) {
    $carpeta = Join-Path $raiz $p.Dir
    $build = Join-Path $carpeta 'build'
    New-Item -ItemType Directory -Force -Path $build | Out-Null
    $salida = Join-Path $build ($p.Exe + '.exe')
    if (Test-Path $salida) { Remove-Item $salida -Force }

    $argumentos = @('-std=gnu11', '-O2')
    if ($p.Extra) { $argumentos += $p.Extra }
    $carpetaFw = $carpeta.Replace('\', '/')   # en el archivo de argumentos de gcc, la barra invertida es un escape
    $argumentos += @("-I`"$carpetaFw/include`"", "-I`"$sdlFw/include`"", "-I`"$sdlFw/include/SDL2`"")
    foreach ($f in $p.Fuentes) { $argumentos += "`"$carpetaFw/$f`"" }
    $argumentos += @("-L`"$sdlFw/lib`"", '-lmingw32', '-lSDL2main', '-lSDL2', '-o', "`"$salida`"".Replace('\', '/'))
    $rsp = Join-Path $build 'gcc.rsp'
    [System.IO.File]::WriteAllText($rsp, ($argumentos -join "`n"), (New-Object System.Text.UTF8Encoding($false)))

    Write-Host ("Compilando {0,-15} " -f $p.Nombre) -NoNewline
    $ErrorActionPreference = 'Continue'
    $log = & gcc "@$($rsp.Replace('\', '/'))" 2>&1
    $codigo = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($codigo -eq 0 -and (Test-Path $salida)) {
        Copy-Item (Join-Path $sdl 'bin\SDL2.dll') $build -Force
        Remove-Item $rsp -Force
        Write-Host "OK" -ForegroundColor Green
        $resultados += $true
    } else {
        Write-Host "FALLO" -ForegroundColor Red
        $log | Where-Object { $_ -match 'error' } | Select-Object -First 5 | ForEach-Object { Write-Host "   $_" }
        $resultados += $false
    }
}

# --- app de escritorio
if (-not $SinLanzador) {
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (-not (Test-Path $csc)) { $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe' }
    $exeApp = Join-Path $raiz 'KUBIX.exe'
    Write-Host ("Compilando {0,-15} " -f 'App KUBIX') -NoNewline
    if (Test-Path $csc) {
        if (Test-Path $exeApp) { Remove-Item $exeApp -Force }
        $ErrorActionPreference = 'Continue'
        $log = & $csc /nologo /target:winexe /codepage:65001 "/out:$exeApp" "/win32icon:$PSScriptRoot\lanzador\kubix.ico" `
            /r:System.Windows.Forms.dll /r:System.Drawing.dll "$PSScriptRoot\lanzador\KubixLauncher.cs" 2>&1
        $codigo = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($codigo -eq 0 -and (Test-Path $exeApp)) { Write-Host "OK" -ForegroundColor Green; $resultados += $true }
        else { Write-Host "FALLO" -ForegroundColor Red; $log | Select-Object -First 5 | ForEach-Object { Write-Host "   $_" }; $resultados += $false }
    } else {
        Write-Host "FALLO (no se encontro csc.exe de .NET Framework)" -ForegroundColor Red
        $resultados += $false
    }
    if ($AccesoDirecto -and (Test-Path $exeApp)) {
        $escritorio = [Environment]::GetFolderPath('Desktop')
        $lnk = (New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $escritorio 'KUBIX.lnk'))
        $lnk.TargetPath = $exeApp
        $lnk.WorkingDirectory = $raiz
        $lnk.IconLocation = $exeApp
        $lnk.Description = 'KUBIX - software de juegos'
        $lnk.Save()
        Write-Host "Acceso directo creado en el escritorio: KUBIX"
    }
}

$fallos = @($resultados | Where-Object { -not $_ }).Count
Write-Host ""
if ($fallos -eq 0) {
    Write-Host "Todo compilado ($($resultados.Count) de $($resultados.Count)). Abre KUBIX.exe en la raiz del proyecto." -ForegroundColor Green
    Terminar 0
} else {
    Write-Host "$fallos de $($resultados.Count) fallaron." -ForegroundColor Red
    Terminar 1
}
