/* osgEarth
* Copyright 2025 Pelican Mapping
* MIT License
*/

#include <osgEarth/catch.hpp>

#include <osgEarth/Registry>
#include <osgEarth/ShaderFactory>
#include <osgEarth/VirtualProgram>
#include <osgEarth/Version>
#include <osg/State>

#include <sstream>
#include <string>
#include <vector>

using namespace osgEarth;

namespace
{
    // A vertex function writing three varyings (a vector, an array, a flat int) and a
    // fragment function reading them.
    const char* vertexModel = R"(
out vec3 oe_vpmain_normal;
out vec4 oe_vpmain_coords[2];
flat out int oe_vpmain_id;
void oe_vpmain_vertex(inout vec4 vertex)
{
    oe_vpmain_normal = vec3(1.0);
    oe_vpmain_coords[0] = vertex;
    oe_vpmain_coords[1] = vertex;
    oe_vpmain_id = 3;
}
)";

    const char* fragmentColoring = R"(
in vec3 oe_vpmain_normal;
in vec4 oe_vpmain_coords[2];
flat in int oe_vpmain_id;
void oe_vpmain_fragment(inout vec4 color)
{
    color.rgb *= oe_vpmain_normal;
    color += oe_vpmain_coords[0] + oe_vpmain_coords[1];
    color.a += float(oe_vpmain_id);
}
)";

    // The generated main of the given stage, or empty.
    std::string generatedMain(osg::Shader::Type type)
    {
        osg::ref_ptr<VirtualProgram> vp = new VirtualProgram();
        vp->setFunction("oe_vpmain_vertex", vertexModel, VirtualProgram::LOCATION_VERTEX_MODEL);
        vp->setFunction("oe_vpmain_fragment", fragmentColoring, VirtualProgram::LOCATION_FRAGMENT_COLORING);

        VirtualProgram::FunctionLocationMap functions;
        vp->getFunctions(functions);
        VirtualProgram::ShaderMap shaders;
        vp->getShaderMap(shaders);
        VirtualProgram::ExtensionsSet extensions;

        // createMains only asserts these two (GL3RealizeOperation sets them); no GL call.
        osg::ref_ptr<osg::State> state = new osg::State();
        state->setUseVertexAttributeAliasing(true);
        state->setUseModelViewAndProjectionUniforms(true);

        std::vector<osg::ref_ptr<osg::Shader> > mains;
        Registry::instance()->getShaderFactory()->createMains(*state, functions, shaders, extensions, mains);
        for (auto& shader : mains)
        {
            if (shader.valid() && shader->getType() == type)
                return shader->getShaderSource();
        }
        return std::string();
    }

    // The lines between two markers of a generated source.
    std::vector<std::string> linesBetween(const std::string& source, const std::string& from, const std::string& to)
    {
        std::vector<std::string> lines;
        std::istringstream in(source);
        std::string line;
        bool inside = false;
        while (std::getline(in, line))
        {
            if (line.find(from) != std::string::npos)
            {
                inside = true;
                continue;
            }
            if (inside && line.find(to) != std::string::npos)
                break;
            if (inside && line.find_first_not_of(" \t\r") != std::string::npos)
                lines.push_back(line);
        }
        return lines;
    }
}

// GL core native D52 (ACT fork): the vertex main copies every stage global to vp_out,
// and only the VP functions' shader objects write them. Each global is declared with a
// zero initializer, so a compiler that checks each object alone (Mesa) reports no
// "used uninitialized"; a VP function's write still reaches vp_out.
TEST_CASE("VirtualProgramMains.TheVertexMainInitializesEveryStageGlobalItCopies")
{
    const std::string vertexMain = generatedMain(osg::Shader::VERTEX);
    INFO(vertexMain);
    REQUIRE(!vertexMain.empty());

    REQUIRE(vertexMain.find("vec3 oe_vpmain_normal = vec3(0);") != std::string::npos);
    REQUIRE(vertexMain.find("vec4 oe_vpmain_coords[2] = vec4[2](vec4(0), vec4(0));") != std::string::npos);
    REQUIRE(vertexMain.find("int oe_vpmain_id = int(0);") != std::string::npos);
    // osgEarth's built-ins too (vp_VertexView is read without a view stage).
    REQUIRE(vertexMain.find("vec3 vp_VertexView = vec3(0);") != std::string::npos);
    REQUIRE(vertexMain.find("vec4 vp_Color = vec4(0);") != std::string::npos);

    // Every declaration in the globals block has an initializer.
    const std::vector<std::string> globals = linesBetween(vertexMain, "// Vertex stage globals:", "// Vertex stage outputs:");
    REQUIRE(globals.size() >= 7u);
    for (const std::string& line : globals)
    {
        INFO(line);
        REQUIRE(line.find(" = ") != std::string::npos);
    }

    // The copy to the output block is still there, after the function call.
    const std::string::size_type call = vertexMain.find("oe_vpmain_vertex(vp_Vertex);");
    const std::string::size_type copy = vertexMain.find("= oe_vpmain_normal;");
    REQUIRE(call != std::string::npos);
    REQUIRE(copy != std::string::npos);
    REQUIRE(call < copy);
}

// The fragment main assigns its globals from vp_in before anything reads them: unchanged.
TEST_CASE("VirtualProgramMains.TheFragmentMainStillLoadsItsGlobalsFromTheInputBlock")
{
    const std::string fragmentMain = generatedMain(osg::Shader::FRAGMENT);
    INFO(fragmentMain);
    REQUIRE(!fragmentMain.empty());
    const std::vector<std::string> globals = linesBetween(fragmentMain, "// Fragment stage globals:", "void main");
    REQUIRE(!globals.empty());
    bool declaresNormal = false;
    for (const std::string& line : globals)
    {
        if (line.find("vec3 oe_vpmain_normal;") != std::string::npos)
            declaresNormal = true;
    }
    REQUIRE(declaresNormal);
}

// The feature define ACT's test support checks before it drops its Mesa tolerance.
TEST_CASE("VirtualProgramMains.TheForkAnnouncesInitializedStageGlobals")
{
#ifdef OSGEARTH_ACT_VP_STAGE_GLOBALS_INITIALIZED
    REQUIRE(OSGEARTH_ACT_VP_STAGE_GLOBALS_INITIALIZED == 1);
#else
    FAIL("osgEarth/Version does not define OSGEARTH_ACT_VP_STAGE_GLOBALS_INITIALIZED");
#endif
}
