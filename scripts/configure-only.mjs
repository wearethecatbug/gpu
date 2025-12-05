import Fs from 'fs'
import Path from 'path'
import { execSync } from 'child_process'
import C from './util/common.js'

// Skip gclient sync - already done
// Skip abseil patch if already applied
const abseilPatchCheck = Path.join(C.dir.abseil, '.patched')
if (!Fs.existsSync(abseilPatchCheck)) {
    console.log("applying abseil-cpp.patch")
    process.chdir(C.dir.abseil)
    try {
        execSync(`git apply --ignore-space-change --ignore-whitespace ${Path.join(C.dir.root, 'abseil-cpp.patch')}`, {
            stdio: 'inherit',
        })
        Fs.writeFileSync(abseilPatchCheck, '')
    } catch (e) {
        console.log("Patch may already be applied, continuing...")
    }
} else {
    console.log("abseil-cpp.patch already applied, skipping")
}

process.chdir(C.dir.dawn)
console.log("configure build in", C.dir.build)

await Fs.promises.rm(C.dir.build, { recursive: true }).catch(() => { })
await Fs.promises.mkdir(C.dir.build, { recursive: true })

let CFLAGS
let LDFLAGS
let crossCompileFlag
let backendFlags = []
let cmakeCompilerFlags = []

// Windows: Use system Ninja and MSVC (don't rely on bundled clang-cl)
if (C.platform === 'win32') {
    const bundledClang = Path.join(C.dir.dawn, 'third_party', 'llvm-build', 'Release+Asserts', 'bin', 'clang-cl.exe')

    if (!Fs.existsSync(bundledClang)) {
        console.log('Using system Ninja and MSVC (bundled clang-cl not found)')
        // Let CMake find system cl.exe and ninja in PATH
        cmakeCompilerFlags = []
    } else {
        console.log('Using bundled Ninja and clang-cl')
        const ninjaPath = Path.join(C.dir.dawn, 'third_party', 'ninja', 'ninja.exe')
        cmakeCompilerFlags = [
            `-DCMAKE_MAKE_PROGRAM="${ninjaPath}"`,
            `-DCMAKE_C_COMPILER="${bundledClang}"`,
            `-DCMAKE_CXX_COMPILER="${bundledClang}"`,
        ]
    }
}
else if (C.platform === 'darwin') {
    let arch = process.env.CROSS_COMPILE_ARCH ?? C.arch
    if (arch === 'x64') { arch = 'x86_64' }

    crossCompileFlag = `-DCMAKE_OSX_ARCHITECTURES="${arch}"`

    if (C.targetArch === 'arm64') {
        CFLAGS = '-mmacosx-version-min=11.0'
        LDFLAGS = '-mmacosx-version-min=11.0'
    }
    else {
        CFLAGS = [
            '-mmacosx-version-min=10.9',
            '-DMAC_OS_X_VERSION_MIN_REQUIRED=1070',
        ].join(' ')
        LDFLAGS = '-mmacosx-version-min=10.9'
    }
}
else if (C.platform === 'linux') {
    backendFlags = [
        '-DDAWN_USE_X11=ON',
        '-DDAWN_USE_WAYLAND=OFF',
    ]
}

console.log('Running cmake...')
execSync(`cmake ${[
    '-S',
    `"${C.dir.dawn}"`,
    '-B',
    `"${C.dir.build}"`,
    '-GNinja',
    '-DCMAKE_BUILD_TYPE=Release',
    '-DDAWN_BUILD_NODE_BINDINGS=ON',
    '-DDAWN_BUILD_SAMPLES=OFF',
    '-DTINT_BUILD_TESTS=OFF',
    '-DTINT_BUILD_CMD_TOOLS=OFF',
    '-DDAWN_USE_GLFW=OFF',
    '-DDAWN_SUPPORTS_GLFW_FOR_WINDOWING=OFF',
    '-DDAWN_ENABLE_PIC=ON',
    '-DDAWN_ENABLE_SPIRV_VALIDATION=ON',
    '-DDAWN_ALWAYS_ASSERT=ON',
    '-DDAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON',
    ...cmakeCompilerFlags,
    crossCompileFlag,
    ...backendFlags,
].filter(Boolean).join(' ')}`, {
    stdio: 'inherit',
    env: {
        ...process.env,
        ...C.depotTools.env,
        CFLAGS,
        LDFLAGS,
    },
})

console.log('Configure complete!')
