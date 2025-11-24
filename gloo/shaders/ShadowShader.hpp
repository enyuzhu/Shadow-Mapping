#ifndef SHADOW_SHADER_H_
#define SHADOW_SHADER_H_

#include "ShaderProgram.hpp"

namespace GLOO {
class ShadowShader : public ShaderProgram {
 public:
  ShadowShader();
  void SetWorldToLightNDC(const glm::mat4& matrix) const;
   
  void AssociateVertexArray(VertexArray& vertex_array) const;
  
  void SetTargetNode(const SceneNode& node,
                     const glm::mat4& model_matrix) const override;
};
}  // namespace GLOO

#endif