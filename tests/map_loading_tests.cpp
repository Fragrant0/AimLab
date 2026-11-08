#include "game_manager.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Require(bool condition, const std::string& message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    void RequireNoGLErrors(const std::string& stage)
    {
        GLenum firstError = glGetError();
        if (firstError != GL_NO_ERROR)
        {
            while (glGetError() != GL_NO_ERROR) {}
            throw std::runtime_error(stage + ": OpenGL error " + std::to_string(firstError));
        }
    }

    struct Context
    {
        Context()
        {
            Require(glfwInit() == GLFW_TRUE, "GLFW initialization failed");
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            Window = glfwCreateWindow(320, 180, "Map loading tests", nullptr, nullptr);
            if (!Window)
            {
                glfwTerminate();
                throw std::runtime_error("Hidden OpenGL 3.3 window creation failed");
            }
            glfwMakeContextCurrent(Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(Window);
                glfwTerminate();
                throw std::runtime_error("GLAD initialization failed");
            }
        }

        ~Context()
        {
            glfwDestroyWindow(Window);
            glfwTerminate();
        }

        GLFWwindow* Window = nullptr;
    };

    struct GeneratedObjects;
    GeneratedObjects* g_Recorder = nullptr;
    PFNGLGENTEXTURESPROC g_GenTextures = nullptr;
    PFNGLGENBUFFERSPROC g_GenBuffers = nullptr;
    PFNGLGENVERTEXARRAYSPROC g_GenVertexArrays = nullptr;
    void APIENTRY RecordTextures(GLsizei count, GLuint* objects);
    void APIENTRY RecordBuffers(GLsizei count, GLuint* objects);
    void APIENTRY RecordVertexArrays(GLsizei count, GLuint* objects);

    // Test-only GLAD hooks observe real object creation without production flags.
    struct GeneratedObjects
    {
        GeneratedObjects()
        {
            Require(g_Recorder == nullptr, "Nested GL recorders are unsupported");
            g_GenTextures = glad_glGenTextures;
            g_GenBuffers = glad_glGenBuffers;
            g_GenVertexArrays = glad_glGenVertexArrays;
            g_Recorder = this;
            glad_glGenTextures = RecordTextures;
            glad_glGenBuffers = RecordBuffers;
            glad_glGenVertexArrays = RecordVertexArrays;
        }

        ~GeneratedObjects()
        {
            glad_glGenTextures = g_GenTextures;
            glad_glGenBuffers = g_GenBuffers;
            glad_glGenVertexArrays = g_GenVertexArrays;
            g_Recorder = nullptr;
        }

        GeneratedObjects(const GeneratedObjects&) = delete;
        GeneratedObjects& operator=(const GeneratedObjects&) = delete;

        void RequireReleased(const std::string& name) const
        {
            for (GLuint object : Textures)
                Require(glIsTexture(object) == GL_FALSE, name + ": candidate texture leaked");
            for (GLuint object : Buffers)
                Require(glIsBuffer(object) == GL_FALSE, name + ": candidate buffer leaked");
            for (GLuint object : VertexArrays)
                Require(glIsVertexArray(object) == GL_FALSE, name + ": candidate vertex array leaked");
        }

        std::vector<GLuint> Textures;
        std::vector<GLuint> Buffers;
        std::vector<GLuint> VertexArrays;
    };

    void APIENTRY RecordTextures(GLsizei count, GLuint* objects)
    {
        g_GenTextures(count, objects);
        g_Recorder->Textures.insert(g_Recorder->Textures.end(), objects, objects + count);
    }

    void APIENTRY RecordBuffers(GLsizei count, GLuint* objects)
    {
        g_GenBuffers(count, objects);
        g_Recorder->Buffers.insert(g_Recorder->Buffers.end(), objects, objects + count);
    }

    void APIENTRY RecordVertexArrays(GLsizei count, GLuint* objects)
    {
        g_GenVertexArrays(count, objects);
        g_Recorder->VertexArrays.insert(g_Recorder->VertexArrays.end(), objects, objects + count);
    }

    struct Snapshot
    {
        Camera View;
        std::array<GLuint, 6> Textures;
        int MapIndex;
    };

    Snapshot Capture(GameManager& game)
    {
        ResourceManager& resources = ResourceManager::GetInstance();
        return {*game.GetCamera(), {
            resources.GetTexture("fixture_ground"),
            resources.GetHDRTexture("skybox_hdr"),
            resources.GetHDRSkybox("skybox"),
            resources.GetIrradianceMap("skybox"),
            resources.GetPrefilterMap("skybox"),
            resources.GetBRDFLUT()
        }, game.GetCurrentMapIndex()};
    }

    bool Equal(const glm::vec3& left, const glm::vec3& right)
    {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }

    void RequirePreserved(GameManager& game, const Snapshot& previous, const std::string& name)
    {
        const Snapshot current = Capture(game);
        Require(current.MapIndex == previous.MapIndex, name + ": active map index changed");
        Require(current.Textures == previous.Textures, name + ": previous resource cache changed");
        for (GLuint texture : previous.Textures)
            Require(texture != 0 && glIsTexture(texture) == GL_TRUE, name + ": previous texture was deleted");
        const Camera& a = current.View;
        const Camera& b = previous.View;
        Require(Equal(a.Position, b.Position) && Equal(a.Front, b.Front) && Equal(a.Up, b.Up) &&
                Equal(a.Right, b.Right) && Equal(a.WorldUp, b.WorldUp) && a.Yaw == b.Yaw &&
                a.Pitch == b.Pitch && a.Zoom == b.Zoom && a.MovementSpeed == b.MovementSpeed &&
                a.MouseSensitivity == b.MouseSensitivity, name + ": camera changed after failed switch");
    }

    void CheckFailedSwitch(GameManager& game, int index, const char* name, bool expectMesh = false)
    {
        const Snapshot previous = Capture(game);
        {
            GeneratedObjects generated;
            Require(!game.SwitchMap(index), std::string(name) + ": invalid map switch succeeded");
            RequirePreserved(game, previous, name);
            generated.RequireReleased(name);
            if (expectMesh)
                Require(!generated.Buffers.empty() && !generated.VertexArrays.empty(),
                        std::string(name) + ": fixture did not reach temporary model allocation");
        }
        RequireNoGLErrors(std::string(name) + " rollback");
        game.Render();
        glFinish();
        RequireNoGLErrors(std::string(name) + " render after rollback");
        std::cout << "[map-loading] PASS " << name << std::endl;
    }

    void Run(GameManager& game)
    {
        const Snapshot baseline = Capture(game);
        Require(baseline.MapIndex == 1, "Expected default fixture map index 1");
        Require(baseline.View.Position.x == 0.0f && baseline.View.Position.z == 3.0f,
                "Expected the fixture baseline map at index 1");
        for (GLuint texture : baseline.Textures)
            Require(texture != 0 && glIsTexture(texture) == GL_TRUE, "Baseline texture is invalid");
        game.Render();
        glFinish();
        RequireNoGLErrors("baseline render");

        CheckFailedSwitch(game, -1, "negative-index");
        CheckFailedSwitch(game, 99, "out-of-range-index");
        CheckFailedSwitch(game, 2, "missing-floor");
        CheckFailedSwitch(game, 3, "corrupt-floor");
        CheckFailedSwitch(game, 4, "missing-hdr");
        CheckFailedSwitch(game, 5, "missing-model-after-valid-model", true);
        CheckFailedSwitch(game, 6, "corrupt-model");
        CheckFailedSwitch(game, 7, "missing-material-texture");
        CheckFailedSwitch(game, 8, "missing-heightmap");

        Require(game.SwitchMap(0), "Valid switch failed after resource failures");
        const Snapshot replacement = Capture(game);
        Require(replacement.MapIndex == 0, "Valid switch did not commit map index 0");
        Require(replacement.View.Position.x == 2.0f && replacement.View.Position.z == 4.0f,
                "Valid switch did not apply the replacement camera spawn");
        for (std::size_t i = 0; i < baseline.Textures.size() - 1; ++i)
        {
            Require(glIsTexture(baseline.Textures[i]) == GL_FALSE, "Committed switch leaked an old map texture");
            Require(replacement.Textures[i] != 0 && glIsTexture(replacement.Textures[i]) == GL_TRUE,
                    "Committed switch contains an invalid texture");
        }
        Require(replacement.Textures.back() == baseline.Textures.back() &&
                glIsTexture(replacement.Textures.back()) == GL_TRUE, "Shared BRDF LUT was not preserved");
        game.Render();
        glFinish();
        RequireNoGLErrors("render after successful replacement");
        Require(game.SwitchMap(1), "Could not switch back to the baseline map");
        Require(game.GetCurrentMapIndex() == 1, "Switching back did not commit map index 1");
        game.Render();
        glFinish();
        RequireNoGLErrors("render after switching back");

        const Snapshot finalResources = Capture(game);
        game.Cleanup();
        for (GLuint texture : finalResources.Textures)
            Require(glIsTexture(texture) == GL_FALSE, "Cleanup leaked a live cached texture");
        RequireNoGLErrors("final cleanup");
        std::cout << "[map-loading] ALL PASS: 9 failed switches, commit, recovery, and cleanup" << std::endl;
    }
}

int main()
{
    try
    {
        Context context;
        GameManager game;
        Require(game.Initialize(context.Window), "GameManager baseline initialization failed");
        Run(game);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[map-loading] FAIL: " << error.what() << std::endl;
        return 1;
    }
}
