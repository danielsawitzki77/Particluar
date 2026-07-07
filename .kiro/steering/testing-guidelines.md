# Testing Guidelines — Non-Interactive Asserts

## MSVC Debug Assertions

On Windows, MSVC's Debug runtime (`/MDd`, Debug configuration) shows **interactive assertion dialogs** when a checked iterator violation occurs (e.g., `vector subscript out of range`). These dialogs block execution and cannot be dismissed without human input, making them incompatible with headless CI/automated runs.

### Required Pattern

**All test executables MUST disable interactive assertion dialogs at startup.** Add this to the top of `main()`:

```cpp
#ifdef _WIN32
#include <crtdbg.h>
#include <stdlib.h>
#endif

int main()
{
#ifdef _WIN32
    // Redirect assertions to stderr instead of showing popup dialogs.
    // Required for headless/CI runs and automated pre-push hooks.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    // ... rest of main
}
```

### What This Does

- `_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE)` — routes assertion messages to a file handle instead of a dialog
- `_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR)` — that file handle is stderr
- `_set_abort_behavior(0, ...)` — prevents the "abort has been called" dialog on crash

### When Assertions Fire

With these settings, a failed assertion will:
1. Print the error to stderr (file, line, expression)
2. Call `abort()` (process exits with non-zero code)
3. The test runner reports FAIL without blocking

### Why This Matters

- The `pre-push` git hook runs `body_renderer_tests.exe` before every push
- Kiro's headless CLI automation runs tests non-interactively
- CI pipelines cannot dismiss modal dialogs
- Without this fix, a single bounds error blocks the entire push/workflow indefinitely

### Applies To

Every test executable in this project:
- `body_renderer_tests.exe`
- `tests.exe` (WFC/renderer tests, if applicable)
- Any future test binaries

### Do NOT Do

- Do NOT disable checked iterators entirely (`_ITERATOR_DEBUG_LEVEL=0`) — they catch real bugs
- Do NOT use Release builds for testing just to avoid this — Debug catches more issues
- Do NOT use `#pragma warning(disable:...)` to suppress CRT asserts
