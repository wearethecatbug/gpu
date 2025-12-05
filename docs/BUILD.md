# Building @kmamal/gpu with Custom Dawn Extensions

This document describes how to build the Dawn WebGPU implementation with custom extensions for native device/queue pointer access, enabling C++ FFI integration for features like Effekseer particle system.

## Overview

The @kmamal/gpu package provides WebGPU bindings for Node.js via Google's Dawn implementation. We've added custom extensions to expose raw device and queue pointers for native code integration:

- `getDevicePointer(device)` - Returns `WGPUDevice` pointer as BigInt
- `getQueuePointer(device)` - Returns `WGPUQueue` pointer as BigInt

## Prerequisites

### Required Software

| Software | Version | Installation |
|----------|---------|--------------|
| Node.js | 20+ | https://nodejs.org |
| pnpm | 9+ | `npm install -g pnpm` |
| Visual Studio 2022 BuildTools | Latest | See below |
| Ninja | Latest | `winget install Ninja-build.Ninja` |
| Go | 1.23+ | `winget install GoLang.Go` |
| Git | Latest | https://git-scm.com |

### Visual Studio 2022 BuildTools

Install from: https://visualstudio.microsoft.com/visual-cpp-build-tools/

Required workloads:
- Desktop development with C++
- Windows 10/11 SDK (any recent version)
- MSVC v143 build tools

### Clang

**Note**: Dawn includes bundled Clang at `third_party/llvm-build/Release+Asserts/bin/clang-cl.exe`. No external Clang installation needed.

## Repository Structure

```
references/gpu/
├── dawn/                    # Dawn source (fetched via gclient)
│   └── src/dawn/node/
│       └── Module.cpp       # Our modifications here
├── build/                   # CMake build output
├── dist/                    # Final dawn.node output
├── scripts/
│   ├── sync.mjs            # Runs gclient sync
│   ├── configure.mjs       # Runs CMake configure
│   ├── configure-only.mjs  # CMake without sync (for rebuilds)
│   └── build.mjs           # Runs ninja build
├── third_party/            # Dependencies (populated by gclient)
│   ├── gpuweb/             # WebGPU IDL definitions
│   ├── node-api-headers/   # Node.js N-API headers
│   └── node-addon-api/     # N-API C++ wrapper
├── dawn.patch              # Original patch file (reference only)
└── docs/
    └── BUILD.md            # This file
```

## Build Process

### Step 1: Clone the Repository

```powershell
cd C:\wrk\etherlords_pr_assets\references
git clone https://github.com/AsukaLabs/gpu.git
cd gpu
```

### Step 2: Install npm Dependencies

```powershell
npm install
```

### Step 3: Sync Dawn Sources (First Time Only)

This step uses Google's depot_tools to fetch Dawn and all its dependencies:

```powershell
node scripts/sync.mjs
```

**Note**: This downloads ~10GB of data and takes 30-60 minutes.

If gclient creates files in the wrong location, you may need to manually move `third_party` contents from `dawn/third_party/` to `references/gpu/third_party/`.

### Step 4: Verify/Fix Dependencies

After sync, verify these files exist:

#### webgpu.idl
```
third_party/gpuweb/webgpu.idl
```

**Critical**: Must use the correct version! Dawn expects specific IDL version.

If missing or wrong version, download from:
```
https://raw.githubusercontent.com/AsukaLabs/gpuweb/a2637f7b880c2556919cdb288fe89815e0ed1c41/spec/webgpu.idl
```

**Wrong version causes**: `error: duplicate member 'message'` in generated code.

#### node-api-headers

If `third_party/node-api-headers/` is empty:
```powershell
cd third_party/node-api-headers
npm pack node-api-headers@1.7.0
tar -xzf node-api-headers-1.7.0.tgz
xcopy /E /Y package\* .
del node-api-headers-1.7.0.tgz
rmdir /S /Q package
```

#### node-addon-api

If `third_party/node-addon-api/` is empty:
```powershell
cd third_party/node-addon-api
npm pack node-addon-api@8.5.0
tar -xzf node-addon-api-8.5.0.tgz
xcopy /E /Y package\* .
del node-addon-api-8.5.0.tgz
rmdir /S /Q package
```

