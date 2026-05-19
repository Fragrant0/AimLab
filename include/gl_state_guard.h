#ifndef GL_STATE_GUARD_H
#define GL_STATE_GUARD_H

#include <glad/glad.h>

class DepthFuncGuard
{
public:
    explicit DepthFuncGuard(GLenum newFunc)
    {
        glGetIntegerv(GL_DEPTH_FUNC, &m_Previous);
        glDepthFunc(newFunc);
    }

    ~DepthFuncGuard()
    {
        glDepthFunc(static_cast<GLenum>(m_Previous));
    }

    DepthFuncGuard(const DepthFuncGuard&) = delete;
    DepthFuncGuard& operator=(const DepthFuncGuard&) = delete;

private:
    GLint m_Previous;
};

class PolygonModeGuard
{
public:
    explicit PolygonModeGuard(GLenum newMode)
    {
        glGetIntegerv(GL_POLYGON_MODE, m_Previous);
        glPolygonMode(GL_FRONT_AND_BACK, newMode);
    }

    ~PolygonModeGuard()
    {
        glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(m_Previous[0]));
    }

    PolygonModeGuard(const PolygonModeGuard&) = delete;
    PolygonModeGuard& operator=(const PolygonModeGuard&) = delete;

private:
    GLint m_Previous[2];
};

class CapabilityGuard
{
public:
    CapabilityGuard(GLenum capability, bool enabled)
        : m_Capability(capability),
          m_Previous(glIsEnabled(capability))
    {
        if (enabled)
            glEnable(capability);
        else
            glDisable(capability);
    }

    ~CapabilityGuard()
    {
        if (m_Previous)
            glEnable(m_Capability);
        else
            glDisable(m_Capability);
    }

    CapabilityGuard(const CapabilityGuard&) = delete;
    CapabilityGuard& operator=(const CapabilityGuard&) = delete;

private:
    GLenum m_Capability;
    GLboolean m_Previous;
};

class BlendFuncGuard
{
public:
    BlendFuncGuard(GLenum srcFactor, GLenum dstFactor)
    {
        glGetIntegerv(GL_BLEND_SRC_RGB, &m_PreviousSrcRGB);
        glGetIntegerv(GL_BLEND_DST_RGB, &m_PreviousDstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_PreviousSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &m_PreviousDstAlpha);
        glBlendFunc(srcFactor, dstFactor);
    }

    ~BlendFuncGuard()
    {
        glBlendFuncSeparate(static_cast<GLenum>(m_PreviousSrcRGB),
                            static_cast<GLenum>(m_PreviousDstRGB),
                            static_cast<GLenum>(m_PreviousSrcAlpha),
                            static_cast<GLenum>(m_PreviousDstAlpha));
    }

    BlendFuncGuard(const BlendFuncGuard&) = delete;
    BlendFuncGuard& operator=(const BlendFuncGuard&) = delete;

private:
    GLint m_PreviousSrcRGB;
    GLint m_PreviousDstRGB;
    GLint m_PreviousSrcAlpha;
    GLint m_PreviousDstAlpha;
};

class DepthMaskGuard
{
public:
    explicit DepthMaskGuard(GLboolean enabled)
    {
        glGetBooleanv(GL_DEPTH_WRITEMASK, &m_Previous);
        glDepthMask(enabled);
    }

    ~DepthMaskGuard()
    {
        glDepthMask(m_Previous);
    }

    DepthMaskGuard(const DepthMaskGuard&) = delete;
    DepthMaskGuard& operator=(const DepthMaskGuard&) = delete;

private:
    GLboolean m_Previous;
};

#endif
