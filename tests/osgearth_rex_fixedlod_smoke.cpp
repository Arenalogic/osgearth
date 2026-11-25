/* osgearth_rex_fixedlod_smoke: standalone rex fixed-LOD smoke test on a real OpenGL 4.6 core context
 * (Arenalogic fork test, ctest; foundation F1).
 *
 * Builds an in-memory global-geodetic Map with a procedural DebugImageLayer (zero disk/network I/O),
 * configures rex for a fixed LOD (firstLOD = maxLOD = 1, synchronous tile creation, no morphing),
 * opens MapNode through the osgdb_osgearth_engine_rex plugin, and renders settled frames on a hidden
 * OpenGL 4.6 core forward-compatible context into an offscreen 64x64 RGBA8 FBO attachment.
 *
 * Checks:
 *  1. Rex engine loads and reports name "rex" with fixed firstLOD == 1, maxLOD == 1.
 *  2. Resident tile count becomes non-zero and stays bounded within LOD 1's 4x2 = 8 tiles.
 *  3. Rendered terrain image contains non-background pixels (terrain actually drew).
 *  4. Two consecutive settled frames are byte-identical on the same driver.
 *  5. Zero GL errors raised during render.
 *
 * Exit code 0 when every check holds, 1 otherwise, 77 when skipped (no hardware GL 4.6 core context).
 */

#include <osgEarth/Common>
#include <osgEarth/DebugImageLayer>
#include <osgEarth/GLUtils>
#include <osgEarth/Map>
#include <osgEarth/MapNode>
#include <osgEarth/Profile>
#include <osgEarth/Registry>
#include <osgEarth/TerrainEngineNode>
#include <osgEarth/TerrainOptions>

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/Image>
#include <osg/Matrixd>
#include <osg/Notify>
#include <osg/Texture2D>
#include <osgDB/Registry>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

USE_GRAPHICSWINDOW()

#ifndef GL_CONTEXT_PROFILE_MASK
    #define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
    #define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif

namespace
{
    const int kSkip = 77;
    // WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB == GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB == 0x0002
    const unsigned int kCreateForwardCompatibleBit = 0x00000002u;

    unsigned int s_checks = 0;
    unsigned int s_failures = 0;

    void check(bool ok, const std::string& what)
    {
        ++s_checks;
        if (!ok)
        {
            ++s_failures;
            std::cout << "FAILED: " << what << std::endl;
        }
    }

    struct ErrorCaptureCallback : public osg::Camera::DrawCallback
    {
        mutable std::vector<GLenum> errors;

        void operator()(osg::RenderInfo&) const override
        {
            for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
            {
                errors.push_back(e);
            }
        }
    };

    // Deterministic solid-colour in-memory ImageLayer (zero disk, network, or font dependency).
    class SolidColorImageLayer : public osgEarth::ImageLayer
    {
    public:
        META_Layer(osgEarth, SolidColorImageLayer, osgEarth::ImageLayer::Options, osgEarth::ImageLayer, solid_test_image);

        osgEarth::Status openImplementation() override
        {
            setProfile(osgEarth::Profile::create(osgEarth::Profile::GLOBAL_GEODETIC));
            return osgEarth::ImageLayer::openImplementation();
        }

