#!/usr/bin/env python3
"""Exercise Git-fetched CMake/xmake consumers using an isolated local snapshot."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
URL = "https://github.com/HK47196/NarrowCXX.git"


def run(command, cwd, env, *, succeeds=True, diagnostic=None):
    print("+ " + " ".join(map(str, command)), flush=True)
    result = subprocess.run(command, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != succeeds:
        raise RuntimeError(f"Exit status {result.returncode}:\n{result.stdout}")
    if diagnostic and diagnostic not in result.stdout:
        raise RuntimeError(f"Missing {diagnostic}:\n{result.stdout}")
    return result.stdout


def snapshot(directory, env):
    source = directory / "NarrowCXX.git"
    source.mkdir()
    for name in ("checks", "cmake", "xmake", "packages"):
        shutil.copytree(ROOT / name, source / name)
    for name in ("CMakeLists.txt", "xmake.lua", ".clang-tidy"):
        shutil.copy2(ROOT / name, source / name)
    # Fetch the current working files, including uncommitted packaging changes,
    # without modifying this checkout or contacting the public Git host.
    recipe = source / "packages/n/narrow-cpp/xmake.lua"
    recipe.write_text(recipe.read_text().replace(URL, source.as_uri()))
    run(["git", "init", "-b", "master"], source, env)
    run(["git", "add", "."], source, env)
    run(["git", "-c", "user.name=Integration test", "-c",
         "user.email=integration@example.invalid", "-c", "commit.gpgsign=false",
         "commit", "-m", "Integration snapshot"], source, env)
    revision = run(["git", "rev-parse", "HEAD"], source, env).strip()
    return source, revision


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adapter", choices=("cmake", "xmake", "xmake-checkout"), required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="narrow-integration-") as temporary:
        directory = Path(temporary)
        scratch = directory / "tmp"
        scratch.mkdir()
        env = dict(os.environ, XMAKE_GLOBALDIR=str(directory / "global"),
                   XMAKE_PKG_CACHEDIR=str(directory / "cache"),
                   XMAKE_PKG_INSTALLDIR=str(directory / "packages"),
                   XMAKE_TMPDIR=str(scratch), TMPDIR=str(scratch),
                   XMAKE_COLORTERM="nocolor", CCACHE_DISABLE="1")
        # Do not inherit an outer xmake project's configuration or local packages.
        for name in ("XMAKE_CONFIGDIR", "XMAKE_PKG_LOCALDIR"):
            env.pop(name, None)
        source, revision = snapshot(directory, env)
        consumer = directory / "consumer"
        consumer.mkdir()
        main_cpp = consumer / "main.cpp"
        main_cpp.write_text("int main() { return 0; }\n")

        if args.adapter == "cmake":
            (consumer / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.27)
project(Consumer LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(narrow_cpp GIT_REPOSITORY "{source.as_uri()}" GIT_TAG {revision})
FetchContent_MakeAvailable(narrow_cpp)
add_executable(app main.cpp)
narrow_enable_tidy(app)
''')
            run(["cmake", "-S", ".", "-B", "build", "-G", "Ninja",
                 "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"], consumer, env)
            build = ["cmake", "--build", "build", "--parallel", "2"]
        elif args.adapter == "xmake":
            # The package repository is local, while its source is fetched from
            # Git. Private mode keeps xmake from updating unrelated repositories.
            (consumer / "xmake.lua").write_text(f'''set_xmakever("3.1.1")
set_policy("network.mode", "private")
set_policy("package.precompiled", false)
set_policy("package.build.ccache", false)
add_repositories("narrow-cpp {source}")
add_requires("narrow-cpp {revision}", {{system = false}})
target("app")
    set_kind("binary")
    set_languages("cxx20")
    add_files("main.cpp")
    add_packages("narrow-cpp")
    add_rules("@narrow-cpp/tidy")
''')
            run(["xmake", "g", "--network=private"], consumer, env)
            run(["xmake", "f", "-y", "--toolchain=clang"], consumer, env)
            build = ["xmake", "build", "-j", "2"]
        else:
            (consumer / "xmake.lua").write_text(f'''set_xmakever("3.1.1")
includes("{source}")
target("app")
    set_kind("binary")
    set_languages("cxx20")
    add_files("main.cpp")
    add_rules("narrow.tidy")
''')
            run(["xmake", "f", "-y", "--toolchain=clang"], consumer, env)
            build = ["xmake", "build", "-j", "2"]

        run(build, consumer, env)
        print("PASS allowed consumer", flush=True)
        main_cpp.write_text('''#include <functional>
int main() {
    std::function<int()> callback = [] { return 0; };
    return callback();
}
''')
        run(build, consumer, env, diagnostic="narrow-restricted-library")
        print("PASS restricted warning remains visible and succeeds", flush=True)
        main_cpp.write_text('''int main() {
    int* value = new int(0);
    int result = *value;
    delete value;
    return result;
}
''')
        run(build, consumer, env, succeeds=False, diagnostic="narrow-no-direct-new-delete")
        print("PASS banned consumer fails", flush=True)
        main_cpp.write_text("int main() { return 0; }\n")
        run(build, consumer, env)
        if args.adapter != "cmake":
            cached = run(build, consumer, env)
            if "narrow.tidy main.cpp" in cached:
                raise RuntimeError("Unchanged consumer was analyzed again")
            print("PASS unchanged consumer reuses successful analysis", flush=True)
            plugin_root = directory / "packages" if args.adapter == "xmake" else consumer / "build"
            extension = ".dylib" if sys.platform == "darwin" else ".so"
            plugin = next(plugin_root.rglob("libNarrowTidy" + extension))
            plugin.write_text("invalid plugin\n")
        else:
            # Modifying a built CMake output can trigger a repair by relinking.
            # A malformed profile tests unconditional verification directly.
            profile = consumer / "build/_deps/narrow_cpp-src/.clang-tidy"
            profile.write_text("Checks: [invalid yaml\n")
        run(build, consumer, env, succeeds=False)
        print("PASS broken installation fails even on an unchanged consumer", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
