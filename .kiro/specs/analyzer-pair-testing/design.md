# Design: Analyzer Pair Testing

## Config File Format

```json
{
  "analyze": {
    "num_random_models": 64,
    "bodies_per_model": 2,
    "subdivision_range": [1, 2, 3, 4, 5, 6, 7, 8],
    "random_seed": 0
  }
}
```

- Parsed with picojson (`#include "../../picojson/picojson.h"`)
- Read via `std::ifstream` from project root on Body_Viewer startup
- If missing/invalid, log a warning and use defaults

## Random 2-Body Model Generation

- Use `BodyGenerator` with `max_depth = 1` to force exactly 2 nodes (root + one child)
- Seed the RNG with `random_seed` from config (or `time(nullptr)` if seed == 0)
- For each model:
  1. Pick random parent shape type (cube, sphere, cylinder, cone, torus)
  2. Pick random child shape type
  3. Pick random connection face on parent, random attachment face on child
  4. Generate random scale parameters within reasonable bounds (0.5–2.0)
- Produces N models where N = `num_random_models`

## Screenshot Capture

1. Create a hidden SDL window with OpenGL context (`SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL`)
2. Set up an FBO (or just use the default framebuffer with glReadPixels)
3. For each model × each subdivision level:
   - Clear framebuffer
   - Set up camera (fixed orthographic or perspective at a standard distance)
   - Render the 2-body model at the given subdivision level
   - Call `glReadPixels` to read the framebuffer into a pixel buffer
   - Write the buffer as a BMP file to `screenshots/model_NN_subdiv_M.bmp`
4. Destroy SDL window after all captures

## Report Format

Output: `test_output/run_YYYYMMDD_HHMMSS/report.md`

```markdown
# Analyzer Pair Test Report
Generated: YYYY-MM-DD HH:MM:SS
Seed: <seed_value>
Models: <num_random_models>
Subdivision range: [1, 2, 3, 4, 5, 6, 7, 8]

## Model 00

```json
{ ... full body JSON definition ... }
```

### Subdivision 1
![model_00_subdiv_1](screenshots/model_00_subdiv_1.bmp)
- Face count: ...
- Quality grade: ...

### Subdivision 2
...
```

## File Structure

```
test_output/
  run_YYYYMMDD_HHMMSS/
    report.md
    screenshots/
      model_00_subdiv_1.bmp
      model_00_subdiv_2.bmp
      ...
      model_63_subdiv_8.bmp
```

## Quality Checks

For each model at each subdivision level:
1. **No extra faces**: Render parent alone, count faces. Render child alone, count faces. Render pair, count faces. Pair faces should equal sum minus connection faces (2 removed).
2. **Shrink not extrude**: At the connection joint, measure the larger face's area. It should be ≤ the smaller face's area (shrunk to match), never larger than baseline.
