# VeriStoreDB

> **A Git-Versioned Embedded Database Engine**  
> Built from scratch in C++17 — combining a relational database with Git-style version control.

---

## What is VeriStoreDB?

VeriStoreDB (`vsdb`) is a lightweight command-line database engine that lets you **version control your data** the same way Git versions your code.

Every `insert`, every schema change — you can `commit` a snapshot, browse the `log`, perform **time-travel queries**, isolate experiments into **branches**, and `restore` any past state of your database. Think of it as **"Ctrl+Z for your database"**.

---

## Features

- 📋 **Table Management** — Create tables with typed columns (`int`, `float`, `text`, `bool`)
- ➕ **Data Operations** — Insert and query records
- 🔍 **B-Tree Indexing** — Efficient in-memory indexing using a custom B-Tree implementation
- 📸 **Commit Snapshots** — Save the full database state with a message
- 🕓 **Commit Log** — Browse your history of commits with hashes and timestamps
- ⏳ **Time-Travel Queries** — Run a `select` query to see data exactly as it was at a specific past commit
- ⏪ **Safe Restore** — Restore the database to a previous state safely without losing history (creates a new commit)
- 🌿 **Branching** — Isolate table structures and data changes in separate, isolated branches
- 💾 **Persistent Storage** — All data, schemas, refs, and snapshots are stored on disk

---

## Architecture

```
VeriStoreDB/
├── src/
│   ├── main.cpp              # Entry point & command dispatcher
│   ├── cli/
│   │   ├── cli_parser.h      # Command definitions (CLI11 wrapper)
│   │   └── cli_parser.cpp    # Argument and subcommand parsing
│   ├── db/
│   │   ├── database.h        # Database, Table, Schema, Record types
│   │   └── database.cpp      # Core database logic & disk I/O
│   ├── btree/
│   │   └── btree.h           # Generic templated B-Tree implementation
│   └── gitstore/
│       ├── gitstore.h        # Commit structure & GitStore interface
│       └── gitstore.cpp      # Version control engine (hash, snapshot, branches)
└── CMakeLists.txt            # Build config (C++17, CLI11 via FetchContent)
```

The `gitstore` module tightly mirrors Git's object model:
- Each **commit** stores a hash, message, timestamp, parent hash, and hashes of all data files.
- Data files are snapshotted into an `objects/` directory by content hash.
- Branch pointers are stored in a `refs/` directory.
- A `HEAD` file tracks the current commit or branch reference (e.g., `ref: main`).

---

## Building

**Requirements:** CMake >= 3.14 (up to 4.3), a C++17 compiler (GCC, Clang, or MSVC)

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

## Usage Guide

### 1. Initialize a database
```bash
vsdb init
```
Creates a folder to store your active tables (`data/`), version objects (`objects/`), and branches (`refs/`). You start on the default `main` branch.

### 2. Create a table
```bash
vsdb create <table_name> --columns <name>:<type> ...
```
```bash
# Example
vsdb create users --columns id:int name:text age:int active:bool
```

### 3. Insert and Query Records
```bash
vsdb insert users --values 1 Alice 22 true
vsdb select users
```

### 4. Committing Changes
Snapshot your changes to preserve them:
```bash
vsdb commit -m "Added first user"
```

### 5. Time-Travel Queries
Want to see what a table looked like two weeks ago, without actually reverting your database? Use the `--at` flag with a past commit hash:
```bash
vsdb select users --at a3f9c12e
```
This reads the historic snapshot entirely in-memory—your current working tables stay completely untouched!

### 6. Branching
Branching allows you to create parallel, isolated versions of your database.

```bash
# List all branches (* marks the current branch)
vsdb branch

# Create a new branch pointing to the current commit
vsdb branch -b feature_x

# Switch to the new branch (safely swaps your data directory)
vsdb branch feature_x
```
Any tables created or rows inserted on `feature_x` will not bleed into `main`. When you switch back to `main`, your files are safely swapped back to their `main` state.

### 7. View History
```bash
vsdb log
```
Shows a chronological list of commits, and points out where you currently are using `(HEAD -> branch)`.

### 8. Safe Restore
Messed up? You can rewind the state of the entire database to an old commit.
```bash
vsdb restore <commit_hash>
```
Unlike a traditional "hard checkout", `restore` is **non-destructive**. It restores your files, and then immediately wraps that restored state into a **brand-new commit**. This ensures you never orphan or lose your timeline history!

---

## Supported Data Types

| Type | Description |
|---|---|
| `int` | Integer values |
| `float` | Floating-point values |
| `text` | String values (without spaces) |
| `bool` | Boolean (`true` / `false` or `1` / `0`) |

---

## Roadmap

- [x] Time-Travel Queries (`select --at <hash>`)
- [x] Safe Non-Destructive Restore (`restore <hash>`)
- [x] Branching and Isolation (`branch`)
- [ ] `WHERE` clause filtering on `select`
- [ ] `UPDATE` and `DELETE` operations
- [ ] `vsdb diff <hash1> <hash2>` — compare two commits
- [ ] SQL-like query parser
- [ ] Data type validation on insert
- [ ] Persistent B-Tree index (currently in-memory only)

---

## Author

**Akash A**  
*Built as a systems programming project demonstrating data structures, file I/O, CLI design, and version control concepts — all from scratch in C++.*
