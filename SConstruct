# Native SCons build, following the Environment/Program style of the course projects.
import os
from pathlib import Path
import shutil
import subprocess
import sys

from SCons.Script import (
    ARGUMENTS,
    Action,
    Alias,
    AlwaysBuild,
    Default,
    Environment,
    Exit,
    GetOption,
    Help,
    SConsignFile,
)

Help("""Build: python -m SCons [-j4] [mode=debug|release] [toolchain=mingw|msvc]
Check: python -m SCons check
Clean: python -m SCons -c (use the same mode/toolchain/deps arguments as the build)
Dependencies: python scripts/setup_dependencies.py; override with deps=directory
Output: ./watercolor[.exe] (intermediate files stay under build-scons/)
Linux/macOS: install GLFW and pkg-config; this build uses pkg-config glfw3.
CMake is required to build the pinned Assimp dependency.
""")
if GetOption("help"):
    Exit(0)

# Separate object directories let debug/release and compiler configurations coexist.
mode = ARGUMENTS.get("mode", "debug")
toolchain = ARGUMENTS.get("toolchain", "mingw" if sys.platform == "win32" else "default")
if mode not in ("debug", "release") or toolchain not in ("mingw", "msvc", "default"):
    print("Invalid mode or toolchain; see python -m SCons -h")
    Exit(1)
tools = {"mingw": ["mingw"], "msvc": ["msvc", "mslink", "mslib"]}.get(toolchain)
env = Environment(ENV=dict(os.environ), **({"tools": tools} if tools else {}))
build = Path("build-scons") / toolchain / mode
deps = Path(ARGUMENTS.get("deps", ".deps")).resolve()
# One signature database tracks the shared root executable across configurations.
SConsignFile("build-scons/.sconsign.dblite")
for name in ("glfw", "glm", "glad", "imgui", "tinyobjloader", "eigen", "assimp"):
    if not (deps / (name + "-src")).is_dir():
        print("Missing dependencies. Run: python scripts/setup_dependencies.py")
        Exit(1)

assimp_src = deps / "assimp-src"
assimp_build = (build / "assimp").resolve()
assimp_libdir = assimp_build / "lib"
assimp_library = assimp_libdir / (
    "assimp.lib" if sys.platform == "win32" and toolchain == "msvc" else "libassimp.a"
)

if env.get("CXX") == "cl":
    env.Append(
        CXXFLAGS=["/std:c++17", "/EHsc", "/W4"],
        CCFLAGS=["/Od", "/Zi"] if mode == "debug" else ["/O2"],
        LINKFLAGS=["/DEBUG"] if mode == "debug" else [],
    )
else:
    env.Append(
        CXXFLAGS=["-std=c++17"],
        CCFLAGS=["-Wall", "-Wextra", "-Wpedantic"]
        + (["-g", "-O0"] if mode == "debug" else ["-O2", "-DNDEBUG"]),
    )
# GLAD supplies OpenGL declarations; prevent GLFW from including competing GL headers.
env.Append(
    CPPDEFINES=["GLFW_INCLUDE_NONE"],
    CPPPATH=[
        "#src",
        str(deps / "eigen-src"),
        str(deps / "glm-src"),
        str(deps / "tinyobjloader-src"),
        str(deps / "glfw-src/include"),
        str(deps / "imgui-src"),
        str(deps / "imgui-src/backends"),
        str(assimp_src / "include"),
        str(assimp_build / "include"),
        str((build / "generated/include").resolve()),
    ],
)


def build_assimp(target, source, env):
    cmake = env.WhereIs("cmake")
    if not cmake:
        raise RuntimeError("CMake is required to build Assimp")

    configure = [
        cmake,
        "-S",
        str(assimp_src),
        "-B",
        str(assimp_build),
        "-DCMAKE_BUILD_TYPE=" + ("Debug" if mode == "debug" else "Release"),
        "-DBUILD_SHARED_LIBS=OFF",
        "-DASSIMP_BUILD_TESTS=OFF",
        "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF",
        "-DASSIMP_BUILD_SAMPLES=OFF",
        "-DASSIMP_BUILD_DOCS=OFF",
        "-DASSIMP_BUILD_ZLIB=" + ("ON" if sys.platform == "win32" else "OFF"),
        "-DASSIMP_INSTALL=OFF",
        "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
        "-DASSIMP_INJECT_DEBUG_POSTFIX=OFF",
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=" + str(assimp_libdir),
    ]
    for config in ("DEBUG", "RELEASE", "RELWITHDEBINFO", "MINSIZEREL"):
        configure.append(
            "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_{}={}".format(config, assimp_libdir)
        )
    if sys.platform == "win32" and toolchain == "mingw":
        configure.extend(["-G", "MinGW Makefiles"])
    subprocess.run(configure, check=True)
    subprocess.run(
        [
            cmake,
            "--build",
            str(assimp_build),
            "--config",
            "Debug" if mode == "debug" else "Release",
            "--target",
            "assimp",
            "--parallel",
            str(GetOption("num_jobs") or 1),
        ],
        check=True,
    )

    library = Path(str(target[0]))
    if not library.exists():
        candidates = [
            path
            for path in assimp_libdir.rglob("*")
            if path.is_file()
            and path.suffix.lower() in (".a", ".lib")
            and path.name.lower().startswith(("libassimp", "assimp"))
        ]
        if len(candidates) != 1:
            raise RuntimeError(
                f"Expected one Assimp static library in {assimp_libdir}, found {len(candidates)}"
            )
        library.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(candidates[0], library)


