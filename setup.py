import os
import sys
import platform
import subprocess
from pathlib import Path
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

ROOT = Path(__file__).parent


class CMakeExt(Extension):
    def __init__(self, name: str):
        super().__init__(name, sources=[])
        self.sourcedir = str(ROOT)


class CMakeBuild(build_ext):
    def run(self):
        try:
            subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError("CMake 3.24+ required, please install cmake")
        for ext in self.extensions:
            self.build_one_ext(ext)

    def build_one_ext(self, ext: CMakeExt):
        ext_output_path = Path(self.get_ext_fullpath(ext.name)).absolute()
        build_dir = Path(self.build_temp) / ext.name
        build_dir.mkdir(parents=True, exist_ok=True)
        import pybind11

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_output_path.parent}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
            f"-Dpybind11_DIR={pybind11.get_cmake_dir()}",
            "-DBUILD_PYTHON_BINDING=ON",
        ]

        # 1. Translate environment variable overrides into clean CMake command-line parameters
        for fallback_lib in ["indicators", "nlohmann_json", "spdlog", "fmt"]:
            if os.environ.get(f"{fallback_lib}_FOUND") == "FALSE":
                cmake_args.append(f"-D{fallback_lib}_FOUND=FALSE")

        # 2. Capture and forward the Homebrew isolation target if present
        if "CMAKE_IGNORE_PREFIX_PATH" in os.environ:
            cmake_args.append(f"-DCMAKE_IGNORE_PREFIX_PATH={os.environ['CMAKE_IGNORE_PREFIX_PATH']}")

        if platform.system() == "Darwin":
            cmake_args.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15")

            # Cross-compile isolation configurations specifically for macOS x86_64
            archs = os.environ.get("ARCHFLAGS", "")
            if "x86_64" in archs:
                cmake_args.append("-DCMAKE_OSX_ARCHITECTURES=x86_64")
                cmake_args.append("-DCMAKE_FIND_FRAMEWORK=NEVER")
                cmake_args.append("-DCMAKE_FIND_APPBUNDLE=NEVER")
                cmake_args.append("-Dzstd_RESOLVED=FALSE")
                cmake_args.append("-Dpcre2_RESOLVED=FALSE")
            elif "arm64" in archs:
                cmake_args.append("-DCMAKE_OSX_ARCHITECTURES=arm64")

        build_args = ["--build", ".", "--config", "Release"]
        if platform.system() == "Windows":
            cmake_args.append("-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON")
            build_args += ["--", "/m"]
        else:
            build_args += ["-j{}".format(os.cpu_count() or 4)]

        # cmake configure
        subprocess.run(
            ["cmake", ext.sourcedir] + cmake_args,
            cwd=build_dir,
            check=True
        )
        # cmake build
        subprocess.run(
            ["cmake"] + build_args,
            cwd=build_dir,
            check=True
        )


setup(
    ext_modules=[CMakeExt("fstd._native")],
    cmdclass={"build_ext": CMakeBuild},
    packages=["fstd"],
    zip_safe=False,
)
