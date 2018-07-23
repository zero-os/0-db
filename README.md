# 0-db [![Build Status](https://travis-ci.org/rivine/0-db.svg?branch=master)](https://travis-ci.org/rivine/0-db) [![codecov](https://codecov.io/gh/rivine/0-db/branch/master/graph/badge.svg)](https://codecov.io/gh/rivine/0-db)  
0-db is a super fast and efficient key-value store redis-protocol (mostly) compatible which
makes data persistant inside an always append datafile, with namespaces support.

Indexes are created to speedup restart/reload process, this index is always append too,
except in direct-mode (see below for more information).

We use it as backend for many of our blockchain work and might replace redis for basic
SET and GET request.

# Quick links
1. [Build targets](#build-targets)
2. [Build instructions](#build-instructions)
3. [Always append](#always-append)
4. [Running modes](#running-modes)
5. [Implementation](#implementation)
6. [Supported commands](#supported-commands)
7. [Namespaces](#namespaces)
8. [Hook system](#hook-system)
9. [Limitation](#limitation)
10. [Tests](#tests)

# Build targets
Currently supported system:
* Linux (using `epoll`)
* MacOS and FreeBSD (using `kqueue`)

Currently supported hardware:
* Any processor supporting `SSE 4.2`

This project won't compile on something else (for now).

# Build instructions
To build the project (server, tools):
* Type `make` on the root directory
* The binaries will be placed on `bin/` directory

You can build each parts separatly by running `make` in each separated directories.

> By default, the code is compiled in debug mode, in order to use it in production, please use `make release`

# Always append
Data file (files which contains everything, included payload) are **in any cases** always append:
any change will result in something appened to files. Data files are immuables. If any suppression is
made, a new entry is added, with a special flag. This have multiple advantages:
- Very efficient in HDD (no seek when writing batch of data)
- More efficient for SSD, longer life, since overwrite doesn't occures
- Easy for backup or transfert: incremental copy work out-of-box
- Easy for manipulation: data is flat

Of course, when we have advantages, some cons comes with them:
- Any overwrite won't clean previous data
- Deleting data won't actually delete anything in realtime
- You need some maintenance to keep your database not exploding

Hopefuly, theses cons have their solution:
- Since data are always append, you can at any time start another process reading that database
and rewrite data somewhere else, with optimization (removed non-needed files). This is what we call
`compaction`, and some tools are here to do so.
- As soon as you have your new files compacted, you can hot-reload the database and profit, without
loosing your clients (small freeze-time will occures, when reloading the index).

Data files are never reach directly, you need to always hit the index first.

Index files are always append in all modes, except in `direct-mode`. The direct mode is explained below,
but basicly in this mode, the key depends on the position in the index file. This exception
make the suppression of that key an **obligation** to edit this entry in place.

Otherwise, index works like data files, with more or less the same data (except payload) and
have the advantage to be small and load fast (can be fully populated in memory for processing).

# Running modes
On runtime, you can choose between multiple mode:
* `user`: user-key mode
* `seq`: sequential mode
* `direct`: direct-position key mode

**Warning**: in any case, please ensure data and index directories used by 0-db are empty, or
contains only valid database namespaces directories.

## User Key
This is a default mode, a simple key-value store. User can `SET` their own keys, like any key-value store.

Even in this mode, the key itself is returned by the `SET` command.

## Sequential
In this mode, the key is a sequential key autoincremented.

You need to provide a null-length key, to generate a new key.
If you provide a key, this key should exists (a valid generated key), and the `SET` will update that key.

Providing any other key will fails.

The id is a little-endian integer key. All the keys are kept in memory.

## Direct Key
This mode works like the sequential mode, except that returned key contains enough information to fetch the
data back, without using in-memory index.

There is no update possible in this mode (since the key itself contains data to the real location
and we use always append method, we can't update existing data). Providing a key has no effect and
is ignored. Only a suppression will modify the index, to flag the entry as deleted.

The key returned by the `SET` command is a binary key.

# Implementation
This project doesn't rely on any dependencies, it's from scratch.

A rudimental and very simplified RESP protocol is supported, allowing only some commands. See below.

Each index files contains a 27 bytes headers containing a magic 4 bytes identifier,
a version, creation and last opened date and it's own file-sequential-id. In addition it contains
the mode used when it was created (to avoid mixing mode on different run).

For each entries on the index, 36 bytes (28 bytes + pointer for linked list) 
plus the key itself (limited to 256 bytes) will be consumed.

The data (value) files contains a 26 bytes headers, mostly the same as the index one
and each entries consumes 18 bytes (1 byte for key length, 4 bytes for payload length, 4 bytes crc,
4 bytes for previous offset, 1 byte for flags, 4 byte for timestamp) plus the key and payload.

> We keep track of the key on the data file in order to be able to rebuild an index based only on datafile if needed.

Each time a key is inserted, an entry is added to the data file, then on the index.
Whenever the key already exists, it's appended to disk and entry in memory is replaced by the new one.

Each time the server starts, it loads (basicly replay) the index in memory. The index is kept in memory 
**all the time** and only this in-memory index is reached to fetch a key, index files are
never read again except during startup (except for 'direct-key mode', where the index is not in memory)
or in direct-mode where key is the location on the index.

When a key-delete is requested, the key is kept in memory and is flagged as deleted. A new entry is added
to the index file, with the according flags. When the server restart, the latest state of the entry is used.

## Index
The current index in memory is a really simple implementation (to be improved).

It uses a rudimental kind-of hashtable. A list of branchs (2^24) is pre-allocated.
Based on the crc32 of the key, we keep 24 bits and uses this as index in the branches.

Branches are allocated only when used. Using 2^24 bits will creates 16 million index entries
(128 MB on 64 bits system).
Each branch (when allocated) points to a linked-list of keys (collisions).

When the branch is found based on the key, the list is read sequentialy.

## Read-only
You can run 0-db using a read-only filesystem (both for keys or data), which will prevent
any write and let the 0-db serving existing data. This can, in the meantime, allows 0-db
to works on disks which contains failure and would be remounted in read-only by the kernel.

This mode is not possible if you don't have any data/index already available.

# Supported commands
- `PING`
- `SET key value`
- `GET key`
- `DEL key`
- `STOP` (used only for debugging, to check memory leaks)
- `EXISTS key`
- `CHECK key`
- `INFO`
- `NSNEW namespace`
- `NSDEL namespace`
- `NSINFO namespace`
- `NSLIST`
- `NSSET namespace property value`
- `SELECT namespace`
- `DBSIZE`
- `TIME`
- `AUTH password`
- `SCAN [optional key]`
- `RSCAN [optional key]`
- `WAIT command`

`SET`, `GET` and `DEL`, `SCAN` and `RSCAN` supports binary keys.

> Compared to real redis protocol, during a `SET`, the key is returned as response.

## EXISTS
Returns 1 or 0 if the key exists

## CHECK
Check internally if the data is corrupted or not. A CRC check is done internally.
Returns 1 if integrity is validated, 0 otherwise.

## SCAN
Walk forward over a dataset (namespace).

- If `SCAN` is called without argument, it returns the first key (first in time) available in the dataset.
- If `SCAN` is called with an argument, it returns the next key after the argument.

If the dataset is empty, or you reach the end of the chain, `-No more data` is returned.

If you provide a non-existing (or deleted) key as argument, `-Invalid index` is returned.

Otherwise, an array (like redis `SCAN`) is returned. Only the first item is relevant, and it's the next
key expected.

By calling `SCAN` with each time the key responded on the previous call, you can walk forward a complete
dataset.

## RSCAN
Same as scan, but backward (last-to-first key)

## NSNEW
Create a new namespace. Only admin can do this.

By default, a namespace is not password protected, is public and not size limited.

## NSDEL
Delete a namespace. Only admin can do this.

Warning:
- You can't remove the namespace you're currently using.
- Any other clients using this namespace will be moved to a special state, awaiting to be disconnected.

## NSINFO
Returns basic informations about a namespace

## NSLIST
Returns an array of all available namespaces.

## NSSET
Change a namespace setting/property. Only admin can do this.

Properties:
* `maxsize`: set the maximum size in bytes, of the namespace's data set
* `password`: lock the namespace by a password, use `*` password to clear it
* `public`: change the public flag, a public namespace can be read-only if a password is set

## SELECT
Change your current namespace. If the requested namespace is password-protected, you need
to add the password as extra parameter. If the namespace is `public` and you don't provide
any password, the namespace will be accessible in read-only.

## AUTH
If an admin account is set, use `AUTH` command to authentificate yourself as `ADMIN`.

## WAIT
Blocking wait on command execution by someone else. This allows you to wait somebody else
doing some commands. This can be useful to avoid polling the server if you want to do periodic
queries (like waiting for a SET).

Wait takes one argument: a command name to wait for. The event will only be triggered for clients
on the same namespace as you (same SELECT)

This is the only blocking function right now. In server side, your connection is set on pending and
you won't receive anything until someone executed the expected command.

# Namespaces
A namespace is a dedicated directory on index and data root directory.
A namespace is a complete set of key/data. Each namespace can be optionally protected by a password
and size limited.

You are always attached to a namespace, by default, it's namespace `default`.

# Hook System
You can request 0-db to call an external program/script, as hook-system. This allows the host
machine running 0-db to adapt itself when something happen.

To use the hook system, just set `--hook /path/to/executable` on argument.
The file must be executable, no shell are invoked.

When 0-db starts, it create it own pseudo `identifier` based on listening address/port/socket.
This id is used on hooks arguments.

First argument is `Hook Name`, second argument is `Generated ID`, next arguments depends of the hook.

Current supported hooks:

| Hook Name             | Action                  | Arguments                  |
| --------------------- | ----------------------- | -------------------------- |
| `ready`               | Server is ready         | (none)                     |
| `close`               | Server closing (nicely) | (none)                     |
| `jump`                | Data/Index incremented  | Closing and New index file |
| `crash`               | Server crashed          | (none)                     |
| `namespace-created`   | New namespace created   | Namespace name             |
| `namespace-deleted`   | Namespace removed       | Namespace name             |
| `namespace-reloaded`  | Namespace reloaded      | Namespace name             |

# Limitation
By default, each datafile is split when bigger than 256 MB.

The datafile id is stored on 16 bits, which makes maximum of 65536 files.
A database can be maximum ~16 TB. Since one single 0-db is made to be used on a single dedicated disk,
this should be good, but that's still a hard limitation. This limitation can be changed by changing
the datafile split size (`data.h`: `DATA_MAXSIZE`), limitation will always be `2^16 * DATA_MAXSIZE`.

# Tests
You can run a sets of test on a running 0-db instance.
Theses tests (optional) requires `hiredis` library.

To build the tests, type `make` in the `tests` directory.

To run the tests, run `./zdbtests` in the `tests` directory.

Warning: for now, only a local 0-db using `/tmp/zdb.sock` unix socket is supported.

Warning 2: please use an empty database, otherwise tests may fails as false-positive issue.
