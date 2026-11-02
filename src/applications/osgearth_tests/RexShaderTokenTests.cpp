/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/catch.hpp>

#include <osgEarth/ShaderLoader>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace osgEarth;
using namespace osgEarth::Util;

// The rex sources are read from the source tree (OSGEARTH_REX_SHADER_DIR, set by
// CMakeLists.txt) into a package, then loaded through ShaderLoader the way the
// engine loads them, so "#pragma include" files are resolved and checked too.

namespace
{
    // Every GLSL file of the rex engine, GL3 and GL4 alike (RexEngine.NormalMap.vert.glsl
    // is not in the engine's package but is kept clean as well).
    const char* const s_rexFiles[] = {
        "RexEngine.vert.glsl",
        "RexEngine.elevation.glsl",
        "RexEngine.gs.glsl",
        "RexEngine.ImageLayer.glsl",
        "RexEngine.NormalMap.glsl",
        "RexEngine.NormalMap.vert.glsl",
        "RexEngine.Morphing.glsl",
        "RexEngine.Tessellation.glsl",
        "RexEngine.SDK.glsl",
        "RexEngine.vert.GL4.glsl",
        "RexEngine.ImageLayer.GL4.glsl",
        "RexEngine.NormalMap.GL4.glsl",
        "RexEngine.Tessellation.GL4.glsl",
        "RexEngine.SDK.GL4.glsl",
        "RexEngine.GL4.glsl"
    };

    struct Token
    {
        const char* text;
        bool prefix; // true: any continuation matches (gl_MultiTexCoord0..7)
    };

    // Compatibility-profile names that a core context does not provide.
    const Token s_compatibilityTokens[] = {
        { "gl_MultiTexCoord", true },
        { "gl_NormalMatrix", false },
        { "gl_ModelViewMatrix", false },
        { "gl_ProjectionMatrix", false },
        { "gl_ModelViewProjectionMatrix", false },
        { "gl_Vertex", false },
        { "gl_Normal", false },
        { "gl_Color", false },
        { "gl_FrontColor", false },
        { "gl_TexCoord", false },
        { "ftransform", false }
    };

    bool isIdentifierChar(char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    // Occurrences of the token as a whole identifier (or as a prefix where marked);
    // gl_VertexID and gl_NormalMatrix are not gl_Vertex and gl_Normal.
    std::vector<std::string> findToken(const std::string& source, const Token& token)
    {
        std::vector<std::string> hits;
        const std::string text(token.text);
        std::string::size_type pos = 0;
        while ((pos = source.find(text, pos)) != std::string::npos)
        {
            const bool startsIdentifier = pos == 0 || !isIdentifierChar(source[pos - 1]);
            const std::string::size_type end = pos + text.length();
            const bool endsIdentifier = token.prefix || end >= source.length() || !isIdentifierChar(source[end]);
            if (startsIdentifier && endsIdentifier)
            {
                const std::string::size_type lineStart = source.rfind('\n', pos);
                const std::string::size_type lineEnd = source.find('\n', pos);
                hits.push_back(source.substr(
                    lineStart == std::string::npos ? 0 : lineStart + 1,
                    (lineEnd == std::string::npos ? source.length() : lineEnd) - (lineStart == std::string::npos ? 0 : lineStart + 1)));
            }
            pos = end;
        }
        return hits;
    }

    std::string readFile(const std::string& path)
    {
        std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
        std::stringstream buf;
        buf << in.rdbuf();
        return buf.str();
    }

    ShaderPackage rexPackage()
    {
        ShaderPackage package;
        for (const char* file : s_rexFiles)
        {
            package.add(file, readFile(std::string(OSGEARTH_REX_SHADER_DIR) + "/" + file));
        }
        return package;
    }
}

TEST_CASE("RexShaderTokens.EveryRexFileIsListed")
{
    // A new variant added to the engine directory has to join the scan.
    std::set<std::string> listed(std::begin(s_rexFiles), std::end(s_rexFiles));
    osgDB::DirectoryContents contents = osgDB::getDirectoryContents(OSGEARTH_REX_SHADER_DIR);
    unsigned found = 0u;
    for (const std::string& name : contents)
    {
        if (osgDB::getLowerCaseFileExtension(name) == "glsl")
        {
            INFO(name);
            REQUIRE(listed.count(name) == 1u);
            ++found;
        }
    }
    REQUIRE(found == listed.size());
}

TEST_CASE("RexShaderTokens.NoCompatibilityTokens")
{
    ShaderPackage package = rexPackage();

    for (const char* file : s_rexFiles)
    {
        const std::string source = ShaderLoader::load(file, package);
        INFO(file);
        REQUIRE(!source.empty());

        for (const Token& token : s_compatibilityTokens)
        {
            const std::vector<std::string> hits = findToken(source, token);
            std::string lines;
            for (const std::string& line : hits)
            {
                lines += line + '\n';
            }
            INFO(token.text);
            INFO(lines);
            REQUIRE(hits.empty());
        }
    }
}

TEST_CASE("RexShaderTokens.TokenMatchIsWholeIdentifier")
{
    const Token vertex = { "gl_Vertex", false };
    const Token texCoord = { "gl_MultiTexCoord", true };
    REQUIRE(findToken("int i = gl_VertexID;", vertex).empty());
    REQUIRE(findToken("vec4 v = osg_Vertex;", vertex).empty());
    REQUIRE(findToken("vec4 v = gl_Vertex;", vertex).size() == 1u);
    REQUIRE(findToken("vec4 t = gl_MultiTexCoord3;", texCoord).size() == 1u);
    REQUIRE(findToken("vec4 t = osg_MultiTexCoord3;", texCoord).empty());
}
