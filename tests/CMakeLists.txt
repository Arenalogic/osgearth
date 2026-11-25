# Arenalogic fork test harness (OSGEARTH_BUILD_TESTS, off by default):
# Registers osgearth_tests (Catch2 unit suite) and standalone rex smoke tests in CTest.

if(TARGET osgearth_tests)
    # Run the Arenalogic fork-owned Catch2 suites cleanly (25 cases: DepthConvention,
    # PagedNode, RexShaderTokens, TerrainSRGBImagery, TerrainPremultipliedAlpha).
    # Note: two upstream 3.8.1 SpatialReferenceTests cases fail against GDAL 3.12/PROJ 9
    # and are left untouched until the next upstream base replay.
    add_test(NAME osgearth_fork_tests
        COMMAND $<TARGET_FILE:osgearth_tests>
            "DepthConvention.*,PagedNode*,RexShaderTokens.*,TerrainSRGBImagery.*,TerrainPremultipliedAlpha.*"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/tests")
endif()

add_executable(osgearth_rex_fixedlod_smoke osgearth_rex_fixedlod_smoke.cpp)
target_link_libraries(osgearth_rex_fixedlod_smoke
    PRIVATE
        osgEarth
        ${OPENSCENEGRAPH_LIBRARIES}
        OpenGL::GL
)
set_target_properties(osgearth_rex_fixedlod_smoke PROPERTIES FOLDER "Tests")
target_compile_definitions(osgearth_rex_fixedlod_smoke PRIVATE
    OSGEARTH_TEST_PLUGIN_DIR="$<TARGET_FILE_DIR:osgdb_osgearth_engine_rex>"
)
if(TARGET osgdb_osgearth_engine_rex)
    add_dependencies(osgearth_rex_fixedlod_smoke osgdb_osgearth_engine_rex)
endif()

# Fixed-LOD rex terrain smoke test on a hidden OpenGL 4.6 core context; exits 77 (skipped) without hardware GL.
add_test(NAME osgearth_rex_fixedlod_smoke COMMAND $<TARGET_FILE:osgearth_rex_fixedlod_smoke>)
set_tests_properties(osgearth_rex_fixedlod_smoke PROPERTIES SKIP_RETURN_CODE 77)
