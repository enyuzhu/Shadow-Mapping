#include "ShadowShader.hpp"

#include <stdexcept>
#include "gloo/SceneNode.hpp"
#include "gloo/components/RenderingComponent.hpp"

namespace GLOO {
ShadowShader::ShadowShader()
    : ShaderProgram(std::unordered_map<GLenum, std::string>{
          {GL_VERTEX_SHADER, "shadow.vert"},
          {GL_FRAGMENT_SHADER, "shadow.frag"}}) {
}

void ShadowShader::AssociateVertexArray(VertexArray& vertex_array) const {
  if (!vertex_array.HasPositionBuffer()) {
    throw std::runtime_error("Shadow shader requires vertex positions!");
  }
  vertex_array.LinkPositionBuffer(GetAttributeLocation("vertex_position"));
}

void ShadowShader::SetTargetNode(const SceneNode& node,
                                 const glm::mat4& model_matrix) const {
  // Associate the VAO
  AssociateVertexArray(node.GetComponentPtr<RenderingComponent>()
                           ->GetVertexObjectPtr()
                           ->GetVertexArray());
  
  // Set the model matrix (object to world transform)
  SetUniform("model_matrix", model_matrix);
                                 }
                                 
void ShadowShader::SetWorldToLightNDC(const glm::mat4& matrix) const {
  SetUniform("world_to_light_ndc_matrix", matrix);
}
}  // namespace GLOO