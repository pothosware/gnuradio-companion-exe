# Copyright (c) 2015-2016 Josh Blum
# SPDX-License-Identifier: BSL-1.0

########################################################################
## Do checks and prepare dependencies for GRC
########################################################################
import os
import sys
import inspect
import tempfile
import subprocess
from ctypes.util import find_library

########################################################################
## Registry/Environment helpers
########################################################################
import ctypes
from ctypes.wintypes import HWND, UINT, WPARAM, LPARAM, LPVOID
LRESULT = LPARAM
import os
import sys
try:
    import winreg
    unicode = str
except ImportError:
    import _winreg as winreg  # Python 2.x

class Environment(object):
    #path = r'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
    #hklm = winreg.ConnectRegistry(None, winreg.HKEY_LOCAL_MACHINE)
    path = r'Environment'
    hklm = winreg.ConnectRegistry(None, winreg.HKEY_CURRENT_USER)
    key = winreg.OpenKey(hklm, path, 0, winreg.KEY_READ | winreg.KEY_WRITE)
    SendMessage = ctypes.windll.user32.SendMessageW
    SendMessage.argtypes = HWND, UINT, WPARAM, LPVOID
    SendMessage.restype = LRESULT
    HWND_BROADCAST = 0xFFFF
    WM_SETTINGCHANGE = 0x1A
    NO_DEFAULT = type('NO_DEFAULT', (object,), {})()

    def get(self, name, default=NO_DEFAULT):
        try:
            value = winreg.QueryValueEx(self.key, name)[0]
        except WindowsError:
            if default is self.NO_DEFAULT:
                raise ValueError("No such registry key", name)
            value = default
        return value

    def set(self, name, value):
        if value:
            winreg.SetValueEx(self.key, name, 0, winreg.REG_EXPAND_SZ, value)
        else:
            winreg.DeleteValue(self.key, name)
        self.notify()

    def notify(self):
        self.SendMessage(
            self.HWND_BROADCAST, self.WM_SETTINGCHANGE, 0, u'Environment')

########################################################################
## determine-if-an-executable-or-library-is-32-or-64-bits-on-windows
## https://stackoverflow.com/questions/1345632
########################################################################
import struct

IMAGE_FILE_MACHINE_I386=332
IMAGE_FILE_MACHINE_IA64=512
IMAGE_FILE_MACHINE_AMD64=34404

def getDllMachineType(path):
    f=open(path, "rb")
    s=f.read(2)
    if s!="MZ": raise Exception("%s is not a DLL"%path)
    f.seek(60)
    s=f.read(4)
    header_offset=struct.unpack("<L", s)[0]
    f.seek(header_offset+4)
    s=f.read(2)
    machine=struct.unpack("<H", s)[0]
    f.close()
    return machine

########################################################################
## Pip helpers
########################################################################
PIP_EXE = os.path.join(os.path.dirname(sys.executable), 'Scripts', 'pip.exe')

def pip_install(arg):
    ret = subprocess.call([PIP_EXE, 'install', arg], shell=True)
    if ret != 0:
        print("Error: pip failed to install %s"%arg)
        return -1

########################################################################
## Python checks
########################################################################
def check_python_version():
    is_64bits = sys.maxsize > 2**32
    if not is_64bits:
        raise Exception("requires 64-bit Python")

    if sys.version_info.major != 2 or sys.version_info.minor != 7:
        raise Exception("requires Python version 2.7")

    if not os.path.exists(PIP_EXE):
        raise Exception("can't find pip executable %s"%PIP_EXE)

    return sys.version

def handle_python_version():
    print("Error: Invoke/Reinstall Python2.7 for amd64")
    return -1

########################################################################
## GTK checks
########################################################################
def check_gtk_runtime():

    gtk_dll_name = "libgtk-win32-2.0-0.dll"

    #first check that the installer default is found
    installer_default = os.path.join("C:\\Program Files\\GTK2-Runtime Win64\\bin", gtk_dll_name)
    if os.path.exists(installer_default): return installer_default

    #regular dll search within the path
    libgtk = find_library(gtk_dll_name)
    if libgtk is None:
        raise Exception("failed to locate the GTK+ runtime DLL")

    #reject 32-bit versions of this dll
    if getDllMachineType(libgtk) != IMAGE_FILE_MACHINE_AMD64:
        raise Exception("%s is not AMD64"%libgtk)

    return libgtk

