//
// Created by Mike on 2021/12/8.
//

#pragma once

#include <core/basic_types.h>
#include <scene/scene_node.h>

namespace luisa::render {

class Transform : public SceneNode {
public:
    Transform(Scene *scene, const SceneNodeDesc *desc) noexcept;
    [[nodiscard]] virtual bool is_static() const noexcept = 0;
    [[nodiscard]] virtual float4x4 matrix(float time) const noexcept = 0;
};

}