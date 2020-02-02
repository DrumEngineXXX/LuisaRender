//
// Created by Mike Smith on 2019/10/24.
//

#pragma once

#include <string_view>
#include <util/noncopyable.h>

#include "data_types.h"
#include "buffer.h"

namespace luisa {

struct KernelArgumentEncoder : Noncopyable {
    
    virtual ~KernelArgumentEncoder() noexcept = default;
    
    virtual void set_buffer(std::string_view argument_name, Buffer &buffer, size_t offset) = 0;
    virtual void set_bytes(std::string_view argument_name, const void *data, size_t size) = 0;
    
    void operator()(std::string_view argument_name, Buffer &buffer, size_t offset = 0ul) {
        set_buffer(argument_name, buffer, offset);
    }
    
    template<typename T>
    void operator()(std::string_view argument_name, BufferView<T> buffer_view) {
        set_buffer(argument_name, buffer_view.buffer(), buffer_view.byte_offset());
    }
    
    void operator()(std::string_view argument_name, const void *bytes, size_t size) {
        set_bytes(argument_name, bytes, size);
    }
    
    template<typename T>
    void operator()(std::string_view argument_name, T data) {
        set_bytes(argument_name, &data, sizeof(T));
    }
    
    template<typename T>
    void operator()(std::string_view argument_name, const T *data, size_t count) {
        set_bytes(argument_name, data, sizeof(T) * count);
    }
};

struct Kernel : Noncopyable {
    virtual ~Kernel() = default;
};

struct KernelDispatcher : Noncopyable {
    virtual ~KernelDispatcher() noexcept = default;
    virtual void operator()(Kernel &kernel, uint2 threadgroups, uint2 threadgroup_size, std::function<void(KernelArgumentEncoder &)> encode) = 0;
    virtual void operator()(Kernel &kernel, uint threadgroups, uint threadgroup_size, std::function<void(KernelArgumentEncoder &)> encode) {
        (*this)(kernel, {threadgroups, 1u}, {threadgroup_size, 1u}, std::move(encode));
    }
};

}