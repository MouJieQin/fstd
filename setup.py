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

        # 1. Force FetchContent on Linux/CI to avoid "missing header" errors
        # If we are running in CIBUILDWHEEL, assume system libs are missing/broken
        if os.environ.get("CIBUILDWHEEL") == "1":
            for lib in ["indicators", "nlohmann_json", "spdlog", "fmt"]:
                cmake_args.append(f"-D{lib}_FOUND=FALSE")

        # ==================== CRITICAL WINDOWS WHEEL FIX ====================
        # On Windows, CMake treats runtime outputs (.dll/.exe) separately from library outputs (.lib).
        # 1. We force the runtime destination folder to match setuptools' expectations.
        # 2. We change the output binary file extension to '.pyd' so Python can import it natively.
        if platform.system() == "Windows":
            cmake_args.append(f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY={ext_output_path.parent}")
            cmake_args.append("-DCMAKE_SHARED_MODULE_SUFFIX=.pyd")
        # ====================================================================

        # In setup.py, update your Darwin configuration block to match this implementation:
        if platform.system() == "Darwin":
            # Dynamically inherit deployment targets directly from the active runtime engine
            deployment_target = os.environ.get("MACOSX_DEPLOYMENT_TARGET", "11.0")
            cmake_args.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment_target}")

        if os.environ.get("CI") == "true" or os.environ.get("CIBUILDWHEEL") == "1":
            # Keep Homebrew/System path isolation strictly enforced
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
