# 1sT-Ranks

Metamod:Source plugin for CS2 that reads rank data from the existing `ranks`
entry in `addons/configs/databases.cfg` and displays fake scoreboard rank icons.

This plugin is read-only against the ranks database. It does not create tables,
alter schema, or update player stats.

## Required server plugins

- Metamod:Source
- Pisex `cs2-menus` / Utils plugin, for `IUtilsApi`
- `sql_mm` with MySQL support, for `SQLInterface`

## Database config

Use this exact connection name in `addons/configs/databases.cfg`:

```text
"ranks"
{
    "host"      "<mysql-host>"
    "database"  "<database-name>"
    "user"      "<user>"
    "pass"      "<password>"
    "port"      "3306"
}
```

The plugin reads `steam`, `rank`, and `value` from the table configured in
`addons/configs/1sT-Ranks.cfg` (`lvl_base` by default).

## Build

Build the same way Pisex Metamod plugins are built:

```bash
python3 configure.py --hl2sdk-root /path/to/hl2sdk-root --mms_path /path/to/metamod-source --hl2sdk-manifests hl2sdk-manifests --sdks cs2 --targets x86_64
ambuild
```

The Pisex build template expects the usual CS2 SchemaEntity headers/source to be
available next to the plugin source, the same as `LR_FakeRanks`.

## Install

Copy the built binary and VDF to the server:

```text
game/csgo/addons/1sT-Ranks/bin/1sT-Ranks.so
game/csgo/addons/metamod/1sT-Ranks.vdf
game/csgo/addons/configs/1sT-Ranks.cfg
```

Console commands:

- `mm_ranks_reload` reloads config and refreshes DB cache.
- `mm_ranks_refresh` refreshes DB cache only.
