# Cybershow Orchestrator

Versión: 0.1  
Estado: activo — refactorización v0.3 completada.

Centro de control del show Cybershow. Lanza, supervisa y detiene las aplicaciones del show desde una única consola.

Para el contexto completo del proyecto ver `CLAUDE.md`.  
Para el plan de refactorización en curso ver `next-steps.md`.

Este archivo contiene las instrucciones específicas del orquestador según el estándar Cybershow. Debe leerse junto a:

- `cybershow_app_standards_v0_3/CYBERSHOW_APP_CONVENTIONS.md`
- `cybershow_app_standards_v0_3/QT_APP_LOOK_AND_FEEL.md`
- `cybershow_app_standards_v0_3/ORCHESTRATOR_VISUAL_AND_OPERATION.md`

---

## 1. Identidad de la aplicación

- **Nombre visible:** CYBERSHOW / Centro de control
- **Nombre interno:** orchestrator
- **Ejecutable:** orchestrator.exe
- **Rol dentro del show:** aplicación de control y lanzamiento. No es una app escénica.
- **Tipo principal:** operativa (control de operador)

---

## 2. Pantallas del orquestador

El orquestador no usa la navegación estándar por escenas. Su estructura es:

| Nº | ID | Título largo | Título corto | Tipo | Notas |
|---:|---|---|---|---|---|
| — | selector | Selector de modo | Selector | operative | Pantalla inicial. No numerada. |
| 1 | configurar | Configurar | Configurar | operative | Control y lanzamiento de apps con `--configure`. |
| 2 | diseño | Diseño | Diseño | operative | Previsto. Deshabilitado en v0.1. |
| 3 | show | Show | Show | operative | Previsto. Deshabilitado en v0.1. |

---

## 3. Pantalla inicial: ModeSelectorScreen

La pantalla inicial no es un setup estándar. Es un selector de modo de operación.

Elementos:
- Título: **CYBERSHOW**
- Subtítulo: *Centro de control*
- Tres tarjetas de modo: CONFIGURAR, DISEÑO, SHOW
- DISEÑO y SHOW deshabilitados con etiqueta "próximamente"
- Estado general / perfil en la parte inferior (futuro)

Navegación en el selector:
- `1` → selecciona y abre CONFIGURAR
- `2`, `3` → enfoca tarjeta (deshabilitada, no abre)
- `←` / `→` → cambia tarjeta enfocada
- `Enter` / `Espacio` → abre la tarjeta enfocada si está disponible
- Click → abre la tarjeta pulsada si está disponible

---

## 4. Pantalla de modo: ConfigureModeScreen

Pantalla de tipo dashboard operativo. Muestra y controla las aplicaciones del show.

Elementos:
- Título: **Configurar**
- Panel de aplicaciones: lista de apps con estado y acciones
- Panel de registro de eventos (log)
- Indicación de navegación: *Esc: volver al selector*

Estados de proceso (en español):

| Estado interno | Etiqueta visible | Color |
|---|---|---|
| Stopped | LISTA | gris `#5F6B78` |
| Starting | EJECUTÁNDOSE | ámbar `#FFB000` |
| Running | EJECUTÁNDOSE | verde `#00FF55` |
| Stopping | EJECUTÁNDOSE | ámbar `#FFB000` |
| Error | ERROR | rojo `#FF3347` |

Acciones de proceso (en español):
- **Iniciar** — lanza la app con los argumentos del modo actual
- **Parar** — termina el proceso
- **Parar todo** — para todas las apps

Navegación:
- `Esc` → vuelve al selector de modo

---

## 5. Modos del orquestador y argumento de lanzamiento

| Modo orquestador | Argumento pasado a apps |
|---|---|
| Configurar | `--configure` |
| Diseño | `--design` |
| Show | `--show` |

---

## 6. Integración con el orquestador (el propio orquestador como cliente)

El orquestador no es lanzado por otro orquestador. No procesa argumentos de arranque estándar (`--configure`, `--show`, etc.) para sí mismo.

