# Slipgate

Slipgate is a tiny Qt-based Quake launcher and Quaddicted downloader.

It tries to keep the workflow boring:

1. Detect the Steam Quake install.
2. Detect Ironwail, falling back to Steam's Quake launcher when needed.
3. Browse/search Quaddicted single-player releases.
4. Download archives and exact Quaddicted dependencies into the XDG app-data cache.
5. Extract/install into the Steam Quake directory.
6. Launch the selected mod with the database-provided command line.

## Search

Slipgate refreshes the Quaddicted database at launch. After that, search is local
and incremental as you type. Plain search terms match titles only:

```text
alkaline
"arcane dimensions"
```

Use `field:value` tokens for other fields. Multiple tokens are combined with AND,
and prefix a token with `-` to exclude matches:

```text
author:sock
type:episode theme:base
mod:arcanedimensions -author:sock
startmap:start
dependency:ad_v1_80p1final
tag:bsp2
```

## Build

```sh
cmake -S . -B build
cmake --build build
./build/slipgate
```

Slipgate expects Qt 6 Widgets/Network and an archive extractor on PATH. It prefers
`bsdtar`, then falls back to `unzip`.

## Data Locations

Downloaded archives are stored under Qt's app-data location, typically:

```text
~/.local/share/slipgate/downloads
```

Installed content goes into the detected or selected Steam Quake directory.

## Notes

Quaddicted metadata is read from:

```text
https://www.quaddicted.com/api/v1/
```

The default query restricts results to Quake single-player content with:

```text
+tags:"game=quake" +tags:"game_mode=singleplayer"
```
