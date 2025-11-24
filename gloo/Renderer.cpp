
#include <cassert>
#include <iostream>
#include <glad/glad.h>
#include <glm/gtx/string_cast.hpp>

#include "Application.hpp"
#include "Scene.hpp"
#include "utils.hpp"
#include "gl_wrapper/BindGuard.hpp"
#include "shaders/ShaderProgram.hpp"
#include "components/ShadingComponent.hpp"
#include "components/CameraComponent.hpp"
#include "debug/PrimitiveFactory.hpp"
#include "gl_wrapper/Framebuffer.hpp"
#include "shaders/ShadowShader.hpp"

namespace {
const size_t kShadowWidth = 4096;
const size_t kShadowHeight = 4096;
const glm::mat4 kLightProjection =
    glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 80.0f);
}  // namespace

namespace GLOO {
Renderer::Renderer(Application& application) : application_(application) {
  UNUSED(application_);

  // Initialize shadow map texture 
  shadow_depth_tex_ = make_unique<Texture>();
  shadow_depth_tex_->Reserve(
      GL_DEPTH_COMPONENT,  // Internal format: store depth only
      kShadowWidth,        // Width: 4096
      kShadowHeight,       // Height: 4096
      GL_DEPTH_COMPONENT,  // Format: depth
      GL_FLOAT             // Type: floating point
  );
  
  // Create framebuffer for shadow rendering
  shadow_fbo_ = make_unique<Framebuffer>();
  shadow_fbo_->AssociateTexture(*shadow_depth_tex_, GL_DEPTH_ATTACHMENT);
  
  // Initialize shadow shader
  shadow_shader_ = make_unique<ShadowShader>();
  
  // Initialize quad for debug visualization
  plain_texture_shader_ = make_unique<PlainTextureShader>();
  quad_ = PrimitiveFactory::CreateQuad();
}

void Renderer::RenderShadow(const Scene& scene,
                            const LightComponent& light) const {
  // Only directional lights cast shadows in this assignment
  if (light.GetLightPtr()->GetType() != LightType::Directional) {
    return;
  }
  
  // Bind shadow framebuffer (render to shadow_depth_tex_ instead of screen)
  BindGuard fbo_guard(shadow_fbo_.get());
  
  // Set viewport to shadow map resolution
  GL_CHECK(glViewport(0, 0, kShadowWidth, kShadowHeight));
  
  // Enable depth writing, disable color writing (we only care about depth)
  GL_CHECK(glDepthMask(GL_TRUE));
  GL_CHECK(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));
  
  // Clear the depth buffer
  GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT));
  
  // Calculate light's view-projection matrix
  const SceneNode* light_node = light.GetNodePtr();
  glm::mat4 light_view_matrix = glm::inverse(light_node->GetTransform().GetLocalToWorldMatrix());
  glm::mat4 light_projection_matrix = kLightProjection;
  glm::mat4 world_to_light_ndc_matrix = light_projection_matrix * light_view_matrix;
  
  // Get all objects to render
  auto rendering_info = RetrieveRenderingInfo(scene);
  
  // Bind shadow shader once for all objects
  BindGuard shader_guard(shadow_shader_.get());
  shadow_shader_->SetWorldToLightNDC(world_to_light_ndc_matrix);

  // Render each object from light's perspective
  for (const auto& pr : rendering_info) {
    auto robj_ptr = pr.first;
    const glm::mat4& model_matrix = pr.second;
    SceneNode& node = *robj_ptr->GetNodePtr();
    
    // Set node-specific uniforms (model matrix)
    shadow_shader_->SetTargetNode(node, model_matrix);
    
    // Render the object (depth gets written to shadow_depth_tex_)
    robj_ptr->Render();
  }
}

void Renderer::SetRenderingOptions() const {
  GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));

  // Enable depth test.
  GL_CHECK(glEnable(GL_DEPTH_TEST));
  GL_CHECK(glDepthFunc(GL_LEQUAL));

  // Enable blending for multi-pass forward rendering.
  GL_CHECK(glEnable(GL_BLEND));
  GL_CHECK(glBlendFunc(GL_ONE, GL_ONE));
}

void Renderer::Render(const Scene& scene) const {
  SetRenderingOptions();
  RenderScene(scene);
  DebugShadowMap();
}

void Renderer::RecursiveRetrieve(const SceneNode& node,
                                 RenderingInfo& info,
                                 const glm::mat4& model_matrix) {
  // model_matrix is parent to world transformation.
  glm::mat4 new_matrix =
      model_matrix * node.GetTransform().GetLocalToParentMatrix();
  auto robj_ptr = node.GetComponentPtr<RenderingComponent>();
  if (robj_ptr != nullptr && node.IsActive())
    info.emplace_back(robj_ptr, new_matrix);

  size_t child_count = node.GetChildrenCount();
  for (size_t i = 0; i < child_count; i++) {
    RecursiveRetrieve(node.GetChild(i), info, new_matrix);
  }
}

