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

        # FIXED FOR LINUX & MACOS ISOLATION:
        # Intercept the environment override settings passed by cibuildwheel's environment layer
        # and translate them into explicit command-line configuration arguments for CMake.
        for fallback_lib in ["indicators", "nlohmann_json", "spdlog", "fmt"]:
            if os.environ.get(f"{fallback_lib}_FOUND") == "FALSE":
                cmake_args.append(f"-D{fallback_lib}_FOUND=FALSE")

        if platform.system() == "Darwin":
            # Force a modern deployment target to make std::filesystem available
            cmake_args.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15")
            # If cibuildwheel is injecting cross-compile flags, forward them to CMake
            archs = os.environ.get("ARCHFLAGS", "")
            if "x86_64" in archs:
                cmake_args.append("-DCMAKE_OSX_ARCHITECTURES=x86_64")
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
