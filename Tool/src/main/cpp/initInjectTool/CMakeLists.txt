cmake_minimum_required(VERSION 3.22.1)
project (initInjectTool)
# The version number.
set (Tutorial_VERSION_MAJOR 1)
set (Tutorial_VERSION_MINOR 0)

# configure a header file to pass some of the CMake settings
# to the source code

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h

# add the executable
add_executable(initInjectTool InjectTool.cpp elf_symbol_resolver.cpp)