def handle_gtk_runtime():

    GTK_URL = 'http://downloads.myriadrf.org/binaries/python27_amd64/gtk2-runtime-2.22.1-2014-02-01-ts-win64.exe'
    GTK_EXE = os.path.join(tempfile.gettempdir(), 'gtk2-runtime-2.22.1-2014-02-01-ts-win64.exe')

    if not os.path.exists(GTK_EXE):

        #need requests to download the exe
        try: import requests
        except: pip_install("requests")
        import requests

        #download from the url to the destination
        r = requests.get(GTK_URL)
        with open(GTK_EXE, 'wb') as fd:
            for chunk in r.iter_content(1024*1024):
                fd.write(chunk)

    if not os.path.exists(GTK_EXE):
        print("Cant find installer: %s"%GTK_EXE)
        print("Failed to download: %s"%GTK_URL)
        return -1

    print("Running installer: %s"%GTK_EXE)
    ret = subprocess.call([GTK_EXE, '/S'], shell=True) #silent install
    if ret != 0:
        print("The GTK installer failed with exit code %d"%ret)
        exit(ret)

    print("The GTK installer should have modified the system path")
    print("Open a new command window and re-run this script...")

def check_import_gtk():
    import gtk
    return inspect.getfile(gtk)

def handle_import_gtk():
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/pygtk-2.22.0-cp27-none-win_amd64.whl')
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/pygobject-2.28.6-cp27-none-win_amd64.whl')
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/pycairo_gtk-1.10.0-cp27-none-win_amd64.whl')

########################################################################
## GNU Radio checks
########################################################################
def guess_bin_path():

    #was it run from the proper install directory?
    path = os.path.abspath(os.path.dirname(__file__))
    if os.path.exists(os.path.join(path, "gnuradio-runtime.dll")): return path

    #otherwise search the path to find the root
    gnuradio_runtime = find_library("gnuradio-runtime.dll")
    if gnuradio_runtime: return gnuradio_runtime

def check_gr_runtime():
    gnuradio_runtime = find_library("gnuradio-runtime.dll")

    if gnuradio_runtime is None:
        raise Exception("failed to locate the GNURadio runtime DLL")

    return gnuradio_runtime

def handle_gr_runtime():

    path = guess_bin_path()

    #we dont know where the bin path is, this is probably an installer issue
    #print this message and return error so other handlers are not invoked
    if path is None:
        print("Error: PothosSDR DLLs missing from the system path")
        print("  See instructions to 'Add PothosSDR to the system PATH'")
        print("  https://github.com/pothosware/PothosSDR/wiki/Tutorial")
        return -1

    e = Environment()
    PATH = e.get('PATH', '')
    print("Current PATH: '%s'"%PATH)
    if not PATH: PATH = list()
    else: PATH = PATH.split(';')

    if path not in PATH:
        print("Adding %s to the PATH"%path)
        PATH.append(path)
        e.set('PATH', ';'.join(PATH))

    print("")
    print("The PATH for the current user has been modified")
    print("Open a new command window and re-run this script...")

def check_import_gr():
    import gnuradio
    from gnuradio import gr
    return inspect.getfile(gnuradio)

def handle_import_gr():
    binDir = guess_bin_path()
    path = os.path.join(os.path.dirname(binDir), 'lib', 'python2.7', 'site-packages')
    if not os.path.exists(path): #or use old-style path without python version
        path = os.path.join(os.path.dirname(binDir), 'lib', 'site-packages')
    path = os.path.normpath(path)
    print("Error: GNURadio modules missing from PYTHONPATH")

    print("")
    print("Current search path:")
    for searchPath in sys.path: print("  * %s"%searchPath)
    print("")

    e = Environment()
    PYTHONPATH = e.get('PYTHONPATH', '')
    print("Current PYTHONPATH: '%s'"%PYTHONPATH)
    if not PYTHONPATH: PYTHONPATH = list()
    else: PYTHONPATH = PYTHONPATH.split(';')

    if path not in PYTHONPATH:
        print("Adding %s to the PYTHONPATH"%path)
        PYTHONPATH.append(path)
        e.set('PYTHONPATH', ';'.join(PYTHONPATH))

    print("")
    print("The PYTHONPATH for the current user has been modified")
    print("Open a new command window and re-run this script...")

def check_grc_blocks_path():
    GRC_BLOCKS_PATH = os.environ.get('GRC_BLOCKS_PATH', '')
    if not GRC_BLOCKS_PATH:
        raise Exception("GRC_BLOCKS_PATH is not set")
    if not os.path.exists(GRC_BLOCKS_PATH):
        raise Exception("GRC_BLOCKS_PATH '%s' does not exist"%GRC_BLOCKS_PATH)
    if not os.path.exists(os.path.join(GRC_BLOCKS_PATH, 'options.xml')):
        raise Exception("GRC_BLOCKS_PATH '%s' does not contain options.xml"%GRC_BLOCKS_PATH)
    return GRC_BLOCKS_PATH

def handle_grc_blocks_path():
    binDir = guess_bin_path()
    path = os.path.join(os.path.dirname(binDir), 'share', 'gnuradio', 'grc', 'blocks')
    path = os.path.normpath(path)

    print("Setting the GRC_BLOCKS_PATH to %s"%path)
    e = Environment()
    e.set('GRC_BLOCKS_PATH', path)

    print("")
    print("The GRC_BLOCKS_PATH for the current user has been modified")
    print("Open a new command window and re-run this script...")

