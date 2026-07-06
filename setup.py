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

        # FIXED FOR WINDOWS MULTI-CONFIG SUBFOLDERS:
        # Instead of using CMAKE_LIBRARY_OUTPUT_DIRECTORY directly (which appends /Release/ on MSVC),
        # we explicitly set the config-specific paths to point directly to ext_output_path.parent.
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_output_path.parent}",
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE={ext_output_path.parent}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
            f"-Dpybind11_DIR={pybind11.get_cmake_dir()}",
            "-DBUILD_PYTHON_BINDING=ON",
        ]

        if os.environ.get("CIBUILDWHEEL") == "1":
            for lib in ["indicators", "nlohmann_json", "spdlog", "fmt"]:
                cmake_args.append(f"-D{lib}_FOUND=FALSE")

        # ==================== UPDATED WINDOWS WHEEL FIX ====================
        # Enforce both regular and Release-specific runtime output paths directly to the parent folder.
        if platform.system() == "Windows":
            cmake_args.append(f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY={ext_output_path.parent}")
            cmake_args.append(f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE={ext_output_path.parent}")
            cmake_args.append("-DCMAKE_SHARED_MODULE_SUFFIX=.pyd")
        # ====================================================================

        if platform.system() == "Darwin":
            deployment_target = os.environ.get("MACOSX_DEPLOYMENT_TARGET", "11.0")
            cmake_args.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment_target}")

        if os.environ.get("CI") == "true" or os.environ.get("CIBUILDWHEEL") == "1":
            print("=== Running on CI/GitHub Actions: enabling strict Homebrew isolation ===")
            cmake_args.append("-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local")
            cmake_args.append("-DCMAKE_FIND_FRAMEWORK=NEVER")
            cmake_args.append("-DCMAKE_FIND_APPBUNDLE=NEVER")
            cmake_args.append("-Dzstd_RESOLVED=FALSE")
            cmake_args.append("-Dpcre2_RESOLVED=FALSE")

            archs = os.environ.get("ARCHFLAGS", "")
            if "x86_64" in archs:
                cmake_args.append("-DCMAKE_OSX_ARCHITECTURES=x86_64")
            elif "arm64" in archs:
                cmake_args.append("-DCMAKE_OSX_ARCHITECTURES=arm64")

        build_args = ["--build", ".", "--config", "Release"]
        if platform.system() == "Windows":
            build_args += ["--", "/m"]
        else:
            build_args += ["-j{}".format(os.cpu_count() or 4)]

        subprocess.run(["cmake", ext.sourcedir] + cmake_args, cwd=build_dir, check=True)
        subprocess.run(["cmake"] + build_args, cwd=build_dir, check=True)


setup(
    ext_modules=[CMakeExt("fstd._native")],
    cmdclass={"build_ext": CMakeBuild},
    packages=["fstd"],
    zip_safe=False,
)
