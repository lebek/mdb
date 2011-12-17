from distutils.core import setup, Extension
from glob import glob

setup(name="mdb",
      version="0.1",
      description="Mach Debugger",
      author="Peter Le Bek",
      packages=["mdb"],
      ext_modules=[Extension("mdb.kern", glob("mdb/kern/*.c"))])
