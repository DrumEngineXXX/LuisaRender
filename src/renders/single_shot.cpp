//
// Created by Mike Smith on 2020/2/11.
//

#include "single_shot.h"

namespace luisa {

SingleShot::SingleShot(Device *device, const ParameterSet &parameter_set)
    : Render{device, parameter_set},
      _shutter_open{parameter_set["shutter_open"].parse_float_or_default(0.0f)},
      _shutter_close{parameter_set["shutter_close"].parse_float_or_default(0.0f)},
      _camera{parameter_set["camera"].parse<Camera>()},
      _output_path_prefix{std::filesystem::absolute(parameter_set["output_prefix"].parse_string_or_default("output"))},
      _command_queue_size{parameter_set["command_queue_size"].parse_uint_or_default(4u)} {
    
    auto viewport = parameter_set["viewport"].parse_uint4_or_default(make_uint4(0u, 0u, _camera->film().resolution()));
    _viewport = {make_uint2(viewport.x, viewport.y), make_uint2(viewport.z, viewport.w)};
    
    if (_shutter_open > _shutter_close) { std::swap(_shutter_open, _shutter_close); }
    
    auto shapes = parameter_set["shapes"].parse_reference_list<Shape>();
    auto lights = parameter_set["lights"].parse_reference_list<Light>();
    _scene = Scene::create(_device, shapes, lights, _shutter_open);
}

void SingleShot::execute() {
    
    std::vector<float> time_samples(_sampler->spp());
    std::default_random_engine random_engine{std::random_device{}()};
    std::uniform_real_distribution<float> uniform{0.0f, 1.0f};
    
    for (auto i = 0u; i < time_samples.size(); i++) { time_samples[i] = lerp(_shutter_open, _shutter_close, (i + uniform(random_engine)) / time_samples.size()); }
    std::shuffle(time_samples.begin(), time_samples.end(), random_engine);
    
    _sampler->reset_states(_camera->film().resolution(), _viewport);
    _camera->film().reset_accumulation_buffer(_viewport);
    
    for (auto time : time_samples) {
        
        _camera->update(time);
        _scene->update(time);
        
        _integrator->prepare_for_frame(_scene.get(), _camera.get(), _sampler.get(), _viewport);
        
        // wait for command queue
        {
            std::unique_lock lock{_mutex};
            _cv.wait(lock, [this]() noexcept { return _working_command_count < _command_queue_size; });
            _working_command_count++;
        }
        
        // render frame
        _device->launch_async([&](KernelDispatcher &dispatch) {
            _sampler->start_next_frame(dispatch);
            _integrator->render_frame(dispatch);
        }, [this, frame_index = _sampler->frame_index()] {  // notify that one frame has been rendered
            {
                std::lock_guard lock_guard{_mutex};
                _working_command_count--;
                std::cout << "Progress: " << frame_index + 1u << "/" << _sampler->spp() << std::endl;
            }
            _cv.notify_one();
        });
    }
    
    // Note: sync commands finishes after all async commands launched before
    _device->launch([&](KernelDispatcher &dispatch) {  // film postprocess
        _camera->film().postprocess(dispatch);
    });
    
    if (!std::filesystem::exists(_output_path_prefix)) {
        std::filesystem::create_directory(_output_path_prefix);
    }
    _camera->film().save(_output_path_prefix / "result");
    
}

}