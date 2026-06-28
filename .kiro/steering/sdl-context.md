# SDL & SDL_image Library Context

This workspace uses two companion libraries located as siblings on disk:

- `c:\Users\Daniel Sawitzki\Desktop\github\SDL`
- `c:\Users\Daniel Sawitzki\Desktop\github\SDL_image`

---

## SDL (Simple DirectMedia Layer) — v3.5.0

This is the **SDL3** branch (a major revision from SDL2). Cross-platform C library for multimedia/game development. Single include: `#include <SDL3/SDL.h>`.

### Subsystems

| Area | Key types / notes |
|---|---|
| Video / Window | `SDL_Window`, display management, OpenGL/Vulkan/Metal context |
| 2D Renderer | `SDL_Renderer`, `SDL_Texture` — hardware-accelerated 2D |
| GPU API | `SDL_GPUDevice`, `SDL_GPUTexture`, etc. — new low-level GPU API in SDL3, similar in spirit to WebGPU/Metal |
| Audio | `SDL_AudioDevice`, streaming, mixing |
| Events | Unified event loop — keyboard, mouse, gamepad, touch, pen |
| Input | Joystick, gamepad, haptic, touch, pen, mouse, keyboard |
| Threads | `SDL_Thread`, `SDL_Mutex`, atomics |
| I/O | `SDL_IOStream` — replaces SDL2's `SDL_RWops` |
| Filesystem / Storage / Async I/O | File access, app storage paths, async ops |
| Camera | Camera capture — new in SDL3 |
| Tray | System tray icons — new in SDL3 |
| Process | Subprocess spawning — new in SDL3 |
| Properties | Generic key-value bags attached to SDL objects |
| Pixels / Surface | `SDL_Surface` — CPU-side pixel buffers |

### Build

CMake-first. VisualC project files also present. Targets Windows, Linux, macOS, Android, iOS, and more.

### Source layout

```
SDL/
  include/SDL3/      ← all public headers
  src/
    audio/  camera/  events/  gpu/  render/  video/  ...
  examples/
    renderer/  audio/  camera/  demo/  ...
```

---

## SDL_image — v3.5.0

Companion library for image loading. Single public header: `#include <SDL3_image/SDL_image.h>`.

### Supported formats

BMP, GIF, JPEG, LBM, PCX, PNG, PNM (PPM/PGM/PBM), QOI, TGA, XCF, XPM, XV, SVG (nanosvg), ANI/CUR/ICO.
Optional (via external libs): AVIF, JPEG-XL, TIFF, WebP.

### Key API

```c
// Load to CPU surface
SDL_Surface *IMG_Load(const char *file);
SDL_Surface *IMG_Load_IO(SDL_IOStream *src, bool closeio);

// Load to 2D renderer texture
SDL_Texture *IMG_LoadTexture(SDL_Renderer *renderer, const char *file);
SDL_Texture *IMG_LoadTexture_IO(SDL_Renderer *renderer, SDL_IOStream *src, bool closeio);

// Load to SDL3 GPU texture (since 3.4.0)
SDL_GPUTexture *IMG_LoadGPUTexture(SDL_GPUDevice *device, SDL_GPUCopyPass *copy_pass,
                                   const char *file, int *width, int *height);

// Clipboard (since 3.4.0)
SDL_Surface *IMG_GetClipboardImage(void);

// Format detection
bool IMG_isPNG(SDL_IOStream *src);
bool IMG_isJPG(SDL_IOStream *src);
// ... IMG_isTYPE for every supported format
```

### Source layout

```
SDL_image/
  include/SDL3_image/SDL_image.h   ← single public header
  src/
    IMG.c              ← dispatch / main entry points
    IMG_png.c          ← per-format decoders
    IMG_jpg.c
    IMG_bmp.c
    IMG_gif.c  IMG_ani.c
    IMG_avif.c  IMG_jxl.c  IMG_webp.c  IMG_tif.c
    IMG_svg.c  IMG_qoi.c  IMG_stb.c
    IMG_gpu.c            ← GPU texture upload helpers
    IMG_WIC.c            ← Windows Imaging Component backend
    IMG_ImageIO.m        ← macOS ImageIO backend
    IMG_anim_decoder.c   ← animation decoding
    IMG_anim_encoder.c   ← animation encoding
    stb_image.h  nanosvg.h  qoi.h  tiny_jpeg.h   ← bundled single-header libs
```

---

## Important

Both repos contain `AGENTS.md` / `CLAUDE.md` explicitly **prohibiting AI-generated code contributions**. Kiro can help analyze, understand, and identify issues in this code, but any code changes intended as contributions to SDL or SDL_image must be authored by a human.
