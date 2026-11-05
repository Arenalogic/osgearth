/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/catch.hpp>

#include <osgEarth/DebugImageLayer>
#include <osgEarth/TerrainEngineNode>
#include <osgEarth/TerrainOptions>
#include <osg/BlendFunc>
#include <osg/StateSet>

using namespace osgEarth;

// ACT fork, GL core native C15 T9: the terrain option premultiplied_alpha (default off)
// switches the blend of the layers the terrain draws into its surface to
// ONE, ONE_MINUS_SRC_ALPHA; off, every blend is today's.

namespace
{
    // A terrain engine that only answers getOptions(), the one call
    // VisibleLayer::prepareForRendering makes on it.
    class OptionsOnlyEngine : public TerrainEngine
    {
    public:
        TerrainOptions concrete;

        UID getUID() const override { return 0; }
        TerrainTileModel* createTileModel(const Map*, const TileKey&, const CreateTileManifest&, ProgressCallback*) override { return nullptr; }
        TerrainResources* getResources() const override { return nullptr; }
        const Map* getMap() const override { return nullptr; }
        Terrain* getTerrain() override { return nullptr; }
        const Terrain* getTerrain() const override { return nullptr; }
        void addEffect(TerrainEffect*) override { }
        void removeEffect(TerrainEffect*) override { }
        void invalidateRegion(const GeoExtent&) override { }
        void invalidateRegion(const GeoExtent&, unsigned) override { }
        void invalidateRegion(const GeoExtent&, unsigned, unsigned) override { }
        void invalidateRegion(const std::vector<const Layer*>, const GeoExtent&, unsigned, unsigned) override { }
        void invalidateRegion(const std::vector<const Layer*>, const GeoExtent&) override { }
        osg::StateSet* getTerrainStateSet() override { return nullptr; }
        osg::StateSet* getSurfaceStateSet() override { return nullptr; }
        osg::Node* getNode() override { return nullptr; }
        void addCreateTileModelCallback(CreateTileModelCallback*) override { }
        void removeCreateTileModelCallback(CreateTileModelCallback*) override { }
        osg::Node* createStandaloneTile(const TerrainTileModel*, int, unsigned, const TileKey&) override { return nullptr; }
        TerrainOptionsAPI getOptions() override { return TerrainOptionsAPI(&concrete); }
        unsigned getNumResidentTiles() const override { return 0u; }
        void dirtyTerrainOptions() override { }
        void setComputeTilePixelSizeCallback(const ComputeTilePixelSizeCallback&) override { }
        void setComputeTileRangeCallback(const ComputeTileRangeCallback&) override { }
    };

    // Exposes the layer's prepare step.
    class ProbeLayer : public DebugImageLayer
    {
    public:
        using DebugImageLayer::prepareForRendering;
    };

    const osg::BlendFunc* blendOf(ProbeLayer* layer)
    {
        const osg::StateSet* ss = layer->getStateSet();
        return ss ? dynamic_cast<const osg::BlendFunc*>(ss->getAttribute(osg::StateAttribute::BLENDFUNC)) : nullptr;
    }

    const osg::BlendFunc* preparedBlend(bool premultiplied, ColorBlending blending)
    {
        static OptionsOnlyEngine engine;
        engine.concrete.premultipliedAlpha() = premultiplied;
        static std::vector<osg::ref_ptr<ProbeLayer> > keep;
        osg::ref_ptr<ProbeLayer> layer = new ProbeLayer();
        layer->options().blend() = blending;
        layer->prepareForRendering(&engine);
        keep.push_back(layer);
        return blendOf(layer.get());
    }
}

TEST_CASE("TerrainPremultipliedAlpha.DefaultIsOff")
{
    TerrainOptions options;
    REQUIRE(options.premultipliedAlpha().get() == false);

    Config conf("terrain");
    conf.set("premultiplied_alpha", true);
    TerrainOptions fromConf(conf);
    REQUIRE(fromConf.premultipliedAlpha().get() == true);
    REQUIRE(fromConf.getConfig().value("premultiplied_alpha") == "true");
}

TEST_CASE("TerrainPremultipliedAlpha.OffASurfaceLayerKeepsTodaysStraightAlphaBlend")
{
    const osg::BlendFunc* blend = preparedBlend(false, BLEND_INTERPOLATE);
    REQUIRE(blend != nullptr);
    REQUIRE(blend->getSource() == GL_SRC_ALPHA);
    REQUIRE(blend->getDestination() == GL_ONE_MINUS_SRC_ALPHA);
}

TEST_CASE("TerrainPremultipliedAlpha.OnASurfaceLayerBlendsPremultiplied")
{
    const osg::BlendFunc* blend = preparedBlend(true, BLEND_INTERPOLATE);
    REQUIRE(blend != nullptr);
    REQUIRE(blend->getSource() == GL_ONE);
    REQUIRE(blend->getDestination() == GL_ONE_MINUS_SRC_ALPHA);
}

TEST_CASE("TerrainPremultipliedAlpha.OnAModulateLayerKeepsItsModulateBlend")
{
    const osg::BlendFunc* blend = preparedBlend(true, BLEND_MODULATE);
    REQUIRE(blend != nullptr);
    REQUIRE(blend->getSource() == GL_DST_COLOR);
    REQUIRE(blend->getDestination() == GL_ZERO);
}