########################################################################
## Other module checks
########################################################################
def check_import_numpy():
    import numpy
    return inspect.getfile(numpy)

def handle_import_numpy():
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/numpy-1.12.0+mkl-cp27-cp27m-win_amd64.whl')

def check_import_lxml():
    import lxml
    return inspect.getfile(lxml)

def handle_import_lxml():
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/lxml-3.7.2-cp27-cp27m-win_amd64.whl')

def check_import_cheetah():
    import Cheetah
    return inspect.getfile(Cheetah)

def handle_import_cheetah():
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/Cheetah-2.4.4-cp27-none-win_amd64.whl')

def check_import_wxpython():
    import wx
    import wx.glcanvas
    return inspect.getfile(wx)

def handle_import_wxpython():
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/wxPython-3.0.2.0-cp27-none-win_amd64.whl')
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/wxPython_common-3.0.2.0-py2-none-any.whl')

def check_import_pyopengl():
    import OpenGL
    import OpenGL.GL
    return inspect.getfile(OpenGL)

def handle_import_pyopengl():
    print("Installing PyOpenGL with pip:")
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/PyOpenGL-3.1.1-cp27-cp27m-win_amd64.whl')
    pip_install('http://downloads.myriadrf.org/binaries/python27_amd64/PyOpenGL_accelerate-3.1.1-cp27-cp27m-win_amd64.whl')
    print("  Done!")

CHECKS = [
    #first check gr runtime so we can locate the install based on runtime dll in PATH
    ("GR_RUNTIME",     'locate GNURadio runtime', check_gr_runtime, handle_gr_runtime),

    #gtk runtime is similar check for dlls in the seatch PATH (no python required)
    ("GTK_RUNTIME",    'locate GTK+ runtime',     check_gtk_runtime, handle_gtk_runtime),

    #basic python environment and import checks and using pip to install from a URL
    ("PYVERSION",      'Python version is 2.7',   check_python_version, handle_python_version),
    ("IMPORT_GTK",     'import gtk module',       check_import_gtk, handle_import_gtk),
    ("IMPORT_NUMPY",   'import numpy module',     check_import_numpy, handle_import_numpy),
    ("IMPORT_LXML",    'import lxml module',      check_import_lxml, handle_import_lxml),
    ("IMPORT_CHEETAH", 'import Cheetah module',   check_import_cheetah, handle_import_cheetah),
    ("IMPORT_WX",      'import wx module',        check_import_wxpython, handle_import_wxpython),
    ("IMPORT_OPENGL",  'import OpenGL module',    check_import_pyopengl, handle_import_pyopengl),

    #final checks for GNU Radio and GRC that set local environment variables
    ("GRC_BLOCKS",     'GRC blocks path set',     check_grc_blocks_path, handle_grc_blocks_path),
    ("IMPORT_GR",      'import GNURadio module',  check_import_gr, handle_import_gr),
]

def main():
    print("")
    print("="*40)
    print("== Runtime and import checks")
    print("="*40)

    maxLen = max([len(c[1]) for c in CHECKS])
    msgs = dict()
    statuses = dict()
    numFails = 0
    numPasses = 0
    for key, what, check, handle in CHECKS:
        whatStr = "%s...%s"%(what, ' '*(maxLen-len(what)))
        try:
            msg = check()
            statStr = "PASS"
            checkPassed = True
            numPasses += 1
        except Exception as ex:
            statStr = "FAIL"
            checkPassed = False
            msg = str(ex)
            numFails += 1

        print(" * Check %s  %s"%(whatStr, statStr))
        msgs[key] = msg
        statuses[key] = checkPassed

    if numPasses:
        print("")
        print("="*40)
        print("== Checks passed summary")
        print("="*40)
        for key, what, check, handle in CHECKS:
            if statuses[key]: print("%s:\t%s"%(key, msgs[key]))

    if numFails == 0:
        print("")
        print("All checked passed! gnuradio-companion is ready to use.")
        return 0

    if numFails:
        print("")
        print("="*40)
        print("== Checks failed summary")
        print("="*40)
        for key, what, check, handle in CHECKS:
            if not statuses[key]: print("%s:\t%s"%(key, msgs[key]))

    if numFails:
        print("")
        print("="*40)
        print("== Fixing problems")
        print("="*40)
        for key, what, check, handle in CHECKS:
            if not statuses[key]:
                print("Handling issues for %s..."%key)
                ret = handle()
                #exit asap when return code provided
                if ret is not None: return ret

    print("")
    print("Changes made! Please re-run this script in a new terminal.")

if __name__ == '__main__':

    #run main with exception handling
    ret = None
    try: ret = main()
    except Exception as ex:
        print("Error: %s"%str(ex))

    #give time to read message if opened from explorer
    #wait for user to press a key
    print("")
    os.system('pause')

    #return the error code from main
    if ret is None: ret = 0
    exit(ret)
