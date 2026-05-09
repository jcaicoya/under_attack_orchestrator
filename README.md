# BAJO ATAQUE — Orchestrator

Centro de control del show de ciberseguridad en vivo "Bajo Ataque".
Lanza, supervisa y coordina las aplicaciones del show, vídeos y el escenario proyectado desde un único panel de operador.

---

## Overview

The orchestrator is **not** a simple launcher. It is the control cabin for a live cybersecurity performance. It manages:

- External show applications (each a self-contained Windows executable with its own DLLs)
- Video playback routed to a secondary projector screen
- A fullscreen stage window (logo / black / video) on the projector
- A rundown of scenes (apps + media) for rehearsal and live show

The operator window opens fullscreen on the laptop display by default. The projector/stage window is still activated separately from the Escenario controls when a secondary screen is available.

Three operating modes:

| Mode | Purpose |
|---|---|
| **CONFIGURAR** | Prepare the show: add/edit apps and media, test app Demo/Live launch, activate stage screen |
| **ENSAYO** | Rehearse: reorder the rundown, launch apps in Demo/Live, play videos |
| **SHOW** | Run the show live: navigate scenes in order with Anterior / Activar / Siguiente |

---

## Navigation & keyboard shortcuts

### Mode selector (startup screen)

| Key | Action |
|---|---|
| `1` | Open CONFIGURAR |
| `2` | Open ENSAYO |
| `3` | Open SHOW |
| `←` / `→` | Move focus between mode cards |
| `Enter` / `Space` | Open focused mode |

### Inside any mode screen

| Key | Action |
|---|---|
| `Esc` | Return to mode selector |
| `1` | Switch to CONFIGURAR |
| `2` | Switch to ENSAYO |
| `3` | Switch to SHOW |
| `F10` | Show / hide log panel |
| `←` | Switch to previous mode (ENSAYO → CONFIGURAR, SHOW → ENSAYO) |
| `→` | Switch to next mode (CONFIGURAR → ENSAYO, ENSAYO → SHOW) |

### SHOW mode additional keys

| Key | Action |
|---|---|
| `→` / `Space` | Next scene |
| `Enter` | Activate current scene |

### Stage screen (projector window)

| Key | Action |
|---|---|
| `Esc` | Deactivate stage (closes projector window) |

---

## Stage screen

The stage window is a fullscreen borderless window on a secondary screen (projector).

**Activating:**
1. In any mode screen, the "Escenario" row shows a screen selector combo and an "Activar" button — but only when two or more screens are detected.
2. Select the target screen (typically Pantalla 2) and press **Activar**.
3. The stage window opens on the selected screen showing the BAJO ATAQUE logo.
4. Press **Desactivar** to close it.

**Automatic stage content:**
- Logo shown on activation and whenever nothing is playing.
- When a video plays, a fullscreen video window overlays the stage screen.
- When an app runs, the stage window hides so the app can appear on the projector; when the app stops, the logo returns.

**Single-screen:** if only one screen is detected, the Escenario controls are hidden and the stage cannot be activated (avoids blacking out the operator screen).

---

## Requirements & constraints

| Item | Requirement |
|---|---|
| OS | Windows 10/11 64-bit |
| Runtime | Visual C++ Redistributable (install once on each machine) |
| Qt | 6.7.3 — bundled in the zip, no global install needed |
| Multimedia | FFmpeg backend — bundled DLLs in the zip |
| External apps | Must be self-contained (exe + DLLs) in their own folder under `apps/` |
| Paths | All paths relative to the package root — never hardcoded absolute paths |
| Config | JSON files in `config/` — created automatically on first run |
| Logs | Written to console and log panel — file logging can be added later |

**Package structure:**

```
orchestrator.exe
config/           ← generated at runtime (apps.json, media.json, rundown.json, stage.json)
apps/             ← external show apps, each self-contained
media/            ← video and audio files
sounds/           ← (reserved)
lights/           ← (reserved)
logs/             ← (reserved)
tools/            ← (reserved)
```

**DLL policy:** each external app must have its own copy of Qt DLLs next to its executable. Do not share DLLs between apps. Robustness for live performance beats disk efficiency.

---

## Making a release zip

```powershell
.\package-release.ps1
```

- Requires PowerShell 7+ (`pwsh`). Run from the project root.
- Builds the Release configuration in CMake.
- Bundles `orchestrator.exe` + all required Qt and FFmpeg DLLs + plugins.
- Creates `dist\bajo-ataque-orchestrator-vNN.zip` (zero-padded incrementing version).
- Appends an entry to `releases.json` and creates a git tag.
- Use `-Force` to skip the uncommitted-changes check.
- After packaging: `git push --tags` to push the tag to the remote.

**Bundled DLLs:** Qt6Core, Qt6Gui, Qt6Widgets, Qt6Multimedia, Qt6MultimediaWidgets, Qt6Network, Qt6OpenGL + FFmpeg (avcodec, avformat, avutil, swresample, swscale) + `plugins/platforms/qwindows.dll` + `plugins/multimedia/` (FFmpeg backend).

**Target machine:** copy the zip, extract, run `orchestrator.exe`. No installer needed.

---

## Look & feel summary

**Theme:** dark, technical, cyber. Black deep background, subtle grid lines, cold blue/green accents. Operator-focused — readable on laptop and projector.

**Palette:**

| Role | Color |
|---|---|
| Background | `#0A0C10` |
| Panel background | `#101318` |
| Border | `#293241` |
| Text primary | `#C8D6E5` |
| Text muted | `#5F6B78` |
| Accent blue | `#00BFFF` |
| Accent green | `#00FF55` |
| Warning | `#FFB000` |
| Error / danger | `#FF3347` |

**State colors in tables:**

| State | Label | Color |
|---|---|---|
| Stopped | LISTA | muted grey |
| Starting / Stopping | EJECUTÁNDOSE | amber |
| Running | EJECUTÁNDOSE | green |
| Playing (media) | REPRODUCIENDO | green |
| Error | ERROR | red |

**Language:** operator UI in Spanish. Code, identifiers, comments, and config keys in English.

**Stage logo (projector):** "BAJO ATAQUE" in red `#FF2020`, 80px bold, wide letter-spacing. Subtitle "en vivo" in dark red `#3A1010`.
