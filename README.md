# Gnuradio Companion Executable

This mini projects builds an executable called gnuradio-companion.py.
Its goal is to aid the GNU Radio support for the PothosSDR installer.

* https://github.com/pothosware/PothosSDR/wiki
* https://github.com/pothosware/PothosSDR/wiki/GNURadio

## The Gnuradio Companion executable

The executable has several purposes:

* provide an executable with an icon for the start menu
* locate the python 2.7 installation path in the registry
* call gnuradio-companion.py with the python 2.7 executable
* call GnuradioHelper.py upon failure to fix setup issues

## The Gnuradio Helper script

The GnuradioHelper.py script is capable of sanity checking an install
and automatically installs missing python modules and the gtk+ runtime.
It also sets environment variables PATH, PYTHONPATH, and GRC_BLOCKS_PATH:
However, normally the installer will setup these paths automatically.
