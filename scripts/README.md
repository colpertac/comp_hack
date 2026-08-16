# COMP_hack local + systemd helper scripts

These scripts automate the setup that got the Wyrd Linux build running on modern Debian.

## Quick start (local, no systemd)

```bash
cd /home/cat/repos/smt/comp_hack

# 1) Build lobby/world/channel
./scripts/build.sh

# 2) Install config + datastore + client BinaryData/QMP
./scripts/setup.sh

# 3) Start servers (lobby -> world -> channel)
./scripts/start.sh

# Status / stop
./scripts/status.sh
./scripts/stop.sh
```

Default client path used by `setup.sh`:

```text
/home/cat/software/smt/game/reimagine
```

Override if needed:

```bash
CLIENT_DIR=/path/to/client ./scripts/setup.sh
EXTERNAL_IP=127.0.0.1 ./scripts/setup.sh
```

## Paths

Portable **runtime root** (no `/etc` or `/var` required):

| Path | Purpose |
|------|---------|
| `build-current/bin/comp_*` | Built binaries |
| `runtime/config/` | Config, `constants.xml`, `newcharacter.xml` |
| `runtime/datastore/` | Server content + client BinaryData/QMP |
| `runtime/database/` | SQLite databases |
| `runtime/logs/` | Server log files |
| `.run/` | Local pid files / stdout captures |

Migrate an old FHS install once: `./scripts/migrate-runtime.sh`

## Optional: systemd (boot + auto-restart)

Do this only after local start works. Stop local processes first.

```bash
./scripts/stop.sh
sudo ./scripts/install-systemd.sh

sudo systemctl start comp_lobby
sudo systemctl start comp_world
sudo systemctl start comp_channel

sudo systemctl status comp_channel
sudo journalctl -u comp_lobby -u comp_world -u comp_channel -f
```

Uninstall units:

```bash
sudo ./scripts/uninstall-systemd.sh
```

## Rebuild notes

`build.sh` already uses:

```bash
cmake -S . -B build-current \
  -DNO_WARNINGS=ON \
  -DDISABLE_TESTING=ON \
  -DUSE_SYSTEM_ZLIB=ON \
  -DUSE_SYSTEM_OPENSSL=ON \
  -DUSE_SYSTEM_PHYSFS=ON \
  -DUSE_SYSTEM_MARIADB=ON
```

and applies the known modern-distro workarounds (squirrel headers, civet/squirrel library name symlinks, googletest GCC 14 patch).

Force a clean reconfigure:

```bash
RECONFIGURE=1 ./scripts/build.sh
```

## Config notes

`setup.sh` installs `contrib/winconfig/newcharacter.xml` as
`/etc/comp_hack/newcharacter.xml`. That file grants starting skills such as
Rest, Summon, and Basic Attack. Without it, new characters cannot sit, summon,
or use the basic skill bar even though the channel still starts.

## Warnings you may still see

Missing spot files / one missing QMP / duplicate mapping warnings are usually incomplete or unused client assets. If you can log in and play, they are not blockers.