        osgEarth::GeoImage createImageImplementation(const osgEarth::TileKey& key, osgEarth::ProgressCallback*) const override
        {
            osg::ref_ptr<osg::Image> img = new osg::Image();
            img->allocateImage(16, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            img->setInternalTextureFormat(GL_RGBA8);
            const unsigned char r = static_cast<unsigned char>(160 + (key.getTileX() & 3) * 20);
            const unsigned char g = static_cast<unsigned char>(100 + (key.getTileY() & 3) * 20);
            const unsigned char b = 60;
            unsigned char* p = img->data();
            for (int i = 0; i < 16 * 16; ++i)
            {
                p[i * 4 + 0] = r;
                p[i * 4 + 1] = g;
                p[i * 4 + 2] = b;
                p[i * 4 + 3] = 255;
            }
            return osgEarth::GeoImage(img.get(), key.getExtent());
        }
    };
}

int main(int, char**)
{
    osg::setNotifyLevel(osg::WARN);
    osgEarth::setNotifyLevel(osg::WARN);

#ifdef OSGEARTH_TEST_PLUGIN_DIR
    osgDB::Registry::instance()->getLibraryFilePathList().push_front(OSGEARTH_TEST_PLUGIN_DIR);
#endif

    osgEarth::initialize();

    // Create an offscreen OpenGL 4.6 core forward-compatible context (pbuffer first so disconnected-RDP /
    // service sessions still run on hardware GL; hidden window fallback). Exits 77 if unavailable.
    osg::ref_ptr<osg::GraphicsContext> gc;
    for (int attempt = 0; attempt < 2 && !gc.valid(); ++attempt)
    {
        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        traits->x = 0;
        traits->y = 0;
        traits->width = 64;
        traits->height = 64;
        traits->windowDecoration = false;
        traits->doubleBuffer = false;
        traits->pbuffer = (attempt == 0);
        traits->hidden = (attempt != 0);
        traits->windowName = "osgearth_rex_fixedlod_smoke";
        traits->glContextVersion = "4.6";
        traits->glContextProfileMask = GL_CONTEXT_CORE_PROFILE_BIT;
        traits->glContextFlags = kCreateForwardCompatibleBit;

        osg::ref_ptr<osg::GraphicsContext> candidate = osg::GraphicsContext::createGraphicsContext(traits.get());
        if (candidate.valid() && candidate->realize() && candidate->makeCurrent())
        {
            gc = candidate;
        }
    }
    if (!gc.valid())
    {
        std::cout << "osgearth_rex_fixedlod_smoke: skipped, no OpenGL 4.6 core context available" << std::endl;
        return kSkip;
    }

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "osgearth_rex_fixedlod_smoke: " << (renderer ? renderer : "?")
              << ", GL " << (version ? version : "?") << std::endl;
    // Initialize osgEarth's GL3/core state (MVP uniforms + vertex attribute aliasing).
    {
        osg::ref_ptr<osgEarth::GL3RealizeOperation> realizeOp = new osgEarth::GL3RealizeOperation();
        (*realizeOp)(gc.get());
    }
    gc->releaseContext();

    // Build an in-memory Map with deterministic procedural imagery and fixed-LOD rex options.
    osg::ref_ptr<osgEarth::Map> map = new osgEarth::Map();
    map->setProfile(osgEarth::Profile::create(osgEarth::Profile::GLOBAL_GEODETIC));

    osg::ref_ptr<SolidColorImageLayer> colorLayer = new SolidColorImageLayer();
    colorLayer->setName("solid_imagery");
    map->addLayer(colorLayer.get());

    osgEarth::MapNode::Options mapNodeOptions;
    mapNodeOptions.terrain().mutable_value().firstLOD() = 1u;
    mapNodeOptions.terrain().mutable_value().maxLOD() = 1u;
    mapNodeOptions.terrain().mutable_value().createTilesAsync() = false;
    mapNodeOptions.terrain().mutable_value().progressive() = false;
    mapNodeOptions.terrain().mutable_value().morphTerrain() = false;
    mapNodeOptions.terrain().mutable_value().morphImagery() = false;
    mapNodeOptions.terrain().mutable_value().minResidentTiles() = 0u;
    mapNodeOptions.terrain().mutable_value().enableLighting() = false;

    osg::ref_ptr<osgEarth::MapNode> mapNode = new osgEarth::MapNode(map.get(), mapNodeOptions);
    const bool opened = mapNode->open();
    check(opened, "MapNode::open() succeeds");

    osgEarth::TerrainEngineNode* engine = dynamic_cast<osgEarth::TerrainEngineNode*>(mapNode->getTerrainEngine());
    check(engine != nullptr, "MapNode created a TerrainEngineNode");
    if (engine != nullptr)
    {
        check(engine->getName() == "rex", "TerrainEngineNode is rex (got '" + engine->getName() + "')");
        check(engine->getOptions().getFirstLOD() == 1u, "rex firstLOD == 1");
        check(engine->getOptions().getMaxLOD() == 1u, "rex maxLOD == 1");
    }

    // Offscreen FBO camera attached to the hidden 4.6 core context.
    osg::ref_ptr<osg::Image> readback = new osg::Image();
    readback->allocateImage(64, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE);
    std::memset(readback->data(), 0, readback->getTotalSizeInBytes());

    osg::ref_ptr<ErrorCaptureCallback> errCb = new ErrorCaptureCallback();

    osg::ref_ptr<osg::Camera> rtt = new osg::Camera();
    rtt->setGraphicsContext(gc.get());
    rtt->setViewport(0, 0, 64, 64);
    rtt->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    rtt->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    rtt->setRenderOrder(osg::Camera::PRE_RENDER);
    rtt->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rtt->setClearColor(osg::Vec4(0.05f, 0.05f, 0.10f, 1.0f));
    rtt->attach(osg::Camera::COLOR_BUFFER0, readback.get());
    rtt->attach(osg::Camera::DEPTH_BUFFER, GL_DEPTH_COMPONENT24);
    rtt->setPostDrawCallback(errCb.get());
    rtt->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
    rtt->setProjectionMatrixAsPerspective(45.0, 1.0, 100000.0, 40000000.0);
    // Look at the equator prime meridian (ECEF +X) from ~14,000 km altitude.
    rtt->setViewMatrixAsLookAt(
        osg::Vec3d(14000000.0, 0.0, 0.0),
        osg::Vec3d(0.0, 0.0, 0.0),
        osg::Vec3d(0.0, 0.0, 1.0));
    rtt->addChild(mapNode.get());

    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::ViewerBase::SingleThreaded);
    viewer.setUseConfigureAffinity(false);
    viewer.setReleaseContextAtEndOfFrameHint(false);
    viewer.getCamera()->setGraphicsContext(gc.get());
    viewer.getCamera()->setViewport(0, 0, 64, 64);
    viewer.getCamera()->setClearMask(0);
    viewer.setSceneData(rtt.get());

