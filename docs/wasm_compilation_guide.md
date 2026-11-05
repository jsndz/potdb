# potDB WebAssembly (Wasm) Compilation Guide with Emscripten

This guide explains how to compile **potDB** (PTDB) into WebAssembly using the **Emscripten** toolchain (`emcc`) to run the storage engine **100% client-side inside any modern web browser**.

---

## 1. Why WebAssembly + Emscripten?

Web browsers execute JavaScript inside a sandboxed virtual machine without access to local POSIX system calls or raw machine code (`.so` / `.a`). 

**Emscripten (`emcc`)** solves this by:
1. Compiling standard C99 source code into a compact binary (`potdb.wasm`, ~35 KB).
2. Providing a **Virtual File System (`FS`)** that transparently emulates POSIX file operations (`open`, `write`, `lseek`, `close`, `fsync`) in browser memory or persistent **IndexedDB**.
3. Generating JavaScript glue code (`potdb.js`) that provides helper bridges (`ccall`, `cwrap`, `FS`) to invoke C functions directly.

```
┌────────────────────────────────────────────────────────┐
│                   Browser Tab Sandbox                  │
│                                                        │
│   HTML5 Geolocation (GPS)                              │
│             │ (lat, lng, timestamp)                   │
│             ▼                                          │
│   app.js (JavaScript Controller)                       │
│             │ Module.cwrap('ptdb_insert_2d')           │
│             ▼                                          │
│   potdb.wasm (C Engine in WebAssembly)                 │
│             │ POSIX open(), write(), lseek()           │
│             ▼                                          │
│   Emscripten Virtual Filesystem (FS)                   │
│             ├── MEMFS: Fast In-Memory Buffer           │
│             └── IDBFS: Persistent Browser IndexedDB    │
└────────────────────────────────────────────────────────┘
```

---

## 2. Installing Emscripten

### Option A: Via Arch Linux / CachyOS (Fastest)
```bash
sudo pacman -S emscripten
```

### Option B: Via Official Portable SDK (Cross-Platform)
Works on Linux, macOS, and Windows (via WSL or PowerShell):

```bash
# 1. Clone official Emscripten SDK repository
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# 2. Download and install the latest toolchain
./emsdk install latest

# 3. Activate PATH and environment variables
./emsdk activate latest
source ./emsdk_env.sh
```

Verify your installation:
```bash
emcc --version
```

---

## 3. The Compilation Command

To compile the core storage engine into WebAssembly, run `emcc`:

```bash
mkdir -p web
emcc -O3 -Iinclude \
  src/crc32.c src/writer.c src/reader.c \
  -s WASM=1 \
  -s FORCE_FILESYSTEM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS=_ptdb_init,_ptdb_open,_ptdb_close,_ptdb_insert_2d,_ptdb_insert_3d,_ptdb_query_range_2d,_ptdb_reader_open,_ptdb_reader_close,_malloc,_free \
  -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,FS \
  -o web/potdb.js
```

### Explanation of Compiler Flags

| Flag | Purpose |
| :--- | :--- |
| `-O3` | Enables maximum compiler optimizations (loop unrolling, dead-code elimination, SIMD where available). |
| `-Iinclude` | Specifies the path to `include/` containing `ptdb.h`, `chunk.h`, `writer.h`, etc. |
| `-s WASM=1` | Outputs standard WebAssembly bytecode (`.wasm`) instead of legacy asm.js. |
| `-s FORCE_FILESYSTEM=1` | Emulates full POSIX filesystem semantics (`open`, `write`, `lseek`, `fsync`). |
| `-s ALLOW_MEMORY_GROWTH=1` | Allows the Wasm virtual heap to expand dynamically as more points are ingested. |
| `-s EXPORTED_FUNCTIONS` | Prevents the dead-code eliminator from stripping our public C API functions (must be prefixed with an underscore `_`). |
| `-s EXPORTED_RUNTIME_METHODS` | Exports JavaScript bridge helpers (`cwrap`, `ccall`, and the `FS` object). |
| `-o web/potdb.js` | Emits both `web/potdb.js` (glue code) and `web/potdb.wasm` (binary). |

---

## 4. Automating via Makefile Target

