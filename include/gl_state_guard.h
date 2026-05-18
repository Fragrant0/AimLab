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
        glPolygonMode(GL_FRONT, static_cast<GLenum>(m_Previous[0]));
        glPolygonMode(GL_BACK, static_cast<GLenum>(m_Previous[1]));
    }

    PolygonModeGuard(const PolygonModeGuard&) = delete;
    PolygonModeGuard& operator=(const PolygonModeGuard&) = delete;

private:
    GLint m_Previous[2];
};

#endif
