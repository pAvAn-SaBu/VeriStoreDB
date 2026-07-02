# VeriStoreDB

> **A Git-Versioned Embedded Database Engine**  
> Built from scratch in C++17 — combining a relational database with Git-style version control.

---

## What is VeriStoreDB?

VeriStoreDB (`vsdb`) is a lightweight command-line database engine that lets you **version control your data** the same way Git versions your code.

Every `insert`, every schema change — you can `commit` a snapshot, browse the `log`, and `checkout` any past state of your database. Think of it as **"Ctrl+Z for your database"**.

---

## Features

- 📋 **Table Management** — Create tables with typed columns (`int`, `float`, `text`, `bool`)
- ➕ **Data Operations** — Insert and query records
- 🔍 **B-Tree Indexing** — Efficient in-memory indexing using a custom B-Tree implementation
- 📸 **Commit Snapshots** — Save the full database state with a message
- 🕓 **Commit Log** — Browse your history of commits with hashes and timestamps
- ⏪ **Checkout** — Restore the database to any previous commit
- 💾 **Persistent Storage** — All data and snapshots are stored on disk

---

## Architecture

```
VeriStoreDB/
├── src/
│   ├── main.cpp              # Entry point & command dispatcher
│   ├── cli/
│   │   ├── cli_parser.h      # Command definitions
│   │   └── cli_parser.cpp    # CLI11-based argument parsing
│   ├── db/
│   │   ├── database.h        # Database, Table, Schema, Record types
│   │   └── database.cpp      # Core database logic & disk I/O
│   ├── btree/
│   │   └── btree.h           # Generic templated B-Tree implementation
│   └── gitstore/
│       ├── gitstore.h        # Commit structure & GitStore interface
│       └── gitstore.cpp      # Version control engine (hash, snapshot, restore)
└── CMakeLists.txt            # Build config (C++17, CLI11 via FetchContent)
```

### Module Overview

| Module | Role |
|---|---|
| `cli/` | Parses command-line input using [CLI11](https://github.com/CLIUtils/CLI11) |
| `db/` | Manages tables, schemas, records, and disk persistence |
| `btree/` | Custom B-Tree for in-memory key-value indexing |
| `gitstore/` | Git-inspired object store — hashes, snapshots, HEAD tracking |

---

## How It Works

```
User Command (CLI)
       ↓
   CLIParser → ParsedCommand
       ↓
   Database (dispatch in main.cpp)
       ↓
  ┌──────────────┬─────────────────┐
  │  Table ops   │    GitStore     │
  │ (insert /    │ (commit / log / │
  │   select)    │   checkout)     │
  └──────────────┴─────────────────┘
        ↓                ↓
   .vsdb/data/      .vsdb/objects/
   (table files)    (versioned snapshots)
```

The `gitstore` module mirrors Git's object model:
- Each **commit** stores a hash, message, timestamp, parent hash, and hashes of all data files
- Data files are snapshotted into an `objects/` directory by content hash
- A `HEAD` file tracks the current commit

---

## Building

**Requirements:** CMake >= 3.14, a C++17 compiler (GCC, Clang, or MSVC)

```bash
# Clone the repo
git clone https://github.com/AKash-A007/VeriStoreDB.git
cd VeriStoreDB

# Configure & build
cmake -B build
cmake --build build

# The binary will be at:
# build/vsdb        (Linux/macOS)
# build/Debug/vsdb.exe  (Windows)
```

> CLI11 is automatically downloaded via CMake's `FetchContent` — no manual setup needed.

---

## Usage

### Initialize a database
```bash
vsdb init
```
Creates a `.vsdb/` directory in the current folder to store all data and version history.

### Create a table
```bash
vsdb create-table <table_name> --col <name>:<type> ...
```
```bash
# Example
vsdb create-table users --col id:int --col name:text --col age:int --col active:bool
```

### Insert a record
```bash
vsdb insert <table_name> --values <val1> <val2> ...
```
```bash
vsdb insert users --values 1 Alice 22 true
```

### Query a table
```bash
vsdb select <table_name>
```
```
id      name    age     active
-------- -------- -------- --------
1       Alice   22      true

1 rows returned
```

### Commit the current state
```bash
vsdb commit "Added first user"
```

### View commit history
```bash
vsdb log
```
```
Commit: a3f9c12e...
Date:   2026-07-02 09:15:00
        Added first user
```

### Restore a previous state
```bash
vsdb checkout <commit_hash>
```
```bash
vsdb checkout a3f9c12e
```

---

## Supported Data Types

| Type | Description |
|---|---|
| `int` | Integer values |
| `float` | Floating-point values |
| `text` | String values |
| `bool` | Boolean (`true` / `false`) |

---

## Git vs VeriStoreDB

| Concept | Git | VeriStoreDB |
|---|---|---|
| `init` | Initialize repo | Initialize database |
| `commit` | Snapshot source files | Snapshot database state |
| `log` | Browse commit history | Browse database history |
| `checkout` | Restore file state | Restore data state |
| Object store | `.git/objects/` | `.vsdb/objects/` |
| HEAD | Current branch tip | Current database commit |

---

## Roadmap

- [ ] `WHERE` clause filtering on `select`
- [ ] `UPDATE` and `DELETE` operations
- [ ] `vsdb diff <hash1> <hash2>` — compare two commits
- [ ] `vsdb status` — show uncommitted changes
- [ ] Branching and merging
- [ ] SQL-like query parser
- [ ] Data type validation on insert
- [ ] Persistent B-Tree index

---

## Author

**Akash A**  
*Built as a systems programming project demonstrating data structures, file I/O, CLI design, and version control concepts — all from scratch in C++.*
