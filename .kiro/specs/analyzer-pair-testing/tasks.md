# Tasks: Analyzer Pair Testing

## Implementation Tasks

- [ ] 1. Create `body_viewer_config.json` at project root with default values
- [ ] 2. Add config loading to Body_Viewer (parse JSON with picojson on startup, store in AnalyzerConfig struct)
- [ ] 3. Update `RunAnalyzeMode` to read config values instead of hardcoded constants
- [ ] 4. Add 2-body random generation (BodyGenerator with max_depth=1, forced 2 nodes, random shape types and connection faces)
- [ ] 5. Add offscreen screenshot capture (hidden SDL window + OpenGL context + glReadPixels + BMP write)
- [ ] 6. Add timestamped output folder creation (`test_output/run_YYYYMMDD_HHMMSS/` + `screenshots/` subdirectory)
- [ ] 7. Add JSON embedding in report (serialize each generated model's Body JSON into the markdown report)
- [ ] 8. Run first test (64 models × 8 subdivisions = 512 screenshots, verify report generation)
