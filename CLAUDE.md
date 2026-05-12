# Cybershow Orchestrator — AI Assistant Context

> Working context for Claude, Codex, Gemini, or any coding assistant.
> Keep this file updated as the project evolves.

---

## 1. Project Summary

Desktop orchestrator for a live cybersecurity show called **Bajo Ataque**.

The orchestrator is the **control cabin** for the show. It launches, monitors, and coordinates
external show applications, video playback, and a fullscreen stage window on a secondary screen
(projector). Built in C++23 / Qt 6.7.3 / CMake for Windows, compiled with MSVC.

The show uses several C++/Qt Windows executables, videos, and later sounds/lighting. The
orchestrator does not merely launch apps — it activates and supervises show scenes.
The operator window opens fullscreen by default; the stage/projector window remains a separate
fullscreen window selected from the Escenario controls.

---

## 2. Repository Layout

```
orchestrator/
├── CMakeLists.txt
├── package-release.ps1          # Release packaging (requires pwsh 7+)
├── README.md                    # Operator documentation
├── CLAUDE.md                    # This file
├── config/                      # Runtime-generated JSON (created on first run)
│   ├── apps.json
│   ├── android.json
│   ├── media.json
│   ├── rundown.json
│   └── stage.json
├── apps/                        # External show apps, each self-contained
├── media/                       # Video/audio files
├── resources/                   # Qt resources (icons, default config templates)
│   ├── apps_default.json
│   └── android_default.json
└── src/
    ├── main.cpp
    ├── MainWindow.h / .cpp
    ├── AdbManager.h / .cpp       # Low-level adb.exe wrapper
    ├── AndroidConfig.h / .cpp    # android.json schema + load/save
    ├── AndroidManager.h / .cpp   # Android app lifecycle (launch/stop via ADB)
    ├── AppConfig.h / .cpp        # apps.json schema + load/save
    ├── AppManager.h / .cpp       # QProcess lifecycle for external apps
    ├── MediaConfig.h / .cpp      # media.json schema + load/save
    ├── MediaManager.h / .cpp     # QMediaPlayer + standalone video widget
    ├── RundownConfig.h / .cpp    # rundown.json — ordered scene list
    ├── StageWindow.h / .cpp      # Fullscreen projector window
    ├── Logger.h / .cpp           # Log panel messages
    └── ui/
        ├── CyberTheme.h / .cpp           # Palette constants + global stylesheet
        ├── CyberBackgroundWidget.h/.cpp  # Animated grid background
        ├── CyberPanel.h / .cpp           # Styled panel widget
        ├── ModeSelectorScreen.h / .cpp   # Startup mode selector (stack index 0)
        ├── ConfigureModeScreen.h / .cpp  # Configure mode (stack index 1)
        ├── RehearsalModeScreen.h / .cpp  # Ensayo mode (stack index 2)
        └── ShowModeScreen.h / .cpp       # Show/live mode (stack index 3)
```

---

## 3. Architecture

### 3.1 Main window stack

`MainWindow` owns a `QStackedWidget` with four screens:

| Index | Screen | Description |
|---|---|---|
| 0 | `ModeSelectorScreen` | Startup / mode picker |
| 1 | `ConfigureModeScreen` | Prepare the show (CONFIGURAR) |
| 2 | `RehearsalModeScreen` | Manual rehearsal control (ENSAYO) |
| 3 | `ShowModeScreen` | Live scene-by-scene execution (SHOW) |

Each mode screen emits `returnToSelector()` and `switchMode(int)` that `MainWindow` connects to
the stack. Keyboard navigation: `Esc` = back to selector, `1`/`2`/`3` = jump to mode,
`←`/`→` = previous/next mode.

### 3.2 StageWindow

`StageWindow` is a separate top-level `QWidget` (not in the main stack). It is a fullscreen
borderless window placed on the projector/secondary screen.

```cpp
enum class Content { Black, Logo };
```

Key public slots: `showBlack()`, `showLogo()`, `softHide()`, `softShow()`.

- `softHide()` hides the window **without** emitting `deactivated` — used just before an app
  appears on the projector. `softShow()` restores it and shows the logo.
- Video is **not** rendered inside StageWindow (see §3.4).
- Stage controls (screen combo + Activar/Desactivar button) are hidden when only one screen is
  detected — avoids accidentally blacking out the only screen.

**Critical: pinning to the correct screen**

```cpp
static void pinToScreen(QWidget* w, QScreen* screen) {
    w->winId();  // forces native HWND; windowHandle() is valid after this
    if (auto* wh = w->windowHandle())
        wh->setScreen(screen);
}
```

