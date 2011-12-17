from distutils.core import setup, Extension

src = ["mdb/vm.c", "mdb/py_region.c", "mdb/py_task.c", "mdb/py_main.c", "mdb/util.c"];

setup(name="mdb",
      version="0.1",
      description="Mach Debugger",
      author="Peter Le Bek",
      packages=["mdb"],
      ext_modules=[Extension("mdb.core", src)])