    // Drive frames until rex reports resident tiles (bounded).
    unsigned int resident = 0u;
    for (int f = 0; f < 10; ++f)
    {
        viewer.frame(static_cast<double>(f) * 0.1);
        resident = engine != nullptr ? engine->getNumResidentTiles() : 0u;
        if (resident > 0u && f >= 3) break;
    }

    std::ostringstream resMsg;
    resMsg << "rex has resident tiles at fixed LOD 1 (resident=" << resident << ", expected 1..8)";
    check(resident >= 1u && resident <= 8u, resMsg.str());

    // Capture two consecutive settled frames and verify determinism and non-background coverage.
    errCb->errors.clear();
    viewer.frame(2.0);
    const unsigned int residentA = engine != nullptr ? engine->getNumResidentTiles() : 0u;
    std::vector<unsigned char> frameA(readback->data(), readback->data() + readback->getTotalSizeInBytes());

    viewer.frame(2.1);
    const unsigned int residentB = engine != nullptr ? engine->getNumResidentTiles() : 0u;
    std::vector<unsigned char> frameB(readback->data(), readback->data() + readback->getTotalSizeInBytes());

    check(residentA == residentB, "settled resident tile count is stable across repeated frames");
    check(frameA == frameB, "two consecutive settled fixed-LOD frames are byte-identical");

    unsigned int nonBgPixels = 0u;
    for (size_t i = 0; i + 3 < frameA.size(); i += 4)
    {
        // Clear colour is ~(13, 13, 26). Any pixel significantly differing is drawn terrain.
        if (std::abs(int(frameA[i]) - 13) > 5 ||
            std::abs(int(frameA[i + 1]) - 13) > 5 ||
            std::abs(int(frameA[i + 2]) - 26) > 5)
        {
            ++nonBgPixels;
        }
    }
    std::ostringstream covMsg;
    covMsg << "rendered terrain covers non-background pixels (nonBg=" << nonBgPixels << " of 4096)";
    check(nonBgPixels >= 100u, covMsg.str());

    std::ostringstream errMsg;
    errMsg << "zero GL errors during settled rex frames (count=" << errCb->errors.size() << ")";
    check(errCb->errors.empty(), errMsg.str());

    // Clean teardown.
    if (engine != nullptr)
    {
        engine->shutdown();
    }
    viewer.setSceneData(nullptr);
    gc->makeCurrent();
    mapNode->releaseGLObjects(gc->getState());
    gc->releaseContext();
    gc->close();

    std::cout << "osgearth_rex_fixedlod_smoke: " << s_checks << " checks, " << s_failures << " failed" << std::endl;
    return s_failures == 0 ? 0 : 1;
}