Call `pinToScreen()` before `showFullScreen()`. Without it, Windows always opens a never-shown
widget on the primary monitor the first time, regardless of `setGeometry()`.

**Never use `QWidget::create()` — it is protected in Qt 6. Always use `winId()` instead.**

### 3.3 AppManager

Manages `QProcess` lifecycle for external show executables.

```cpp
enum class AppState { Stopped, Starting, Running, Stopping, Error };
enum class AppLaunchMode { Demo, Live };
```

- Each app is launched with its own `workingDirectory` so DLL/plugin lookup works.
- App launch mode is explicit per start call: `Demo` appends `--demo`, `Live` appends `--live`.
  SHOW mode always uses live. CONFIGURAR and ENSAYO expose both Demo and Live buttons for apps.
- `setStageGeometry(QRect)` passes the projector screen geometry; `scheduleWindowMove()` uses
  Windows API (`EnumWindows` + `SetWindowPos`) to move the app window to the projector after
  it starts.
- Close policy: polite `terminate()` → 3 s kill timer → `kill()`.
- **No ADB dependency.** AppManager only handles Qt processes. Android app lifecycle is fully
  owned by `AndroidManager`.

### 3.6 AdbManager

Low-level wrapper around `adb.exe`. Looks for adb at
`C:/Users/caico/AppData/Local/Android/Sdk/platform-tools/adb.exe`, falls back to PATH.

Key methods:
- `detectDevice()` — runs `adb devices`, emits `deviceFound(serial)` or `deviceLost()`
- `connectWifi(ipPort)` — runs `adb connect <ip:port>`, then auto-calls `detectDevice()`
- `disconnectDevice()` — for WiFi connections, runs `adb disconnect <serial>` and emits `deviceLost()`; for USB, just clears internal state
- `testConnection()` — runs `getprop ro.product.model` and on success vibrates the device; if `m_serial` was empty and the test succeeds, auto-calls `detectDevice()` to sync state
- `setupReverseTunnel(port)` — runs `adb reverse tcp:<port> tcp:<port>`
- `launchApp(package, activity)` — runs `adb shell am start -n package/activity`
- `stopApp(package)` — runs `adb shell am force-stop package`

All commands are fire-and-forget (async `QProcess`). Signals: `deviceFound(serial)`, `deviceLost()`, `log(text)`.

### 3.7 AndroidManager

Owns Android app lifecycle. Receives a shared `AdbManager*` at construction.

```cpp
enum class AndroidState { Stopped, Running };
```

- `loadApps(QList<AndroidEntry>)` — replaces the entry list
- `start(id)` — calls `setupReverseTunnel(wsPort)` if wsPort > 0, then `launchApp`; sets Running
- `stop(id)` — calls `stopApp`; sets Stopped
- `stopAll()` — stops all entries

State is fire-and-forget: there is no process to monitor, so Running/Stopped reflects the last
command sent, not verified app state. Signals: `stateChanged(id, state)`, `logMessage(text)`.

Each screen (CONFIGURAR, ENSAYO) owns its own `AdbManager` + `AndroidManager` instance,
consistent with the AppManager/MediaManager per-screen ownership pattern.

### 3.4 MediaManager — standalone video widget

Video is **not** embedded in `StageWindow` or any `QStackedWidget`. Embedding was unreliable
across screens with the FFmpeg backend. Instead, `MediaManager` creates a top-level
`QVideoWidget` with `Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint` and
pins it directly to the stage screen geometry.

```cpp
void MediaManager::setStageGeometry(const QRect& geo);
```

Call this whenever the stage activates (connect to `StageWindow::activated`). When `play()`
is called and stage geometry is set, the manager:
1. Creates a standalone `QVideoWidget` (once per media runtime).
2. Calls `winId()` to force native window, then `windowHandle()->setScreen(targetScreen)`.
3. Sets geometry = stage screen geometry, then `show()` + `raise()`.

When video stops, the mode screen calls `m_stageWindow->showLogo()`.

### 3.5 RundownConfig

`config/rundown.json` holds an ordered flat list of scene references:

```json
[
  {"type": "app",   "ref": "my_app_id"},
  {"type": "media", "ref": "my_video_id"}
]
```

`syncWithLibraries(appIds, mediaIds)` adds new entries not yet in the rundown and removes
stale refs. Both `RehearsalModeScreen` and `ShowModeScreen` call this on `showEvent`.

### 3.6 Automatic logo

Logo shows automatically — there is no explicit logo/black button in the operator UI.
It is triggered:
- On stage activation (`StageWindow::activated` signal).
- When video playback stops (`MediaState::Stopped`).
- When an app stops (after `softShow()` restores the stage window).
- When stop-all is invoked.

---

## 4. Config Files

