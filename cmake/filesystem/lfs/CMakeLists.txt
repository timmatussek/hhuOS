cmake_minimum_required(VERSION 3.7)
 
target_sources(filesystem PUBLIC
        ${HHUOS_SRC_DIR}/filesystem/lfs/LfsDriver.cpp
        ${HHUOS_SRC_DIR}/filesystem/lfs/LfsNode.cpp)
