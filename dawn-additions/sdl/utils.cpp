// Copyright 2020 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstdlib>
#include <memory>
#include <utility>

#include "dawn/common/Platform.h"
#include "webgpu/webgpu_sdl.h"

namespace wgpu::sdl {

#if DAWN_PLATFORM_IS(WINDOWS)
    #include <windows.h>
    struct NativeData {
        HWND hwnd;
        HINSTANCE hinstance;
    };
#elif defined(DAWN_ENABLE_BACKEND_METAL)
    class CALayer;
    struct NativeData {
        CALayer *layer;
    };
#elif defined(DAWN_USE_X11)
    #include <X11/Xlib.h>
    struct NativeData {
        Display* display;
        Window window;
    };
#endif

std::unique_ptr<WGPUChainedStruct, void (*)(WGPUChainedStruct*)> GetSurfaceDescriptor(NativeData* native) {
#if DAWN_PLATFORM_IS(WINDOWS)
    WGPUSurfaceSourceWindowsHWND* desc = new WGPUSurfaceSourceWindowsHWND();
    desc->chain.next = nullptr;
    desc->chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    desc->hwnd = native->hwnd;
    desc->hinstance = native->hinstance;
    return {
        reinterpret_cast<WGPUChainedStruct*>(desc),
        [](WGPUChainedStruct* desc) {
            delete reinterpret_cast<WGPUSurfaceSourceWindowsHWND*>(desc);
        }
    };
#elif defined(DAWN_ENABLE_BACKEND_METAL)
    WGPUSurfaceSourceMetalLayer* desc = new WGPUSurfaceSourceMetalLayer();
    desc->chain.next = nullptr;
    desc->chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    desc->layer = native->layer;
    return {
        reinterpret_cast<WGPUChainedStruct*>(desc),
        [](WGPUChainedStruct* desc) {
            delete reinterpret_cast<WGPUSurfaceSourceMetalLayer*>(desc);
        }
    };
#elif defined(DAWN_USE_X11)
    WGPUSurfaceSourceXlibWindow* desc = new WGPUSurfaceSourceXlibWindow();
    desc->chain.next = nullptr;
    desc->chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    desc->display = native->display;
    desc->window = native->window;
    return {
        reinterpret_cast<WGPUChainedStruct*>(desc),
        [](WGPUChainedStruct* desc) {
            delete reinterpret_cast<WGPUSurfaceSourceXlibWindow*>(desc);
        }
    };
#else
    return { nullptr, [](WGPUChainedStruct*) {} };
#endif
}

WGPUSurface CreateSurfaceForWindow(const DawnProcTable *procs, const WGPUInstance& instance, void* window) {
    std::unique_ptr<WGPUChainedStruct, void (*)(WGPUChainedStruct*)> chainedDescriptor = GetSurfaceDescriptor((NativeData*) window);

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = chainedDescriptor.get();
    WGPUSurface surface = procs->instanceCreateSurface(instance, &surfaceDesc);

    return surface;
}

}  // namespace wgpu::sdl