Add this target to your [Makefile](file:///home/jsndz/code/projects/dbaCe/Makefile):

```makefile
WASM_DIR = web
WASM_TARGET = $(WASM_DIR)/potdb.js

wasm: $(WASM_TARGET)

$(WASM_TARGET): $(SRC_DIR)/crc32.c $(SRC_DIR)/writer.c $(SRC_DIR)/reader.c
	mkdir -p $(WASM_DIR)
	emcc -O3 -I$(INC_DIR) $^ \
		-s WASM=1 \
		-s FORCE_FILESYSTEM=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s EXPORTED_FUNCTIONS=_ptdb_init,_ptdb_open,_ptdb_close,_ptdb_insert_2d,_ptdb_insert_3d,_ptdb_query_range_2d,_ptdb_reader_open,_ptdb_reader_close,_malloc,_free \
		-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,FS \
		-o $@
	@echo "WebAssembly build complete: $(WASM_TARGET) & $(WASM_DIR)/potdb.wasm"
```

Now you can compile with a single command:
```bash
make wasm
```

---

## 5. Integrating with JavaScript (`app.js`)

### Step 1: Include `potdb.js` in `index.html`
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>potDB In-Browser Tracker</title>
</head>
<body>
  <button id="btn-start">Start Tracking</button>
  <button id="btn-stop">Stop Tracking</button>
  <button id="btn-export">Download .db File</button>

  <script src="potdb.js"></script>
  <script src="app.js"></script>
</body>
</html>
```

### Step 2: Initialize and Wrap C Functions (`app.js`)
```javascript
// Wait for Emscripten runtime to load
Module.onRuntimeInitialized = async () => {
  console.log("potDB WebAssembly initialized successfully!");

  // Bind C functions using cwrap: cwrap(name, returnType, [argTypes])
  const ptdb_init = Module.cwrap('ptdb_init', 'number', ['string', 'number', 'number']);
  const ptdb_open = Module.cwrap('ptdb_open', 'number', ['string', 'number']);
  const ptdb_insert_2d = Module.cwrap('ptdb_insert_2d', 'number', ['number', 'number', 'number', 'number']);
  const ptdb_close = Module.cwrap('ptdb_close', 'number', ['number']);

  const DB_PATH = '/telemetry.db';
  let dbHandle = null;
  let watchId = null;

  // 1. Initialize Schema: (dim: 2D, time_unit: ms)
  ptdb_init(DB_PATH, 2, 1);

  // 2. Open Writer Handle (pass 0 for default config)
  dbHandle = ptdb_open(DB_PATH, 0);

  // 3. Hook GPS Geolocation Stream
  document.getElementById('btn-start').onclick = () => {
    watchId = navigator.geolocation.watchPosition(
      (pos) => {
        const ts = BigInt(pos.timestamp);
        const lat = pos.coords.latitude;
        const lng = pos.coords.longitude;

        // Ingest into embedded database
        ptdb_insert_2d(dbHandle, ts, lat, lng);
        console.log(`[GPS] Recorded: ${lat}, ${lng} at ${ts}`);
      },
      (err) => console.error("GPS error:", err),
      { enableHighAccuracy: true, timeout: 5000 }
    );
  };

  // 4. Stop Tracking and Flush Database
  document.getElementById('btn-stop').onclick = () => {
    if (watchId) {
      navigator.geolocation.clearWatch(watchId);
      watchId = null;
    }
    if (dbHandle) {
      ptdb_close(dbHandle);
      dbHandle = null;
      console.log("Database committed and closed.");
    }
  };

  // 5. Export the raw binary .db file to user's computer
  document.getElementById('btn-export').onclick = () => {
    // Read raw binary bytes directly from Emscripten Virtual Filesystem
    const data = Module.FS.readFile(DB_PATH);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.href = url;
    a.download = 'walk.db';
    a.click();
    URL.revokeObjectURL(url);
  };
};
```

---

## 6. Offline Persistence with IndexedDB (`IDBFS`)

By default, Emscripten's virtual filesystem operates in RAM (`MEMFS`). To make your database survive browser restarts and tab refreshes:

```javascript
// Mount IndexedDB to /data directory
Module.FS.mkdir('/data');
Module.FS.mount(Module.IDBFS, {}, '/data');

// Sync from IndexedDB into memory on app launch
Module.FS.syncfs(true, (err) => {
  if (err) console.error("Error reading from IndexedDB:", err);
  else console.log("Persistent filesystem mounted successfully.");
});

// Sync from memory back to IndexedDB after chunk flushes:
function saveDatabase() {
  Module.FS.syncfs(false, (err) => {
    if (err) console.error("Error writing to IndexedDB:", err);
  });
}
```

---

## 7. Verifying in the Terminal

When the user clicks **Download .db File**, the browser downloads `walk.db`. You can verify that it is 100% binary-compatible with the native C storage engine:

```bash
./potdb dump ~/Downloads/walk.db
```

Output:
```
==================================================
           PTDB FILE DUMP: walk.db
==================================================
Magic: 0x42445450 (PTDB) | Version: 1 | Dimension: 2D | TimeUnit: ms
--------------------------------------------------
Chunk #0:
  Count: 142 points
  Time: [1700000000000 .. 1700000142000]
  MBR: X:[37.774900 .. 37.778200] Y:[-122.419400 .. -122.415000]
  CRC-32: 0x4B3A821E (VERIFIED)
==================================================
```

---

## 8. Debugging Tips for WebAssembly

* **Enable Debugging Symbols**: Add `-g3 -s ASSERTIONS=1` during development to get detailed stack traces and bounds checks in the browser console.
* **Catch Memory Leaks**: Add `-s SAFE_HEAP=1` to detect unaligned memory accesses or invalid pointer arithmetic.
* **Inspect Wasm Binary**: Inspect the generated WebAssembly instructions using standard `wasm-objdump` or Chrome DevTools (Sources -> wasm panel).
