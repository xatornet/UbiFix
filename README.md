# GhostReconFS-PathFix

> **ASI plugin** that fixes the black-screen crash on startup when Tom Clancy's Ghost Recon Future Soldier is installed on a drive other than `C:\`.

---

## How it works

The game has a hardcoded assumption that it lives on `C:\`. This plugin hooks the following Windows APIs at process startup using **Microsoft Detours**:

| Hooked API | Purpose |
|---|---|
| `CreateFileA/W` | File open calls with hardcoded `C:\` paths |
| `GetPrivateProfileStringA/W` | INI-file reads the game performs on startup |
| `RegOpenKeyExA` | Registry key lookups containing install paths |

Whenever the game passes a path that starts with `C:\<your-install-dirs>`, the plugin silently rewrites it to the real drive (e.g. `D:\Games\GhostReconFS\...`) before forwarding the call to Windows. The game never notices.

The plugin detects the real install path automatically from the location of `GRFSGame.exe` — no manual configuration needed.

If the game **is** already on `C:\`, the plugin detects this and skips installing any hooks.

---

## Installation

1. **Download** the latest release ZIP from the [Releases](../../releases) page.
2. **Get an ASI Loader** — the plugin needs one to be injected. The recommended one is [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases):
   - Download `dsound.zip` (x86 version!) and extract `dsound.dll` into your game folder.
3. **Copy** `GhostReconFS-PathFix.asi` and `GhostReconFS-PathFix.ini` into your Ghost Recon Future Soldier folder (where `GRFSGame.exe` is).
4. Launch the game normally. ✅

Your game folder should look like this:
```
GRFSGame.exe
dsound.dll                    ← ASI Loader
GhostReconFS-PathFix.asi      ← this plugin
GhostReconFS-PathFix.ini      ← config
```

---

## Configuration (`GhostReconFS-PathFix.ini`)

```ini
[Settings]
; Set to 1 to enable verbose logging (creates GhostReconFS-PathFix.log)
DebugMode=0
```

| Key | Values | Default | Description |
|---|---|---|---|
| `DebugMode` | `0` / `1` | `0` | Enables a detailed log of every intercepted path call |

When `DebugMode=1`, a file called `GhostReconFS-PathFix.log` will be created next to the EXE. It logs every patched path call with timestamps — useful for debugging if something still doesn't work.

---

## Building from source

The project uses **CMake + MSVC** and is compiled as a 32-bit DLL (`.asi`).

### Locally (Windows)

```powershell
# Requires: Visual Studio 2022 with C++ workload, CMake
cmake -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
# Output: build/Release/GhostReconFS-PathFix.asi
```

### Via GitHub Actions (recommended)

Just push to `main` — the workflow in `.github/workflows/build.yml` will:
1. Compile the plugin for x86/Release using MSVC on a Windows runner.
2. Upload the `.asi` + `.ini` as a downloadable **Actions Artifact**.
3. On version tags (e.g. `v1.0.0`), automatically create a **GitHub Release** with the files attached.

---

## Dependencies

- [Microsoft Detours](https://github.com/microsoft/Detours) — fetched automatically by CMake via FetchContent. No manual setup needed.

---

## License

MIT
