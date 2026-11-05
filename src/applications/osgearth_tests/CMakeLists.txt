set(TARGET_SRC
    main.cpp
    CacheTests.cpp
    DepthConventionTests.cpp
    CoverageTests.cpp
    EndianTests.cpp
    ExpressionTests.cpp
    GeoExtentTests.cpp
    TerrainSRGBImageryTests.cpp
    FeatureTests.cpp
    ImageUtilsTests.cpp
    TerrainPremultipliedAlphaTests.cpp
    PathTests.cpp
    PagedNodeTests.cpp
    RexShaderTokenTests.cpp
    ImageLayerTests.cpp
    SpatialReferenceTests.cpp
    ThreadingTests.cpp
    VirtualProgramMainTests.cpp)

add_osgearth_app(
    TARGET osgearth_tests
    SOURCES ${TARGET_SRC}
    FOLDER Tests)

# RexShaderTokenTests reads the rex engine's GLSL sources from the source tree.
target_compile_definitions(osgearth_tests PRIVATE
    OSGEARTH_REX_SHADER_DIR="${PROJECT_SOURCE_DIR}/src/osgEarthDrivers/engine_rex")
