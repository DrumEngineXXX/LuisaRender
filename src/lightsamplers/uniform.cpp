//
// Created by Mike Smith on 2022/1/10.
//

#include <luisa-compute.h>
#include <base/light_sampler.h>
#include <base/pipeline.h>

namespace luisa::render {

class UniformLightSampler final : public LightSampler {

private:
    float _environment_weight{.5f};

public:
    UniformLightSampler(Scene *scene, const SceneNodeDesc *desc) noexcept
        : LightSampler{scene, desc},
          _environment_weight{desc->property_float_or_default("environment_weight", 0.5f)} {}
    [[nodiscard]] luisa::unique_ptr<Instance> build(Pipeline &pipeline, CommandBuffer &command_buffer) const noexcept override;
    [[nodiscard]] string_view impl_type() const noexcept override { return LUISA_RENDER_PLUGIN_NAME; }
    [[nodiscard]] auto environment_weight() const noexcept { return _environment_weight; }
};

class UniformLightSamplerInstance final : public LightSampler::Instance {

private:
    uint _light_buffer_id{0u};
    float _env_prob{0.f};

public:
    UniformLightSamplerInstance(const UniformLightSampler *sampler, Pipeline &pipeline, CommandBuffer &command_buffer) noexcept
        : LightSampler::Instance{pipeline, sampler} {
        if (!pipeline.lights().empty()) {
            auto [view, buffer_id] = pipeline.arena_buffer<Light::Handle>(pipeline.lights().size());
            _light_buffer_id = buffer_id;
            command_buffer << view.copy_from(pipeline.instanced_lights().data())
                           << compute::commit();
        }
        if (auto env = pipeline.environment()) {
            if (pipeline.lights().empty()) {
                _env_prob = 1.f;
            } else {
                _env_prob = std::clamp(
                    sampler->environment_weight(), 0.01f, 0.99f);
            }
        }
    }
    [[nodiscard]] Light::Evaluation evaluate_hit(
        const Interaction &it, Expr<float3> p_from,
        const SampledWavelengths &swl, Expr<float> time) const noexcept override {
        auto eval = Light::Evaluation::zero(swl.dimension());
        if (pipeline().lights().empty()) [[unlikely]] {// no lights
            LUISA_WARNING_WITH_LOCATION("No lights in scene.");
            return eval;
        }
        pipeline().dynamic_dispatch_light(it.shape()->light_tag(), [&](auto light) noexcept {
            auto closure = light->closure(swl, time);
            eval = closure->evaluate(it, p_from);
        });
        auto n = static_cast<float>(pipeline().lights().size());
        eval.pdf *= (1.f - _env_prob) / n;
        return eval;
    }
    [[nodiscard]] Light::Evaluation evaluate_miss(
        Expr<float3> wi, const SampledWavelengths &swl, Expr<float> time) const noexcept override {
        if (_env_prob == 0.f) [[unlikely]] {// no environment
            LUISA_WARNING_WITH_LOCATION("No environment in scene");
            return {.L = SampledSpectrum{swl.dimension()}, .pdf = 0.f};
        }
        auto eval = pipeline().environment()->evaluate(wi, swl, time);
        eval.pdf *= _env_prob;
        return eval;
    }
    [[nodiscard]] Light::Sample sample(
        Sampler::Instance &sampler, const Interaction &it_from,
        const SampledWavelengths &swl, Expr<float> time) const noexcept override {
        auto sample = Light::Sample::zero(swl.dimension());
        if (_env_prob > 0.f) {// consider environment
            auto u = sampler.generate_1d();
            $if(u < _env_prob) {
                sample = pipeline().environment()->sample(
                    it_from.p(), swl, time, sampler.generate_2d());
                sample.eval.pdf *= _env_prob;
            }
            $else {
                if (!pipeline().lights().empty()) {
                    auto n = static_cast<float>(pipeline().lights().size());
                    auto u_remapped = (u - _env_prob) / (1.f - _env_prob);
                    auto i = cast<uint>(clamp(u_remapped * n, 0.f, n - 1.f));
                    auto handle = pipeline().buffer<Light::Handle>(_light_buffer_id).read(i);
                    auto u_prim = sampler.generate_1d();
                    auto u_light = sampler.generate_2d();
                    pipeline().dynamic_dispatch_light(handle.light_tag, [&](auto light) noexcept {
                        auto closure = light->closure(swl, time);
                        sample = closure->sample(handle.instance_id, it_from.p(), u_prim, u_light);
                    });
                    sample.eval.pdf *= (1.f - _env_prob) / n;
                }
            };
        } else if (!pipeline().lights().empty()) {
            auto u = sampler.generate_1d();
            auto n = static_cast<float>(pipeline().lights().size());
            auto i = cast<uint>(clamp(u * n, 0.f, n - 1.f));
            auto handle = pipeline().buffer<Light::Handle>(_light_buffer_id).read(i);
            auto u_prim = sampler.generate_1d();
            auto u_light = sampler.generate_2d();
            pipeline().dynamic_dispatch_light(handle.light_tag, [&](auto light) noexcept {
                auto closure = light->closure(swl, time);
                sample = closure->sample(handle.instance_id, it_from.p(), u_prim, u_light);
            });
            sample.eval.pdf *= 1.f / n;
        } else {
            LUISA_WARNING_WITH_LOCATION("No light or environment to sample.");
        }
        return sample;
    }
};

unique_ptr<LightSampler::Instance> UniformLightSampler::build(
    Pipeline &pipeline, CommandBuffer &command_buffer) const noexcept {
    return luisa::make_unique<UniformLightSamplerInstance>(
        this, pipeline, command_buffer);
}

}// namespace luisa::render

LUISA_RENDER_MAKE_SCENE_NODE_PLUGIN(luisa::render::UniformLightSampler)
