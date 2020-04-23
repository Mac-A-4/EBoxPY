from distutils.core import setup, Extension

setup(name="EBoxPY", version="1.0", ext_modules=[Extension("EBoxPY", ["EBoxPY.c"])])