# RavenXD Launcher — C++ / Minecraft 1.8.9

Windows launcher skeleton with a compact iOS-HUD-inspired dark glass UI.

Features:
- Minecraft 1.8.9 profile UI
- Loader selector: Forge / None
- RAM field
- `launcher/mod/RavenXD.jar` local mod
- Copies RavenXD.jar to `%APPDATA%\.minecraft\mods\RavenXD.jar`
- Version + mod update checker
- Logo assets included
- Configurable update URLs in `launcher/config.ini`

## Build with Visual Studio Developer Command Prompt

```bat
cd /d C:\path\to\RavenXD_Launcher_CPP
cl /std:c++17 /EHsc /DUNICODE /D_UNICODE /DWIN32 /D_WINDOWS main.cpp /link /SUBSYSTEM:WINDOWS urlmon.lib shell32.lib
```

The output is `RavenXDLauncher.exe`.

## Build with CMake

```bat
cmake -S . -B build
cmake --build build --config Release
```

## Mod/update setup

Put the current mod at:

```text
launcher\mod\RavenXD.jar
```

Edit:

```text
launcher\config.ini
```

Set:

```text
VERSION_URL=https://your-domain.example/ravenxd/version.txt
MOD_URL=https://your-domain.example/ravenxd/RavenXD.jar
VERSION=2.0
```

Your `version.txt` should contain only the latest version, for example:

```text
2.1
```

The updater downloads the JAR and replaces:

```text
%APPDATA%\.minecraft\mods\RavenXD.jar
```

Note: the example URLs are placeholders. Replace them with your own release/server URLs.

## Important

The launcher does not bundle Minecraft or Forge. It only prepares the RavenXD mod and opens the official Minecraft launcher. Forge 1.8.9 must be installed separately if the Forge option is selected.
