#include "shadow_mapper.h"

#include <cmath>
#include <iostream>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

ShadowMapper::ShadowMapper()
    : m_Size(4096),
      m_DepthFBO(0),
      m_DepthMap(0),
      m_LightSpaceMatrix(1.0f),
      m_PreviousViewport{0, 0, 0, 0},
      m_Initialized(false)
{
}

ShadowMapper::~ShadowMapper()
{
    Cleanup();
}

bool ShadowMapper::Initialize(unsigned int size)
{
    if (m_Initialized)
        return true;

    m_Size = size;
    m_DepthShader = std::make_unique<Shader>("shaders/shadow_depth.vs", "shaders/shadow_depth.fs");

    glGenFramebuffers(1, &m_DepthFBO);
    glGenTextures(1, &m_DepthMap);
    glBindTexture(GL_TEXTURE_2D, m_DepthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_Size, m_Size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, m_DepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "[ShadowMapper] Shadow framebuffer is incomplete." << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_Initialized = true;
    return true;
}

void ShadowMapper::Cleanup()
{
    if (m_DepthMap) { glDeleteTextures(1, &m_DepthMap); m_DepthMap = 0; }
    if (m_DepthFBO) { glDeleteFramebuffers(1, &m_DepthFBO); m_DepthFBO = 0; }
    m_DepthShader.reset();
    m_Initialized = false;
}

void ShadowMapper::BeginDepthPass(const glm::vec3& lightDirection, const Camera& camera)
{
    const glm::vec3 dir = glm::normalize(lightDirection);
    
    // Project view direction to horizontal XZ plane, preventing shadow box drifting up/down
    glm::vec3 horizontalFront = camera.Front;
    if (std::abs(horizontalFront.x) > 0.001f || std::abs(horizontalFront.z) > 0.001f)
    {
        horizontalFront = glm::normalize(glm::vec3(horizontalFront.x, 0.0f, horizontalFront.z));
    }
    
    // Position the shadow box center 10 meters in front of the camera at ground/feet level
    glm::vec3 center = camera.Position + horizontalFront * 10.0f;
    center.y = camera.Position.y - 1.5f;

    const float shadowDistance = 30.0f;
    const float orthoExtent = 16.0f;

    glm::vec3 lightPos = center - dir * shadowDistance;
    glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

    const float texelSize = (orthoExtent * 2.0f) / static_cast<float>(m_Size);
    glm::vec4 centerLightSpace = lightView * glm::vec4(center, 1.0f);
    centerLightSpace.x = std::floor(centerLightSpace.x / texelSize) * texelSize;
    centerLightSpace.y = std::floor(centerLightSpace.y / texelSize) * texelSize;
    const glm::vec3 snappedCenter = glm::vec3(glm::inverse(lightView) * centerLightSpace);
    lightPos = snappedCenter - dir * shadowDistance;
    lightView = glm::lookAt(lightPos, snappedCenter, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 lightProjection = glm::ortho(-orthoExtent, orthoExtent,
                                           -orthoExtent, orthoExtent,
                                           0.1f, shadowDistance * 2.0f);
    m_LightSpaceMatrix = lightProjection * lightView;

    glGetIntegerv(GL_VIEWPORT, m_PreviousViewport);
    glViewport(0, 0, m_Size, m_Size);
    glBindFramebuffer(GL_FRAMEBUFFER, m_DepthFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
}

void ShadowMapper::EndDepthPass()
{
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(m_PreviousViewport[0], m_PreviousViewport[1],
               m_PreviousViewport[2], m_PreviousViewport[3]);
}
