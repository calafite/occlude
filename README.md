# occlude

`occlude` is a daemonized wallpaper manager designed to isolate wallpapers based on dynamic visibility modes (`Safe` and `Unsafe`). It works by segregating physical files across isolated filesystem directories, actively monitoring download directories for automatic ingestion, and providing both command-line (CLI) and terminal user interface (TUI) clients to control state and classification.

---

## Overview

The system is split into three main binaries and a shared library:
*   **`occluded`**: The background daemon. It coordinates wallpaper states, manages directory structures, operates the background scanning and automatic cycling threads, executes shell templates to apply images, validates media files, and exposes a Unix domain socket IPC server.
*   **`occlude`**: A command-line companion client used for quick keybindings, shell scripts, and querying daemon status.
*   **`occlude-tui`**: An interactive terminal-based interface driven by FTXUI, displaying tracked files, sorting criteria, metadata, search filtering, external previews, bulk selection, and configuration modals.
*   **`occlude_lib`**: The static core library implementing atomic filesystem abstractions, SHA-256 stream hashing, binary state database persistence, and internal routing.

---

## Installation & Setup

### Prerequisites

You need a C++26 compliant compiler (e.g., GCC 14+ or Clang 18+) and CMake 3.22+.

### Build from Source

```bash
git clone https://github.com/calafite/occlude.git
cd occlude
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

This compiles three executables in the `build` directory:
*   `apps/daemon/occluded`
*   `apps/cli/occlude`
*   `apps/tui/occlude-tui`

### Quick Start

1. Start the daemon in the background or as a systemd service:
   ```bash
   ./occluded &
   ```
2. Verify connectivity with the CLI:
   ```bash
   ./occlude status
   ```
3. Run the interactive interface:
   ```bash
   ./occlude-tui
   ```

---

## Configuration

On initial startup, `occluded` generates a default configuration file under `~/.config/occlude/config.json`. 

```json
{
  "publicRoot": "/home/username/.config/occlude/public",
  "privateRoot": "/home/username/.config/occlude/private",
  "unclassifiedRoot": "/home/username/.config/occlude/unclassified",
  "manifestPath": "/home/username/.config/occlude/manifest.bin",
  "setterCommandTemplate": "noctalia msg wallpaper-set {path}",
  "getterCommandTemplate": "noctalia msg wallpaper-get",
  "defaultDownloadDirectory": "",
  "scanIntervalSeconds": 30,
  "cycleIntervalSeconds": 0,
  "defaultIngestionVisibility": "unclassified"
}
```

### Settings Reference

*   `publicRoot`: Directory where **Safe** wallpapers are physically relocated.
*   `privateRoot`: Directory where **Unsafe** wallpapers are physically relocated.
*   `unclassifiedRoot`: Directory where quarantined, unclassified wallpapers are held.
*   `manifestPath`: Path to the binary database file tracking hashes, states, and history.
*   `setterCommandTemplate`: Shell command executed to change wallpapers. The `{path}` placeholder is replaced with the absolute path of the chosen wallpaper.
*   `getterCommandTemplate`: Shell command executed to check what wallpaper is currently applied.
*   `defaultDownloadDirectory`: Path (e.g. `~/Downloads`) that the daemon periodically scans for incoming images. The scanner checks both file extensions and magic bytes (`.jpg`, `.png`, `.webp`, `.gif`) to prevent spoofing. Leave empty to disable background scanning.
*   `scanIntervalSeconds`: Frequency of background scans in seconds.
*   `cycleIntervalSeconds`: Frequency of automatic background wallpaper cycling in seconds. Set to `0` to disable auto-cycling.
*   `defaultIngestionVisibility`: Target category for automatically discovered wallpapers. Options are `"safe"`, `"unsafe"`, `"unclassified"`, or `"current"` (inherits the active system mode).

---

## Visibility Isolation

`occlude` guarantees hard isolation between wallpapers depending on the daemon's active visibility state mode:

| Active Mode | Eligible Wallpapers | File Placement |
| :--- | :--- | :--- |
| **Safe Mode** | `Safe` only | `publicRoot` |
| **Unsafe Mode** | `Safe` + `Unsafe` | `publicRoot` or `privateRoot` |
| **Quarantined** | None (Uneligible) | `unclassifiedRoot` |

### Core Rules

1.  **Strict State Confinement**: When switching to `Safe` mode, if the current active wallpaper is evaluated as `Unsafe`, `occlude` instantly swaps it to the next available `Safe` wallpaper.
2.  **Quarantine**: Newly ingested wallpapers placed in `unclassifiedRoot` are quarantined. They cannot be set, cycled, or viewed until classified as `Safe` or `Unsafe`.
3.  **Cross-Device Atomic Operations**: Moving files between these directories falls back gracefully to copy-then-delete in the event of cross-partition or cross-filesystem links (`EXDEV`), ensuring database-to-disk consistency.

---

## CLI Client

The command-line tool `occlude` is used to trigger operations or query status synchronously over IPC.

### Commands

```text
Usage: occlude <command> [args...]

Commands:
  cycle                 Move to the next eligible wallpaper based on the oldest-shown algorithm
  toggle                Toggle between Safe and Unsafe modes
  set_mode <mode>       Set mode explicitly (safe/unsafe)
  ingest_safe <path>    Add a new safe wallpaper, hashing and relocating it to publicRoot
  ingest_unsafe <path>  Add a new unsafe wallpaper, hashing and relocating it to privateRoot
  classify <hash> <vis> Change visibility of a wallpaper (safe/unsafe/unclassified)
  status                Print daemon status and active mode
  current               Get the active wallpaper path from the setter/getter command template
  list                  Show formatted table of all tracked wallpapers
  scan                  Force manual directory scan of downloads folder
