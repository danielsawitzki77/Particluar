// body_renderer_tests - Property-based tests for OpenGL Body Renderer
// Entry point

#include <rapidcheck.h>
#include <cstdio>

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
