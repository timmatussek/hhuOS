# Copyright (C) 2018-2021 Heinrich-Heine-Universitaet Duesseldorf,
# Institute of Computer Science, Department Operating Systems
# Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
#
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
# warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>

cmake_minimum_required(VERSION 3.7)

project(platform-x86)
message(STATUS "Project " ${PROJECT_NAME})

set(X86_ROOT_DIR "${HHUOS_ROOT_DIR}/platform/x86")
set(X86_SRC_DIR "${HHUOS_ROOT_DIR}/platform/x86/src")

# Set compiler flags
ENABLE_LANGUAGE(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT "elf32")
set(CMAKE_ASM_NASM_INCLUDES "${X86_SRC_DIR}")
set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -I${CMAKE_ASM_NASM_INCLUDES} -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -mno-sse -march=i486")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -mno-sse -march=i486" PARENT_SCOPE)
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -T ${X86_ROOT_DIR}/link.ld")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS} -T ${X86_ROOT_DIR}/link.ld" PARENT_SCOPE)

# Add platform specific subdirectories
add_subdirectory(bios)
add_subdirectory(device)
add_subdirectory(kernel)