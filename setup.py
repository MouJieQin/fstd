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
        ]
        cmake_args.append("-DBUILD_PYTHON_BINDING=ON")

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
    name="fstd",
    version="0.1.0",
    author="Moujie Qin",
    description="FSTD high-performance dictionary engine Python binding",
    long_description=Path("README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    ext_modules=[CMakeExt("fstd._native")],
    cmdclass={"build_ext": CMakeBuild},
    packages=["fstd"],
    zip_safe=False,
    python_requires=">=3.9",
)
