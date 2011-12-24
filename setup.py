from distutils.core import setup, Extension
from glob import glob
import os, subprocess

baseDir = os.getcwd()
os.chdir("mdb/kern")

print "compiling MiG definitions"
subprocess.call(["/usr/bin/mig", "exception.defs"])

os.chdir(baseDir)

setup(name="mdb",
      version="0.1",
      description="Mach Debugger",
      author="Peter Le Bek",
      packages=["mdb"],
      ext_modules=[Extension("mdb.kern", glob("mdb/kern/*.c"))])
