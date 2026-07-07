// body_renderer_tests - Property-based tests for OpenGL Body Renderer
// Entry point

#include <rapidcheck.h>
#include <cstdio>

#ifdef _WIN32
#include <crtdbg.h>
#include <stdlib.h>
#endif

// Forward declarations for test functions
void RunBodyLoaderTests();
void RunFaceGeneratorTests();
void RunTriangulatorTests();
void RunConnectionSolverTests();
void RunModelSwitcherTests();
void RunConnectionValidatorTests();
void RunConnectionFaceMatcherTests();

int main()
{
#ifdef _WIN32
    // Disable interactive assertion dialogs for headless/CI runs.
    // Assertions will be printed to stderr instead of showing a popup.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    printf("=== Body Renderer Property Tests ===\n\n");

    RunBodyLoaderTests();
    RunFaceGeneratorTests();
    RunTriangulatorTests();
    RunConnectionSolverTests();
    RunModelSwitcherTests();
    RunConnectionValidatorTests();
    RunConnectionFaceMatcherTests();

    printf("\n=== All tests complete ===\n");
    return 0;
}