### Step 5: Apply Custom Modifications

We have saved all Dawn modifications in this repository:

- `dawn-changes.patch` - Git patch for modified files (Module.cpp, CMakeLists, etc.)
- `dawn-additions/` - New files (SDL integration: webgpu_sdl.h, sdl/*.cpp)

**Option A: Apply patches (recommended)**

```powershell
cd dawn

# Apply modifications to existing files
git apply ../dawn-changes.patch

# Copy new SDL integration files
xcopy /E /Y ..\dawn-additions\sdl src\dawn\sdl\
copy ..\dawn-additions\webgpu_sdl.h include\webgpu\
```

**Option B: Manual modifications**

If patches don't apply cleanly, manually add these functions to `dawn/src/dawn/node/Module.cpp`:

#### Add getDevicePointer Function

After the `Create` function (around line 60), add:

```cpp
// Get raw WGPUDevice pointer for FFI
Napi::Value GetDevicePointer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected GPUDevice argument").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    Napi::Object jsDevice = info[0].As<Napi::Object>();
    
    // Get the binding GPUDevice which wraps the wgpu::Device
    wgpu::binding::GPUDevice* bindingDevice = 
        reinterpret_cast<wgpu::binding::GPUDevice*>(wgpu::interop::GPUDevice::Unwrap(jsDevice));
    
    if (!bindingDevice) {
        Napi::TypeError::New(env, "Invalid GPUDevice").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    // Get the underlying WGPUDevice handle
    WGPUDevice wgpuDevice = bindingDevice->device_.Get();
    
    // Return as BigInt (64-bit pointer)
    return Napi::BigInt::New(env, reinterpret_cast<uint64_t>(wgpuDevice));
}
```

#### Add getQueuePointer Function

```cpp
// Get raw WGPUQueue pointer for FFI
Napi::Value GetQueuePointer(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "Expected GPUDevice argument").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    Napi::Object jsDevice = info[0].As<Napi::Object>();
    
    // Get the binding GPUDevice
    wgpu::binding::GPUDevice* bindingDevice = 
        reinterpret_cast<wgpu::binding::GPUDevice*>(wgpu::interop::GPUDevice::Unwrap(jsDevice));
    
    if (!bindingDevice) {
        Napi::TypeError::New(env, "Invalid GPUDevice").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    WGPUDevice wgpuDevice = bindingDevice->device_.Get();
    
    // Get the DawnProcTable
    const DawnProcTable* procs = &dawn::native::GetProcs();
    
    // Get the queue from the device
    WGPUQueue wgpuQueue = procs->deviceGetQueue(wgpuDevice);
    
    return Napi::BigInt::New(env, reinterpret_cast<uint64_t>(wgpuQueue));
}
```

#### Register Exports

In the `Initialize` function, add to the exports object:

```cpp
exports.Set("getDevicePointer", Napi::Function::New<GetDevicePointer>(env));
exports.Set("getQueuePointer", Napi::Function::New<GetQueuePointer>(env));
```

### Step 6: Configure Build

Open Visual Studio Developer Command Prompt (x64), then:

```powershell
cd C:\wrk\etherlords_pr_assets\references\gpu
node scripts/configure.mjs
```

Or use configure-only.mjs to skip gclient sync on subsequent builds:

```powershell
node scripts/configure-only.mjs
```

### Step 7: Build

```powershell
cd build
ninja dawn.node
```

Build takes 20-40 minutes on first run (1109 compilation steps).

### Step 8: Verify Build

```powershell
node -e "console.log(Object.keys(require('./build/dawn.node')))"
```

Expected output:
```
[ 'globals', '_create', 'renderGPUDeviceToWindow', 'getDevicePointer', 'getQueuePointer' ]
```

### Step 9: Copy to Artifacts

```powershell
copy build\dawn.node ..\..\..\artifacts\dawn.node\win32\x64\dawn.node
```

## One-Liner Build Command

For rebuilds after modifications:

```powershell
# Refresh PATH (after installing Go/Ninja)
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

# Build (from Developer Command Prompt)
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat\" -arch=x64 && cd /d C:\wrk\etherlords_pr_assets\references\gpu && node scripts/configure-only.mjs && cd build && ninja dawn.node"
```

## Troubleshooting

### Error: "ninja: command not found"

Install Ninja:
```powershell
winget install Ninja-build.Ninja
```

Then refresh PATH or restart terminal.

### Error: "go: command not found"

Install Go:
```powershell
winget install GoLang.Go
```

Then refresh PATH or restart terminal.

### Error: "duplicate member 'message'" during IDL generation

Wrong webgpu.idl version. Download the correct version:
```powershell
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/AsukaLabs/gpuweb/a2637f7b880c2556919cdb288fe89815e0ed1c41/spec/webgpu.idl" -OutFile "third_party/gpuweb/webgpu.idl"
```

### Error: "Cannot find node_api.h" or "Cannot find napi.h"

The node-api-headers or node-addon-api packages are empty. Repopulate them using npm pack (see Step 4).

### Error: "cl.exe not found" or MSVC errors

Run from Visual Studio Developer Command Prompt (x64) or use:
```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat\" -arch=x64 && <your-command>"
```

### Build succeeds but exports missing

The modifications to Module.cpp weren't applied. Verify the file contains `getDevicePointer` and `getQueuePointer` functions and exports.

## Usage in Application

```javascript
const dawn = require('@kmamal/gpu');

// Create device
const adapter = await navigator.gpu.requestAdapter();
const device = await adapter.requestDevice();

// Get raw pointers for FFI
const devicePtr = dawn.getDevicePointer(device);
const queuePtr = dawn.getQueuePointer(device);

console.log('Device pointer:', devicePtr.toString(16));
console.log('Queue pointer:', queuePtr.toString(16));

// Pass to native code (e.g., Effekseer)
nativeBinding.initialize(devicePtr, queuePtr);
```

## Version Information

- @kmamal/gpu: v0.2.12 (fork)
- Dawn: Chromium main branch (as of build date)
- webgpu.idl: gpuweb/gpuweb commit `a2637f7b880c2556919cdb288fe89815e0ed1c41`
- node-api-headers: v1.7.0
- node-addon-api: v8.5.0
- Build tools: VS 2022, CMake, Ninja, Clang (bundled), Go 1.25+

## Related Files

- `artifacts/dawn.node/win32/x64/dawn.node` - Prebuilt binary
- `references/gpu/dawn-module.patch` - Dawn source modifications (Module.cpp, CMakeLists)
- `references/gpu/wrapper.patch` - Wrapper (src/index.js) to export our functions
- `references/gpu/dawn.patch` - Original patch (may not apply cleanly)
- `packages/platforms/native/src/particles/` - Effekseer integration using these pointers

## Post-Install Step

After `pnpm install` or `npm install`, you need to:

1. Replace `node_modules/@kmamal/gpu/dist/dawn.node` with our custom build
2. Apply `wrapper.patch` to `node_modules/@kmamal/gpu/src/index.js`

Or use the artifacts:
```powershell
# Copy custom dawn.node
copy artifacts\dawn.node\win32\x64\dawn.node node_modules\@kmamal\gpu\dist\dawn.node

# Apply wrapper patch (or manually add getDevicePointer/getQueuePointer exports)
cd node_modules\@kmamal\gpu
git apply ..\..\..\references\gpu\wrapper.patch
```

## Linux Build (for CI/CD)

### Prerequisites (Ubuntu/Debian)

```bash
# Build essentials
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build golang-go python3

# Additional dependencies for Dawn
sudo apt-get install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### Build Steps

```bash
cd references/gpu

# Sync Dawn sources
node scripts/sync.mjs

# Configure and build
node scripts/configure.mjs
cd build
ninja dawn.node

# Copy to artifacts
mkdir -p ../../../artifacts/dawn.node/linux/x64
cp dawn.node ../../../artifacts/dawn.node/linux/x64/
```

### CI/CD Integration

The `tools/scripts/dawn/setup-dawn-node.mjs` helper automatically:
1. Checks for prebuilt artifacts in `artifacts/dawn.node/<platform>/<arch>/`
2. Copies to `node_modules/@kmamal/gpu/dist/` if found
3. Falls back to @kmamal release downloads if not

For CI, either:
- Pre-populate `artifacts/dawn.node/linux/x64/dawn.node` in the repo
- Or build as part of CI pipeline and cache the result