All paths relative to the package root (the directory containing `orchestrator.exe`).

| File | Purpose |
|---|---|
| `config/apps.json` | Registered show applications |
| `config/media.json` | Registered video/audio files |
| `config/rundown.json` | Ordered scene list (flat array, no enabled flag) |
| `config/stage.json` | Last-used projector screen index |

### AppEntry fields

```json
{
  "id": "my_app",
  "name": "My App",
  "description": "optional",
  "executable": "apps/MyApp/MyApp.exe",
  "workingDirectory": "apps/MyApp",
  "arguments": [],
  "startupPolicy": "manual",
  "closePolicy": "terminateThenKill",
  "expectedWindowTitle": "",
  "category": "",
  "linkedAndroidId": "my_android_app"
}
```

`linkedAndroidId` is optional (empty = no Android companion). It's a reference to an `AndroidEntry.id`
in `android.json`. Used for UI pairing hints; the orchestrator does not enforce it automatically.

### AndroidEntry fields (`config/android.json`)

```json
{
  "id": "companion",
  "name": "Companion",
  "package": "com.cuarzopolar.companion",
  "activity": ".MainActivity",
  "wsPort": 8765
}
```

`wsPort` > 0 triggers `adb reverse tcp:<wsPort> tcp:<wsPort>` before launch. Set to 0 to skip
the tunnel (e.g. for apps that use their own connection mechanism).

Default entries: `companion` (port 8765) and `password-android` (port 8767).

### MediaEntry fields

```json
{
  "id": "intro_1",
  "name": "Intro 1",
  "type": "video",
  "path": "media/intro_1.mp4"
}
```

`type` can be `"video"` or `"audio"`. Audio entries play without a video widget.

---

## 5. UI / Theme

All visual constants live in `CyberTheme.h` (namespace `CyberTheme`):

| Token | Value | Role |
|---|---|---|
| `BackgroundBase` | `#050608` | Deepest background |
| `PanelBackground` | `#101318` | Panel fill |
| `PanelBorder` | `#293241` | Panel border |
| `TextPrimary` | `#F2F5F8` | Primary text |
| `TextSecondary` | `#8D96A3` | Secondary text |
| `TextMuted` | `#5F6B78` | Muted / disabled text |
| `AccentPrimary` | `#1688E8` | Blue accent (buttons) |
| `AccentCyan` | `#00D1FF` | Cyan accent |
| `AccentGreen` | `#00FF55` | Running / playing state |
| `Warning` | `#FFB000` | Starting / stopping state |
| `Error` | `#FF3347` | Error state |

`CyberTheme::globalStyleSheet()` is applied once in `main.cpp`.

**Language rule:** UI labels in Spanish. Code, identifiers, JSON keys, and comments in English.

**Stage logo:** "BAJO ATAQUE" in red `#FF2020`, 80 px bold, wide letter-spacing.

**State colors in tables:**

| State | Label | Color |
|---|---|---|
| Stopped | LISTA / LISTO | `TextMuted` grey |
| Starting / Stopping | EJECUTÁNDOSE | `Warning` amber |
| Running | EJECUTÁNDOSE | `AccentGreen` |
| Playing | REPRODUCIENDO | `AccentGreen` |
| Error | ERROR | `Error` red |

---

## 6. Build

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Requires Qt 6.7.3 and MSVC (no MinGW). The `Qt6Multimedia` module must be enabled.
FFmpeg DLLs (`avcodec`, `avformat`, `avutil`, `swresample`, `swscale`) and
`plugins/multimedia/` must be present next to the exe.

**Release packaging:**
```powershell
.\package-release.ps1
```
Requires PowerShell 7+ (`pwsh`). Creates `dist\bajo-ataque-orchestrator-vNN.zip`.
Use `-Force` to skip the uncommitted-changes check.

---

## 7. Known Gotchas

| Issue | Solution |
|---|---|
| `QWidget::create()` is protected (C2248 in MSVC) | Use `winId()` — forces native window creation as a public side effect |
| First `showFullScreen()` lands on primary monitor | Call `pinToScreen()` (winId + windowHandle()->setScreen()) before showFullScreen |
| QVideoWidget inside QStackedWidget renders on wrong screen | Use standalone top-level QVideoWidget pinned directly to stage screen geometry |
| FFmpeg multimedia backend missing | avcodec/avformat/avutil/swresample/swscale DLLs + plugins/multimedia/ must be in package |
| App DLL lookup fails | Always set `workingDirectory` to the app's own folder; never rely on PATH |
| MSVC stricter auto deduction | Prefer explicit types in range-for over `QMap` — MSVC rejects some GCC-valid patterns |
| Video widget not showing on projector | Verify `m_stageGeometry` is set (via `setStageGeometry`) before `play()` is called |

