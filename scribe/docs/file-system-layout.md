# File system layout

An example of the intended format of the file system layout is shown below and is focused around a central package storage.

```
/
├── bin -> usr/bin
├── boot
├── dev
├── etc
│   ├── pharos
│   │   └── system.toml
│   ├── scribe
│   │   ├── scribe.toml
│   │   ├── repositories.toml
│   │   └── keys
│   ├── systemd
│   ├── hostname
│   ├── hosts
│   ├── fstab
│   ├── passwd
│   ├── group
│   ├── shadow
│   ├── os-release
│   └── machine-id
├── home
├── lib -> usr/lib
├── lib32 -> usr/lib32
├── lib64 -> usr/lib64
├── library
│   ├── works
│   │   └── <name>-<version>-<revision>-<arch>--<hash>
│   │       ├── bin
│   │       ├── sbin
│   │       ├── lib
│   │       ├── include
│   │       ├── share
│   │       ├── libexec
│   │       ├── etc-defaults
│   │       ├── systemd
│   │       ├── manifest.toml
│   │       └── build-info.toml
│   ├── generations
│   │   └── system
│   │       ├── 0
│   │       │   ├── usr
│   │       │   │   ├── bin
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/bin/<file>
│   │       │   │   ├── sbin
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/sbin/<file>
│   │       │   │   ├── lib
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/lib/<file>
│   │       │   │   ├── lib32
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-i686--<hash>/lib/<file>
│   │       │   │   ├── lib64
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-x86_64--<hash>/lib/<file>
│   │       │   │   ├── include
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/include/<file>
│   │       │   │   ├── share
│   │       │   │   │   └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/share/<file>
│   │       │   │   └── libexec
│   │       │   │       └── <file> -> /library/works/<name>-<version>-<revision>-<arch>--<hash>/libexec/<file>
│   │       │   ├── bin -> usr/bin
│   │       │   ├── sbin -> usr/sbin
│   │       │   ├── lib -> usr/lib
│   │       │   ├── lib32 -> usr/lib32
│   │       │   └── lib64 -> usr/lib64
│   │       └── <generation>
│   │           ├── usr
│   │           │   ├── bin
│   │           │   ├── sbin
│   │           │   ├── lib
│   │           │   ├── lib32
│   │           │   ├── lib64
│   │           │   ├── include
│   │           │   ├── share
│   │           │   └── libexec
│   │           ├── bin -> usr/bin
│   │           ├── sbin -> usr/sbin
│   │           ├── lib -> usr/lib
│   │           ├── lib32 -> usr/lib32
│   │           └── lib64 -> usr/lib64
│   ├── profiles
│   │   ├── system -> ../generations/system/<generation>
│   │   └── rescue -> ../generations/system/<generation>
│   ├── pins
│   │   ├── manual
│   │   └── rescue
│   └── tmp
├── media
├── mnt
├── opt
├── proc
├── root
├── run
│   ├── scribe
│   ├── systemd
│   └── user
├── sbin -> usr/sbin
├── srv
│   ├── compose
│   ├── data
│   ├── media
│   ├── www
│   └── backups
├── sys
├── tmp
├── usr
│   ├── bin -> /library/profiles/system/usr/bin
│   ├── sbin -> /library/profiles/system/usr/sbin
│   ├── lib -> /library/profiles/system/usr/lib
│   ├── lib32 -> /library/profiles/system/usr/lib32
│   ├── lib64 -> /library/profiles/system/usr/lib64
│   ├── include -> /library/profiles/system/usr/include
│   ├── share -> /library/profiles/system/usr/share
│   ├── libexec -> /library/profiles/system/usr/libexec
│   ├── local
│   └── src
└── var
    ├── lib
    │   ├── scribe
    │   │   ├── ledger.sqlite
    │   │   ├── locks
    │   │   ├── state
    │   │   ├── recipes
    │   │   └── transactions
    │   └── systemd
    ├── cache
    │   └── scribe
    ├── log
    │   └── scribe
    ├── tmp
    └── spool
```