Renderer::RenderingInfo Renderer::RetrieveRenderingInfo(
    const Scene& scene) const {
  RenderingInfo info;
  const SceneNode& root = scene.GetRootNode();
  // Efficient implementation without redundant matrix multiplications.
  RecursiveRetrieve(root, info, glm::mat4(1.0f));
  return info;
}

void Renderer::RenderScene(const Scene& scene) const {
  GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  const SceneNode& root = scene.GetRootNode();
  auto rendering_info = RetrieveRenderingInfo(scene);
  auto light_ptrs = root.GetComponentPtrsInChildren<LightComponent>();
  if (light_ptrs.size() == 0) {
    // Make sure there are at least 2 passes of we don't forget to set color
    // mask back.
    return;
  }

  CameraComponent* camera = scene.GetActiveCameraPtr();

  {
    // Here we first do a depth pass (note that this has nothing to do with the
    // shadow map). The goal of this depth pass is to exclude pixels that are
    // not really visible from the camera, in later rendering passes. You can
    // safely leave this pass here without understanding/modifying it, for
    // assignment 5. If you are interested in learning more, see
    // https://www.khronos.org/opengl/wiki/Early_Fragment_Test#Optimization

    GL_CHECK(glDepthMask(GL_TRUE));
    bool color_mask = GL_FALSE;
    GL_CHECK(glColorMask(color_mask, color_mask, color_mask, color_mask));

    for (const auto& pr : rendering_info) {
      auto robj_ptr = pr.first;
      SceneNode& node = *robj_ptr->GetNodePtr();
      auto shading_ptr = node.GetComponentPtr<ShadingComponent>();
      if (shading_ptr == nullptr) {
        std::cerr << "Some mesh is not attached with a shader during rendering!"
                  << std::endl;
        continue;
      }
      ShaderProgram* shader = shading_ptr->GetShaderPtr();

      BindGuard shader_bg(shader);

      // Set various uniform variables in the shaders.
      shader->SetTargetNode(node, pr.second);
      shader->SetCamera(*camera);

      robj_ptr->Render();
    }
  }

  // The real shadow map/Phong shading passes.
  for (size_t light_id = 0; light_id < light_ptrs.size(); light_id++) {
    LightComponent& light = *light_ptrs.at(light_id);
    
    // Render shadow map from light's perspective
    if (light.GetLightPtr()->GetType() == LightType::Directional) {
      RenderShadow(scene, light);
      
      // Reset viewport back to window size
      glm::ivec2 window_size = application_.GetWindowSize();
      GL_CHECK(glViewport(0, 0, window_size.x, window_size.y));
    }

    GL_CHECK(glDepthMask(GL_FALSE));
    bool color_mask = GL_TRUE;
    GL_CHECK(glColorMask(color_mask, color_mask, color_mask, color_mask));

    for (const auto& pr : rendering_info) {
      auto robj_ptr = pr.first;
      SceneNode& node = *robj_ptr->GetNodePtr();
      auto shading_ptr = node.GetComponentPtr<ShadingComponent>();
      if (shading_ptr == nullptr) {
        std::cerr << "Some mesh is not attached with a shader during rendering!"
                  << std::endl;
        continue;
      }
      ShaderProgram* shader = shading_ptr->GetShaderPtr();

      BindGuard shader_bg(shader);

      // Set various uniform variables in the shaders.
      shader->SetTargetNode(node, pr.second);
      shader->SetCamera(*camera);

      LightComponent& light = *light_ptrs.at(light_id);
      shader->SetLightSource(light);

      // Pass shadow map to shader if light casts shadows
      if (light.GetLightPtr()->GetType() == LightType::Directional) {
        const SceneNode* light_node = light.GetNodePtr();
        glm::mat4 light_view_matrix = glm::inverse(light_node->GetTransform().GetLocalToWorldMatrix());
        glm::mat4 world_to_light_ndc_matrix = kLightProjection * light_view_matrix;
  
  shader->SetShadowMapping(*shadow_depth_tex_, world_to_light_ndc_matrix);
}
      robj_ptr->Render();
    }
  }

  // Re-enable writing to depth buffer.
  GL_CHECK(glDepthMask(GL_TRUE));
}

void Renderer::RenderTexturedQuad(const Texture& texture, bool is_depth) const {
  BindGuard shader_bg(plain_texture_shader_.get());
  plain_texture_shader_->SetVertexObject(*quad_);
  plain_texture_shader_->SetTexture(texture, is_depth);
  quad_->GetVertexArray().Render();
}

void Renderer::DebugShadowMap() const {
  GL_CHECK(glDisable(GL_DEPTH_TEST));
  GL_CHECK(glDisable(GL_BLEND));

  glm::ivec2 window_size = application_.GetWindowSize();
  glViewport(0, 0, window_size.x / 4, window_size.y / 4);
  RenderTexturedQuad(*shadow_depth_tex_, true);

  glViewport(0, 0, window_size.x, window_size.y);
}
}  // namespace GLOO
