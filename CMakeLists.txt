########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(gnuradio-companion-exe CXX)

########################################################################
# Resource file - adds an icon to gnuradio-companion executable
########################################################################
set(ICON_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/grc-icon-256.ico)
set(RES_FILES "${CMAKE_CURRENT_BINARY_DIR}/gnuradio-companion.rc")
file(WRITE "${RES_FILES}" "id ICON \"${ICON_SOURCE}\"")
set(CMAKE_RC_COMPILER_INIT windres)
enable_language(RC)
set(CMAKE_RC_COMPILE_OBJECT
    "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
set(PYTHON_VERSION "3.9" CACHE STRING "Python version to support")
string(REPLACE "." "" PYVER_NO_DOTS "${PYTHON_VERSION}")

########################################################################
# Create gnuradio-companion executable
########################################################################
set(CMAKE_EXE_LINKER_FLAGS "/entry:mainCRTStartup ${CMAKE_EXE_LINKER_FLAGS}")
add_executable(gnuradio-companion WIN32 gnuradio-companion.cpp ${RES_FILES})
target_compile_definitions(gnuradio-companion PRIVATE -DPYTHON_VERSION=\"${PYTHON_VERSION}\")
target_compile_definitions(gnuradio-companion PRIVATE -DPYVER_NO_DOTS=\"${PYVER_NO_DOTS}\")

########################################################################
# Install gnuradio-companion executable
########################################################################
install(
    TARGETS gnuradio-companion
    RUNTIME DESTINATION bin)

########################################################################
# Install gnuradio helper script
########################################################################
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/GNURadioHelper.py
    ${CMAKE_CURRENT_BINARY_DIR}/GNURadioHelper.py
    @ONLY)
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/GNURadioHelper.py
    DESTINATION bin)