---

## 8. What Is Built and Working

- Mode selector screen with keyboard navigation and animated cyber background
- Operator window opens fullscreen by default and the selector cards resize with the display
- Three mode screens: CONFIGURAR, ENSAYO, SHOW — full keyboard navigation between them
- **CONFIGURAR:** 4-tab layout:
  - **ADB tab:** device status (auto-detects on showEvent), Detectar / Probar / Desconectar, WiFi connect (IP:port)
  - **Qt tab:** full CRUD for Qt apps (add/edit/delete), Demo/Live launch, Parar
  - **Android tab:** full CRUD for Android apps (add/edit/delete via dialog), Lanzar/Parar
  - **Multimedia tab:** full CRUD for media, Iniciar/Parar
- **ENSAYO:** rundown table (Qt apps + media) with reorder arrows, Android apps section below with ADB status indicator; auto-detects ADB device on showEvent
- **SHOW:** scene-by-scene navigation (Anterior / Activar / Siguiente), current-row highlight
- Log panels are hidden by default on operator screens and toggled with `F10`.
- AdbManager: detectDevice, connectWifi, disconnectDevice, testConnection (vibrate), setupReverseTunnel, launchApp, stopApp
- AndroidManager: start/stop/stopAll via ADB, fire-and-forget state (Running/Stopped)
- AppManager: start / stop / restart / stopAll with QProcess, 3-second kill fallback; no ADB dependency
- MediaManager: play / stop video with standalone fullscreen widget pinned to stage screen
- StageWindow: fullscreen on secondary screen, logo / black, softHide / softShow
- `pinToScreen()` fix — stage always opens on the correct screen on first activation
- Automatic logo: shows on activation, video stop, app stop — no explicit button needed
- RundownConfig: ordered list synced from apps + media libraries
- Stage controls hidden on single-screen setup
- Logger: visible log panel in all mode screens
- Package script (`package-release.ps1`) with versioned zip output

---

## 9. Next Steps (Priority Order)

1. **Test stage + video on two-screen hardware.** The `pinToScreen()` fix and standalone video
   widget were committed but not yet validated with a real projector. This is the highest-priority
   test before any live use.

2. **ADB Desconectar for USB.** `disconnectDevice()` clears internal state for USB devices but
   does not actually disconnect the USB link (ADB cannot do this by software). For WiFi it works
   correctly. Consider whether a visual "force-forget" behavior is sufficient for USB or if a
   different UX is needed.

3. **Android state is fire-and-forget.** `AndroidManager` sets Running/Stopped based on the last
   command sent, not on verified app state. If the Android app crashes or is force-stopped by the
   user, the orchestrator will still show Running. A polling or ADB-based verification loop could
   improve this, but adds complexity.

4. **SHOW mode: Android apps.** SHOW mode does not yet have an Android apps section or ADB
   indicator. If needed for live show control, add it following the ENSAYO pattern.

5. **Emergency controls.** Add to both ENSAYO and SHOW: stop current item, restart current item,
   black screen on stage (already possible via `m_stageWindow->showBlack()`). Should be prominent
   and accessible without mouse in live mode.

6. **App window to projector.** `AppManager::scheduleWindowMove()` exists (EnumWindows +
   SetWindowPos) but may need tuning for timing. The stage should `softHide()` when an app starts
   so the app window is visible; `softShow()` + `showLogo()` when it stops.

7. **Sound support.** `MediaEntry.type` already supports `"audio"`. `MediaManager::play()` would
   need a branch that creates `QAudioOutput` without a video widget for audio-only entries.

8. **Robustness pass.** Pre-show checklist, clearer operator alerts, better error recovery.

---

## 10. Design Rules (Non-Negotiable)

1. All paths relative to the package root — never hardcoded absolute paths.
2. Each external app is self-contained in its own folder under `apps/` with its own DLLs.
3. Always set `workingDirectory` when launching an external app via QProcess.
4. Do not embed external executables in Qt resources.
5. Prefer robustness over disk efficiency (duplicate DLLs per app is correct).
6. Code, identifiers, JSON keys, comments in English. UI labels in Spanish.
7. Logo shows automatically — no explicit black/logo buttons in the operator UI.
8. `winId()` before `windowHandle()->setScreen()` — never `QWidget::create()`.
9. Video uses standalone top-level `QVideoWidget` pinned to stage geometry — not embedded in StageWindow.
10. Keep emergency controls accessible from ENSAYO and SHOW — the show must survive a scene failure.
11. Do not over-engineer. Add features only when the show actually needs them.
12. Fewer controls in SHOW mode = safer live performance.
