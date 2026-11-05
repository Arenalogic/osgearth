/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/catch.hpp>

#include <osgEarth/DebugImageLayer>
#include <osgEarth/ImageLayer>
#include <osgEarth/TerrainOptions>
#include <osgEarth/TerrainTileModelFactory>
#include <osg/Image>
#include <osg/Texture>

#ifndef GL_SRGB8
#define GL_SRGB8 0x8C41
#endif
#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_R16
#define GL_R16 0x822A
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif

using namespace osgEarth;

// ACT fork, GL core native C15 T9: sRGB-tagged colour layers sample through sRGB internal
// formats when the terrain option srgb_imagery is set; everything else keeps its format.

namespace
{
    // Exposes the factory's image texture path.
    struct ProbeFactory : public TerrainTileModelFactory
    {
        explicit ProbeFactory(const TerrainOptions& options) : TerrainTileModelFactory(options) { }
        using TerrainTileModelFactory::createImageTexture;
        using TerrainTileModelFactory::createCoverageTexture;
    };

    osg::ref_ptr<osg::Image> makeImage(GLenum pixelFormat, GLenum internalFormat)
    {
        osg::ref_ptr<osg::Image> image = new osg::Image();
        image->allocateImage(16, 16, 1, pixelFormat, GL_UNSIGNED_BYTE);
        image->setInternalTextureFormat(internalFormat);
        return image;
    }

    GLint textureFormatOf(bool srgbImagery, ImageLayer::ColorSpace space, GLenum pixelFormat, GLenum internalFormat)
    {
        TerrainOptions options;
        options.srgbImagery() = srgbImagery;
        ProbeFactory factory(options);

        osg::ref_ptr<DebugImageLayer> layer = new DebugImageLayer();
        layer->options().colorSpace() = space;

        osg::ref_ptr<osg::Image> image = makeImage(pixelFormat, internalFormat);
        Texture::Ptr tex = factory.createImageTexture(image.get(), layer.get());
        REQUIRE(tex != nullptr);
        REQUIRE(tex->osgTexture().valid());
        return tex->osgTexture()->getInternalFormat();
    }
}

TEST_CASE("TerrainSRGBImagery.TheFormatTableMapsColourFormatsOnly")
{
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGB8, GL_UNSIGNED_BYTE) == GL_SRGB8);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGBA8, GL_UNSIGNED_BYTE) == GL_SRGB8_ALPHA8);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGB, GL_UNSIGNED_BYTE) == GL_SRGB8);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGBA, GL_UNSIGNED_BYTE) == GL_SRGB8_ALPHA8);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGBA, GL_UNSIGNED_SHORT) == (GLenum)GL_RGBA);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_UNSIGNED_BYTE) ==
            GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_UNSIGNED_BYTE) ==
            GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_UNSIGNED_BYTE) ==
            GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_UNSIGNED_BYTE) ==
            GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);

    // Elevation (R16 / R32F, Elevation.cpp), normal maps (RG8), 16-bit colour and sRGB: unchanged.
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_R16, GL_UNSIGNED_SHORT) == (GLenum)GL_R16);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_R32F, GL_FLOAT) == (GLenum)GL_R32F);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RG8, GL_UNSIGNED_BYTE) == (GLenum)GL_RG8);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_RGBA16, GL_UNSIGNED_SHORT) == (GLenum)GL_RGBA16);
    REQUIRE(TerrainTileModelFactory::getSRGBInternalFormat(GL_SRGB8_ALPHA8, GL_UNSIGNED_BYTE) == GL_SRGB8_ALPHA8);
}

TEST_CASE("TerrainSRGBImagery.DefaultsAreOff")
{
    TerrainOptions options;
    REQUIRE(options.srgbImagery().get() == false);
    DebugImageLayer::Options layerOptions;
    REQUIRE((layerOptions.colorSpace().get() == ImageLayer::ColorSpace::SRGB));
}

TEST_CASE("TerrainSRGBImagery.ColourLayersGetSRGBFormatsOnlyWithTheOption")
{
    // Option off (today): the image's format, sRGB tag or not.
    REQUIRE(textureFormatOf(false, ImageLayer::ColorSpace::SRGB, GL_RGBA, GL_RGBA8) == GL_RGBA8);
    REQUIRE(textureFormatOf(false, ImageLayer::ColorSpace::SRGB, GL_RGB, GL_RGB8) == GL_RGB8);

    // Option on: sRGB-tagged colour layers decode in the texture unit.
    REQUIRE(textureFormatOf(true, ImageLayer::ColorSpace::SRGB, GL_RGBA, GL_RGBA8) == GL_SRGB8_ALPHA8);
    REQUIRE(textureFormatOf(true, ImageLayer::ColorSpace::SRGB, GL_RGB, GL_RGB8) == GL_SRGB8);
    // The factory sizes an unsized 8-bit format first (RGBA -> RGBA8), then maps it.
    REQUIRE(textureFormatOf(true, ImageLayer::ColorSpace::SRGB, GL_RGBA, GL_RGBA) == GL_SRGB8_ALPHA8);

    // Option on, a linear (data) layer: unchanged.
    REQUIRE(textureFormatOf(true, ImageLayer::ColorSpace::LINEAR, GL_RGBA, GL_RGBA8) == GL_RGBA8);
}

TEST_CASE("TerrainSRGBImagery.CoverageTexturesStayLinear")
{
    TerrainOptions options;
    options.srgbImagery() = true;
    ProbeFactory factory(options);
    osg::ref_ptr<osg::Image> image = new osg::Image();
    image->allocateImage(4, 4, 1, GL_RED, GL_FLOAT);
    image->setInternalTextureFormat(GL_R32F);
    Texture::Ptr tex = factory.createCoverageTexture(image.get());
    REQUIRE(tex != nullptr);
    const GLint format = tex->osgTexture()->getInternalFormat();
    REQUIRE(format != GL_SRGB8);
    REQUIRE(format != GL_SRGB8_ALPHA8);
}

TEST_CASE("TerrainSRGBImagery.TheOptionsRoundTripThroughConfig")
{
    Config terrainConf("terrain");
    terrainConf.set("srgb_imagery", true);
    TerrainOptions terrain(terrainConf);
    REQUIRE(terrain.srgbImagery().get() == true);
    REQUIRE(terrain.getConfig().value("srgb_imagery") == "true");

    Config linearConf("image");
    linearConf.set("color_space", "linear");
    DebugImageLayer::Options linear(linearConf);
    REQUIRE((linear.colorSpace().get() == ImageLayer::ColorSpace::LINEAR));
    REQUIRE(linear.getConfig().value("color_space") == "linear");

    Config dataConf("image");
    dataConf.set("color_space", "data");
    DebugImageLayer::Options data(dataConf);
    REQUIRE((data.colorSpace().get() == ImageLayer::ColorSpace::LINEAR));

    Config srgbConf("image");
    srgbConf.set("color_space", "srgb");
    DebugImageLayer::Options srgb(srgbConf);
    REQUIRE((srgb.colorSpace().get() == ImageLayer::ColorSpace::SRGB));
    REQUIRE(srgb.getConfig().value("color_space") == "srgb");
}
