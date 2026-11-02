/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/catch.hpp>

#include <osgEarth/GLUtils>
#include <osg/Depth>
#include <osg/ref_ptr>

using namespace osgEarth;

namespace
{
    // The world encoding is process-wide: every case restores the default.
    struct ScopedWorldEncoding
    {
        explicit ScopedWorldEncoding(DepthConvention::WorldEncoding value)
        {
            DepthConvention::setWorldEncoding(value);
        }
        ~ScopedWorldEncoding()
        {
            DepthConvention::setWorldEncoding(DepthConvention::WorldEncoding::Legacy);
        }
    };

    void requireDepth(const osg::Depth* depth, osg::Depth::Function function, bool writeMask)
    {
        REQUIRE(depth != nullptr);
        REQUIRE(depth->getFunction() == function);
        REQUIRE(depth->getZNear() == 0.0);
        REQUIRE(depth->getZFar() == 1.0);
        REQUIRE(depth->getWriteMask() == writeMask);
    }
}

TEST_CASE("DepthConvention.DefaultIsLegacy")
{
    REQUIRE(DepthConvention::getWorldEncoding() == DepthConvention::WorldEncoding::Legacy);
}

TEST_CASE("DepthConvention.WorldLegacy")
{
    ScopedWorldEncoding encoding(DepthConvention::WorldEncoding::Legacy);

    // Exactly the state the world sites built before the entry points existed.
    osg::ref_ptr<osg::Depth> written = DepthConvention::world(true);
    osg::ref_ptr<osg::Depth> readOnly = DepthConvention::world(false);
    requireDepth(written.get(), osg::Depth::LEQUAL, true);
    requireDepth(readOnly.get(), osg::Depth::LEQUAL, false);
    osg::ref_ptr<osg::Depth> todayWritten = new osg::Depth(osg::Depth::LEQUAL, 0, 1, true);
    osg::ref_ptr<osg::Depth> todayReadOnly = new osg::Depth(osg::Depth::LEQUAL, 0, 1, false);
    REQUIRE(written->compare(*todayWritten) == 0);
    REQUIRE(readOnly->compare(*todayReadOnly) == 0);
    REQUIRE(DepthConvention::worldClearDepth() == 1.0);
}

TEST_CASE("DepthConvention.WorldReversedZ")
{
    ScopedWorldEncoding encoding(DepthConvention::WorldEncoding::ReversedZ);
    REQUIRE(DepthConvention::getWorldEncoding() == DepthConvention::WorldEncoding::ReversedZ);

    osg::ref_ptr<osg::Depth> written = DepthConvention::world(true);
    osg::ref_ptr<osg::Depth> readOnly = DepthConvention::world(false);
    requireDepth(written.get(), osg::Depth::GEQUAL, true);
    requireDepth(readOnly.get(), osg::Depth::GEQUAL, false);
    REQUIRE(DepthConvention::worldClearDepth() == 0.0);
}

TEST_CASE("DepthConvention.LocalNeverChanges")
{
    const DepthConvention::WorldEncoding encodings[2] = {
        DepthConvention::WorldEncoding::Legacy,
        DepthConvention::WorldEncoding::ReversedZ
    };

    for (auto value : encodings)
    {
        ScopedWorldEncoding encoding(value);

        osg::ref_ptr<osg::Depth> written = DepthConvention::local(true);
        osg::ref_ptr<osg::Depth> readOnly = DepthConvention::local(false);
        requireDepth(written.get(), osg::Depth::LEQUAL, true);
        requireDepth(readOnly.get(), osg::Depth::LEQUAL, false);
        REQUIRE(DepthConvention::localClearDepth() == 1.0);
    }
}

TEST_CASE("DepthConvention.EachCallIsANewObject")
{
    // Sites own their state attribute; nothing is shared between them.
    osg::ref_ptr<osg::Depth> a = DepthConvention::world(true);
    osg::ref_ptr<osg::Depth> b = DepthConvention::world(true);
    REQUIRE(a.get() != b.get());
    REQUIRE(a->compare(*b) == 0);
}