El orquestador sí emite mensajes `CYBERSHOW_*` para posibles integraciones futuras (logging central, monitorización remota).

---

## 7. Excepciones al estándar común

Las siguientes reglas del estándar común **no aplican** al orquestador, por diseño:

| Regla estándar | Excepción del orquestador |
|---|---|
| Pantalla inicial es Setup (`--configure`) | La pantalla inicial es ModeSelectorScreen, no un setup |
| `Esc` vuelve a Setup | `Esc` vuelve al selector de modo |
| `1`-`9` cambian pantalla de ejecución | `1`-`3` seleccionan modo en el selector |
| Barra inferior de navegación por escenas | No existe; el orquestador no navega entre escenas |
| Acepta `--configure` / `--show` como args propios | No aplica; el orquestador los pasa a las apps hijas |
| Navegación `←`/`→` entre escenas | `←`/`→` cambian tarjeta enfocada en el selector |

---

## 8. Checklist de refactorización

- [x] Usa `CyberBackgroundWidget` como base de ventana.
- [x] Usa paleta y botones de `CyberTheme`.
- [x] No tiene pantalla de setup estándar.
- [x] Tiene `ModeSelectorScreen` como pantalla inicial.
- [x] CONFIGURAR disponible y funcional.
- [x] DISEÑO y SHOW deshabilitados con indicación visual.
- [x] `1`, `2`, `3` seleccionan modos en el selector.
- [x] `Enter` y `Espacio` abren el modo seleccionado.
- [x] `←` / `→` cambian tarjeta enfocada en el selector.
- [x] `Esc` desde modo vuelve al selector.
- [x] `Alt+F4` cierra la aplicación (comportamiento estándar del SO).
- [x] Sin barra inferior de escenas.
- [x] Apps lanzadas con `--configure` desde modo CONFIGURAR.
- [x] La arquitectura permite añadir `--design` y `--show` más adelante.
- [x] Estados de proceso en español.
- [x] Textos de operador en español.
- [x] Errores visibles y comprensibles para el operador.
- [x] Compila sin errores.
- [x] Arranca y muestra el selector de modo.
- [x] Navegación teclado probada.
- [x] Probado en portátil de desarrollo.

---

## 9. Estructura del proyecto

```
orchestrator/
├── src/
│   ├── main.cpp
│   ├── Logger.{h,cpp}
│   ├── AppConfig.{h,cpp}
│   ├── AppManager.{h,cpp}
│   ├── MainWindow.{h,cpp}
│   └── ui/
│       ├── CyberTheme.{h,cpp}
│       ├── CyberBackgroundWidget.{h,cpp}
│       ├── CyberPanel.{h,cpp}
│       ├── ModeSelectorScreen.{h,cpp}
│       └── ConfigureModeScreen.{h,cpp}
├── resources/
│   └── apps_default.json          ← plantilla embebida (Qt resource)
├── resources.qrc
├── CMakeLists.txt
├── dist/                          ← zips de release (gitignored)
└── releases.json
```

En tiempo de ejecución, el orquestador copia `apps_default.json` a `config/apps.json` junto al `.exe` si no existe. Para editar la configuración en uso, editar ese archivo externo, no el recurso embebido.

---

## 10. Packaging

Script: `package-release.ps1`  
Tracking file: `releases.json` (committed to git)  
Output folder: `dist\` (gitignored)  
Zip naming: `cybershow-orchestrator-vNN.zip` (zero-padded, incrementing)

Workflow:
- `.\package-release.ps1` — builds Release, zips, appends to `releases.json`, creates git tag
- `.\package-release.ps1 -Force` — same but skips commit-change check
- `git push --tags` — push tags to remote after packaging

Zip contents: `orchestrator.exe` + Qt6Core/Gui/Widgets DLLs + `plugins/platforms/qwindows.dll`  
+ empty placeholder folders: `config/`, `apps/`, `media/`, `sounds/`, `lights/`, `logs/`, `tools/`

Target machine requires Visual C++ Redistributable (install once).
