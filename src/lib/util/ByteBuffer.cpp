/*
 * Copyright (C) 2021 Burak Akguel, Christian Gesse, Fabian Ruhland, Filip Krakowski, Michael Schoettner, Tim Matussek
 * Heinrich-Heine University
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
#include "ByteBuffer.h"

namespace Util {

    uint32_t ByteBuffer::readU32(uint8_t *buffer, size_t offset) {
        return ((uint32_t)buffer[offset]) 
            | (((uint32_t)buffer[offset + 1]) << 8) 
            | (((uint32_t)buffer[offset + 2]) << 16) 
            | (((uint32_t)buffer[offset + 3]) << 24);
    }

    uint64_t ByteBuffer::readU64(uint8_t *buffer, size_t offset) {
        return ((uint64_t)buffer[offset]) 
            | (((uint64_t)buffer[offset + 1]) << 8) 
            | (((uint64_t)buffer[offset + 2]) << 16) 
            | (((uint64_t)buffer[offset + 3]) << 24) 
            | (((uint64_t)buffer[offset + 4]) << 32) 
            | (((uint64_t)buffer[offset + 5]) << 40) 
            | (((uint64_t)buffer[offset + 6]) << 48) 
            | (((uint64_t)buffer[offset + 7]) << 56);
    }

    String ByteBuffer::readString(uint8_t *buffer, size_t offset, size_t length) {
        String result;
        for(size_t i = 0; i < length; i++) {
            result += String(buffer[offset + i]);
        }
        return result;
    }

    void ByteBuffer::writeU32(uint8_t *buffer, size_t offset, uint32_t data) {
        buffer[offset] = data;
        buffer[offset + 1] = data >> 8;
        buffer[offset + 2] = data >> 16;
        buffer[offset + 3] = data >> 24;
    }

    void ByteBuffer::writeU64(uint8_t *buffer, size_t offset, uint64_t data) {
        buffer[offset] = data;
        buffer[offset + 1] = data >> 8;
        buffer[offset + 2] = data >> 16;
        buffer[offset + 3] = data >> 24;
        buffer[offset + 4] = data >> 32;
        buffer[offset + 5] = data >> 40;
        buffer[offset + 6] = data >> 48;
        buffer[offset + 7] = data >> 56;
    }

    void ByteBuffer::writeString(uint8_t *buffer, size_t offset, String string) {
        for(size_t i = 0; i < string.length(); i++) {
            buffer[offset + i] = string[i];
        }
    }

}