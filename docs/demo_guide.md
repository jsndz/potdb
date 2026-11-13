# potDB Interactive Demo Website Guide

The `web/` directory contains a 100% client-side, zero-backend interactive showcase website for **potDB (Position Over Time Database)**. It compiles the C99 storage engine to WebAssembly (`potdb.wasm`) and runs entirely within the user's browser sandbox.

---

## Architecture & Features

The demo website includes:

1. **Top Hero & Value Proposition**:
   - Live WebAssembly engine status indicator (`potdb.wasm` ~24 KB).
   - Core hardware-independent architectural metrics (< 32 KB RAM, 16 bytes/point, CRC-32 integrity).
2. **Interactive Showcase Studio (4 Dedicated Modes)**:
   - **Mode 1: GPS & Route Simulator**:
     - Live device GPS streaming via `navigator.geolocation` (with automatic fallback from satellite GPS to Wi-Fi/network positioning).
     - Preset city simulations: San Francisco Embarcadero (Walk), Tokyo Shibuya Crossing (City Run), and 3D Aerial Drone Survey (Spiral flight).
     - Adjustable simulation speed slider (1x to 25x).
     - Live telemetry: Ingested points, distance traveled, elapsed time, virtual `.db` file size, and flushed chunk counts.
     - Database operations: Download `.db` file, or upload any `.db` file to visualize its path on the map.
   - **Mode 2: In-Browser Ingestion Benchmark**:
     - Real-time stress test running timed C99 insertion loops in `potdb.wasm`.
     - Supports 10,000 to 250,000 points and configurable chunk sizes.
     - Measures real-time points/second throughput, execution latency, chunks created, and file size.
   - **Mode 3: Spatiotemporal Range Query Visualizer**:
     - Visualizes how potDB achieves fast queries via **chunk-skipping**.
     - Bounding box presets (Entire Route, First Half, Second Half, Outside).
     - Shows how many chunks were skipped instantly via 44-byte MBR header checks vs. how many chunks were scanned.
   - **Mode 4: Binary File & Integrity Inspector**:
     - Direct byte-level visualization of the active `.db` file.
     - Inspects file header (magic `0x42445450`, dimensions, version) and chunk headers (magic `0x4348`, point count, MBR bounds).
     - Real-time IEEE 802.3 CRC-32 verification badge for each chunk.
     - **"Simulate Bit Rot"** button: Mutates a byte in the chunk payload to demonstrate how potDB's Triple Integrity Gate immediately catches corrupted chunks.
3. **Core Engine Architecture & Comparison Matrix**:
   - Visual breakdown of the Chunked Append-Only Log, MBR Pruning, Triple Integrity Gate, and deterministic memory ceiling.
   - Side-by-side comparison table against SQLite, Flat JSON, and InfluxDB.
4. **Developer Quickstart**:
   - Ready-to-use integration code snippets for C/C++ native, WebAssembly/JavaScript, and Python `ctypes`.

---

## Running Locally

Because the browser's Geolocation API and WebAssembly module require a Secure Context or HTTP server (they cannot be run over `file://`), launch a local web server:

```bash
# From project root:
python3 -m http.server 8000 -d web
```

Then open your browser to:
```
http://localhost:8000
```

---

## Deploying to Production (Zero Backend Required)

Since `web/` is completely static (HTML, CSS, JS, Wasm), it can be hosted on any static hosting provider at zero cost:

### 1. GitHub Pages
1. Push your repository to GitHub.
2. Go to **Settings** > **Pages**.
3. Under **Build and deployment**, select **Deploy from a branch**.
4. Select `main` branch and folder `/web`.
5. Click **Save**. Your demo site will be live at `https://<username>.github.io/<repo>/`.

### 2. Cloudflare Pages / Vercel / Netlify
- Point the project root to `web/` with no build command required.
