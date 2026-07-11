# Requirements: Analyzer Pair Testing

## Overview
Configurable analyzer mode for Body_Viewer that generates random 2-body primitive pairs, renders them at multiple subdivision levels, and produces a timestamped report with embedded screenshots to isolate and verify connection quality.

## Requirements

### 1. Configuration File
- A JSON config file `body_viewer_config.json` exists at the project root
- Contains an `analyze` section with all analyzer parameters
- Parsed with picojson on startup

### 2. Configurable Properties
| Property | Default | Description |
|---|---|---|
| `num_random_models` | 64 | Number of random 2-body models to generate per run |
| `bodies_per_model` | 2 | Number of primitive bodies per generated model (always 2 for pair testing) |
| `subdivision_range` | [1,2,3,4,5,6,7,8] | List of subdivision levels to test each model at |
| `random_seed` | 0 | RNG seed for reproducibility (0 = use time-based seed) |

### 3. Config Loading
- Analyzer reads `body_viewer_config.json` on startup
- If file is missing or malformed, fall back to hardcoded defaults
- Config values are used exclusively in `--analyze` mode

### 4. Timestamped Output
- Each analyzer run produces a folder under `test_output/` named `run_YYYYMMDD_HHMMSS`
- Folder contains: `report.md` and a `screenshots/` subdirectory
- Screenshots are named `model_NN_subdiv_M.bmp`

### 5. Report Format
- Report is Markdown with embedded JSON blocks for each generated model
- JSON blocks contain the full body definition used for that model
- Image references link to the screenshots directory

### 6. Offscreen Screenshot Capture
- Screenshots are rendered offscreen — no visible window needed during analyze mode
- Uses SDL hidden window + OpenGL FBO or glReadPixels to capture framebuffer
- Output format: BMP (simple, no external dependencies)

### 7. 2-Body Pair Focus
- Each generated model consists of exactly 2 primitive bodies connected at a single joint
- This isolates connection quality without interference from complex hierarchies
- Primitives are selected randomly from the available shape types (cube, sphere, cylinder, etc.)

### 8. Quality Checks
- Analyzer verifies: no extra faces appear compared to baseline single-body render
- Analyzer verifies: the larger face at a connection is shrunk (not extruded) to match the smaller
