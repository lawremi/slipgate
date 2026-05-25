# Slipgate

Slipgate is a lightweight Qt launcher for classic Quake single-player maps and
mods. It refreshes the Quaddicted catalog at startup, lets you search locally,
downloads archives and dependencies, installs them into your Quake folder, and
launches them with a modern QuakeSpasm-family client.

This is an early release, but it is already meant to cover the boring workflow:
find a release, install it, launch it, and come back later without remembering
which `-game` or `+map` incantation it needed.

## Features

- Auto-detects Steam's Quake install by reading Steam library metadata.
- Auto-detects modern QuakeSpasm-family clients in priority order:
  Ironwail, vkQuake, QuakeSpasm-Spiked, then QuakeSpasm.
- Searches for clients across PATH, Linux desktop entries, Windows App Paths,
  macOS app bundles, and near the detected Quake folder.
- Falls back to launching Steam app `2310` if no source port is found.
- Verifies the selected Quake folder contains `id1/pak0.pak` and `id1/pak1.pak`.
- On Linux, offers to create lowercase symlinks when Steam installed uppercase
  `PAK0.PAK` and `PAK1.PAK`.
- Refreshes the Quaddicted single-player Quake catalog at launch.
- Searches locally and incrementally as you type.
- Downloads archives into the app data location and verifies SHA256 checksums.
- Resolves and installs Quaddicted dependencies before the selected entry.
- Installs files into the detected or selected Quake folder using Quaddicted
  install metadata.
- Tracks installed files with per-entry manifests so installed entries can be
  uninstalled later.
- Shows installed entries first and lets Enter launch installed entries or
  install uninstalled entries from the result list.
- Supports deleting cached archives without uninstalling installed content.

## Search

Plain search terms match titles only:

```text
alkaline
"arcane dimensions"
```

Use `field:value` tokens for other fields. Multiple tokens are combined with
AND, and prefix a token with `-` to exclude matches:

```text
author:sock
type:episode theme:base
mod:arcanedimensions -author:sock
startmap:start
dependency:ad_v1_80p1final
tag:bsp2
```

Useful fields include `title`, `author`, `filename`, `archive`, `commandline`,
`command`, `cmd`, `date`, `released`, `startmap`, `map`, `dependency`,
`depends`, `sha`, `sha256`, `description`, `desc`, and `tag`. Quaddicted tag
fields such as `type`, `theme`, `mod`, `version`, or `bsp_format` also work.

## Build

Slipgate is a Qt 6 Widgets application.

```sh
cmake -S . -B build
cmake --build build
./build/slipgate
```

Dependencies:

- Qt 6 Widgets
- Qt 6 Network
- CMake 3.24 or newer
- A ZIP extractor on PATH: `bsdtar` preferred, `unzip` as fallback

## Data Locations

Downloaded archives and install manifests are stored under Qt's app data
location. On Linux this is typically:

```text
~/.local/share/slipgate/slipgate/
```

Common subdirectories:

```text
downloads/
installed/
```

Installed map/mod files are copied into the detected or selected Quake folder.
The app labels this as the Quake folder because Steam is only one possible way
to find it.

## Quake Data

Slipgate will block launch if the Quake folder does not contain:

```text
id1/pak0.pak
id1/pak1.pak
```

On Linux these names are case-sensitive. Steam may install them as uppercase
`PAK0.PAK` and `PAK1.PAK`; Slipgate can create lowercase symlinks for that case.

## Quaddicted

Catalog metadata is read from:

```text
https://www.quaddicted.com/api/v1/
```

The catalog refresh uses this query:

```text
+tags:"game=quake" +tags:"game_mode=singleplayer"
```

Dependency resolution first checks the local catalog, then queries Quaddicted by
dependency filename when necessary.

## Limitations

- Slipgate focuses on QuakeSpasm-style clients and intentionally does not try to
  support FTEQW or DarkPlaces-specific workflows.
- The GitHub release currently ships source only; no binary packages are built
  yet.
- Installation copies files into your chosen Quake folder. Uninstall only removes
  files recorded in Slipgate's install manifest for that entry.