assimp_library_node = env.Command(
    str(assimp_library),
    [str(assimp_src / "CMakeLists.txt")],
    Action(build_assimp, "Building Assimp"),
)
assimp_libraries = [assimp_library_node[0]]
if sys.platform == "win32":
    zlib_library = "zlibstatic.lib" if toolchain == "msvc" else "libzlibstatic.a"
    assimp_libraries.append(str(assimp_libdir / zlib_library))
    if toolchain == "mingw":
        assimp_libraries.append("winpthread")
else:
    assimp_libraries.append("z")
env.Append(LIBPATH=[str(assimp_libdir)], LIBS=assimp_libraries)


def generate_glad(target, source, env):
    # REPRODUCIBLE uses the XML bundled in the pinned GLAD checkout, offline.
    subprocess.run(
        [
            sys.executable,
            "-m",
            "glad",
            "--api",
            "gl:core=3.3",
            "--out-path",
            str((build / "generated").resolve()),
            "--reproducible",
            "c",
        ],
        cwd=deps / "glad-src",
        check=True,
    )


# Declare generated headers as outputs so parallel compilation waits for the loader.
generated = env.Command(
    [
        str(build / "generated/src/gl.c"),
        str(build / "generated/include/glad/gl.h"),
        str(build / "generated/include/KHR/khrplatform.h"),
    ],
    [
        str(p)
        for p in (deps / "glad-src/glad").rglob("*")
        if p.is_file() and p.suffix in (".py", ".xml", ".jinja2")
    ],
    Action(generate_glad, "Generating OpenGL 3.3 loader"),
)


def objects(sources, folder, environment=env):
    # Explicit targets keep compiler output out of src/ and third-party checkouts.
    result = []
    for source in sources:
        target = build / "obj" / folder / Path(str(source)).stem
        nodes = environment.Object(target=str(target), source=str(source))
        environment.Depends(nodes, [generated, assimp_library_node])
        result.extend(nodes)
    return result


support = objects([generated[0]], "glad")
support += objects([deps / "tinyobjloader-src/tiny_obj_loader.cc"], "tinyobjloader")
imgui_sources = [
    deps / "imgui-src" / source
    for source in (
        "imgui.cpp",
        "imgui_demo.cpp",
        "imgui_draw.cpp",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
        "backends/imgui_impl_glfw.cpp",
        "backends/imgui_impl_opengl3.cpp",
    )
]
imgui_objects = objects(imgui_sources, "imgui")
if sys.platform == "win32":
    glfw = env.Clone()
    # GLFW's own sources reject application header-option macros like INCLUDE_NONE.
    glfw.Replace(CPPDEFINES=[])
    if glfw.get("CXX") != "cl":
        glfw.Replace(CCFLAGS=["-g", "-O0"] if mode == "debug" else ["-O2", "-DNDEBUG"])
    glfw.Append(
        CPPDEFINES=["_GLFW_WIN32", "UNICODE", "_UNICODE", ("WINVER", "0x0501")],
        CPPPATH=[str(deps / "glfw-src/src")],
    )
    names = """context init input monitor platform vulkan window egl_context osmesa_context
               null_init null_monitor null_window null_joystick win32_module win32_time
               win32_thread win32_init win32_joystick win32_monitor win32_window wgl_context""".split()
    support += objects([deps / "glfw-src/src" / (name + ".c") for name in names], "glfw", glfw)
    env.Append(LIBS=["gdi32", "user32", "shell32"])
else:
    # Use the platform-packaged GLFW rather than duplicating its platform configuration.
    if not GetOption("clean"):
        install_hint = (
            "sudo apt update && sudo apt install build-essential pkg-config libglfw3-dev"
            if sys.platform.startswith("linux")
            else "Install pkg-config and GLFW development files; see README.md."
        )
        if not env.WhereIs("pkg-config"):
            print("Missing system build dependency: pkg-config.\n" + install_hint)
            Exit(1)
        try:
            env.ParseConfig("pkg-config --cflags --libs glfw3")
        except OSError:
            print(
                "Cannot find GLFW development files through pkg-config.\n"
                + install_hint
                + "\nFor a custom GLFW installation, set PKG_CONFIG_PATH to its pkgconfig directory."
            )
            Exit(1)
    if sys.platform.startswith("linux"):
        env.Append(LIBS=["dl", "pthread"])

sources = [Path("src/main.cpp")] + sorted(Path("src/renderer").glob("*.cpp"))
program = env.Program("watercolor", objects(sources, "renderer") + imgui_objects + support)
env.Depends(program, assimp_library_node)
# The root executable loads shaders/ and assets/ directly from the repository.
Default(program)

# Tests use CPU helpers without creating a window or rendering a frame.
check_sources = [
    "tests/renderer_checks.cpp",
    "src/renderer/Camera.cpp",
    "src/renderer/Image_Output.cpp",
    "src/renderer/SceneImporter.cpp",
    "src/renderer/TextureGenerator.cpp",
    "src/renderer/PigmentLookup.cpp",
    "src/renderer/KubelkaMunk.cpp",
    "src/renderer/KubelkaMunkMeasured.cpp",
    "src/renderer/WatercolorSim.cpp",
]
checks = env.Program(str(build / "renderer_checks"), objects(check_sources, "checks") + support)
env.Depends(checks, assimp_library_node)


def run_checks(target, source, env):
    subprocess.run([str(Path(str(checks[0])).resolve())], cwd=build, check=True)


check = Alias(
    "check",
    checks,
    Action(run_checks, "Checking scene import, renderer, LIC, and pigment lookup helpers"),
)
# Rerun assertions even if the test executable itself did not need recompiling.
AlwaysBuild(check)
