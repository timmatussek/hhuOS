/*
 * Copyright (C) 2018-2021 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 * Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include "Structure.h"
#include "Constants.h"
#include <util/elf/ElfConstants.h>

namespace Kernel::Multiboot {

Info Structure::info;
FrameBufferInfo Structure::frameBufferInfo;

void Structure::init(Info *address) {
    info = *address;
}

void Structure::parse() {
    parseFrameBufferInfo();
}

FrameBufferInfo Structure::getFrameBufferInfo() {
    return frameBufferInfo;
}

void Structure::parseFrameBufferInfo() {

    if (info.flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        frameBufferInfo.address = static_cast<uint32_t>(info.framebufferAddress);
        frameBufferInfo.width = static_cast<uint16_t>(info.framebufferWidth);
        frameBufferInfo.height = static_cast<uint16_t>(info.framebufferHeight);
        frameBufferInfo.bpp = info.framebufferBpp;
        frameBufferInfo.pitch = static_cast<uint16_t>(info.framebufferPitch);
        frameBufferInfo.type = info.framebufferType;
    } else {
        frameBufferInfo.address = 0;
        frameBufferInfo.width = 0;
        frameBufferInfo.height = 0;
        frameBufferInfo.bpp = 0;
        frameBufferInfo.pitch = 0;
        frameBufferInfo.type = 0;
    }
}

}