```

### Example Output

#### `occlude list`
```text
HASH     │ VISIBILITY   │ DATE                │ PATH
─────────┼──────────────┼─────────────────────┼────────────────────────────────────────
a18b14cf │ Safe         │ 2026-07-03 20:23:11 │ /home/user/.config/occlude/public/landscape.jpg
f94bb2a0 │ Unsafe       │ 2026-07-03 20:25:04 │ /home/user/.config/occlude/private/night_city.jpg
c238fa01 │ Unclassified │ 2026-07-03 20:25:30 │ /home/user/.config/occlude/unclassified/img1.png
```

#### `occlude status`
```text
Current Mode: SAFE
```

## Terminal User Interface (TUI)

The `occlude-tui` program provides a mouse-supported interactive screen to manage, filter, and modify wallpaper classifications. The interface includes a streamlined help bar at the bottom containing essential hotkeys to keep the workspace clean and uncluttered.

<img width="1918" height="1078" alt="image" src="https://github.com/user-attachments/assets/0bd425a1-3c72-4864-89e3-f8234594ff54" />

### TUI Keybindings

*   `?` : Toggle the detailed **Keybinds & Help** overlay.
*   `/` : Focus search input field.
*   `Escape` : Unfocus search input field (returns control to list navigation) or close any active modals.
*   `j` / `k` (or `Up` / `Down` arrows) : Navigate wallpaper list.
*   `Space` : Toggle multi-selection for the currently highlighted wallpaper.
*   `*` : Toggle multi-selection for **all** wallpapers currently visible in the filtered list.
*   `a` : Apply the selected wallpaper immediately.
    *   *Note: In Safe mode, trying to apply an Unsafe or Unclassified wallpaper triggers a warning and blocks execution.*
*   `p` : Securely launch an external image preview of the highlighted wallpaper (does not block the TUI).
*   `v` : Open the **Classify** selection modal. Operates on all selected wallpapers if a bulk selection is active.
*   `r` : Open the **Rename** text input modal (disabled during bulk selection).
*   `d` : Open the **Delete** confirmation modal. Operates on all selected wallpapers if a bulk selection is active.
*   `S` : Cycle list sorting criteria (by Name, Date, or Visibility).
*   `s` : Trigger background filesystem scanner immediately.
*   `c` : Force-cycle the wallpaper.
*   `t` : Toggle visibility modes modal (`Safe` <-> `Unsafe`).
*   `q` : Quit `occlude-tui`.

## State & Manifest File Format

To safeguard state consistency, `occlude` writes metadata to a structured binary file specified in `manifestPath` (defaulting to `manifest.bin`). Saving is completely transaction-safe: the engine writes out a `.tmp` file, performs an `fsync()`, and renaming/relocating replaces the target atomically.

### Binary Spec (v1)

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Magic Bytes ('O','C','C','L')             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Version Number (1)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| StateMode(u8) | PublicCurrentPresent | PrivateCurrentPresent  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          PublicCurrent String Value (Length + Raw Bytes)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          PrivateCurrent String Value (Length + Raw Bytes)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Wallpaper Entry Count (uint64_t)             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                        Wallpaper Entries                      |
|                                                               |
```

#### Individual Wallpaper Entry Layout:
1.  **Absolute Path**: String (uint32 length prefixed + UTF-8 payload).
2.  **SHA-256 Hash**: Fixed 32 bytes binary digest.
3.  **Creation Timestamp**: Int64 nanoseconds since Epoch.
4.  **Visibility Class**: Uint8 (`0` = Safe, `1` = Unsafe, `2` = Unclassified).
5.  **Last Shown Flag**: Uint8 (`1` = Present, `0` = Null).
6.  **Last Shown Timestamp**: (Optional) Int64 nanoseconds since Epoch.

---

## IPC Protocol

The daemon exposes an IPC server over Unix Domain Sockets. The socket file is located at `$XDG_RUNTIME_DIR/occlude.sock` if the directory environment variable exists; otherwise, it falls back to `/tmp/occlude.sock`.
Every IPC transaction is synchronous. The client initiates a connection, transmits a single instruction terminated by a `\n` character, reads back a response terminated by a `\n` character, and terminates the session.

### Protocol Command Specification

*   `CYCLE`: Advances the active theme. Returns `OK <msg>` or `ERR <reason>`.
*   `TOGGLE`: Toggles standard state mode. Returns `OK <msg>` or `ERR <reason>`.
*   `STATUS`: Outputs the standard active status header. Returns `OK <msg>`.
*   `LIST`: Returns a list layout separated internally by vertical tabs (`\v`).
*   `CURRENT`: Returns current wallpaper active on the screen.
*   `SCAN`: Forces scanner run. Returns `OK <msg>`.
*   `CLASSIFY <hash> <safe|unsafe|unclassified>`: Relocates file to target directory and alters database state. Returns `OK <msg>` or `ERR <reason>`.
*   `INGEST_SAFE <path>` / `INGEST_UNSAFE <path>`: Instructs daemon to ingest target file. Returns `OK <msg>` or `ERR <reason>`.
*   `DUMP`: Returns a serialized JSON string containing active properties, daemon states, and complete wallpaper collections. Used directly by `occlude-tui` to update its components.
*   `APPLY <hash>`: Directly forces wallpaper transition to standard target path. Returns `OK <msg>` or `ERR <reason>`.
*   `DELETE <hash>`: Physically removes the file from the filesystem and database track. Returns `OK <msg>` or `ERR <reason>`.
*   `RENAME <hash> <new_name>`: Renames the actual target file on filesystem and updates paths in manifest. Returns `OK <msg>` or `ERR <reason>`.
*   `SET_MODE <safe|unsafe>`: Directly sets state mode. Returns `OK <msg>` or `ERR <reason>`.
