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
#ifndef __ByteBuffer_include__
#define __ByteBuffer_include__

#include "lib/string/String.h"

namespace Util {

    /**
     * Static helper functions to read and write various data types from a
     * uint8_t* buffer.
     */
    class ByteBuffer {
    public:
        /**
         * Read a uint8_t at offset from buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * 
         * @return uint8_t value at offset in buffer
         */
        static uint8_t readU8(uint8_t *buffer, size_t offset);

        /**
         * Read a uint32_t in little-endian at offset from buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * 
         * @return uint32_t value at offset in buffer
         */
        static uint32_t readU32(uint8_t *buffer, size_t offset);

        /**
         * Read a uint64_t in little-endian at offset from buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * 
         * @return uint64_t value at offset in buffer
         */
        static uint64_t readU64(uint8_t *buffer, size_t offset);

         /**
         * Read multiple chars at offset from buffer into a String.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * @param length The length of the String.
         * 
         * @return String of chars at offset in buffer
         */
        static String readString(uint8_t *buffer, size_t offset, size_t length);

        /**
         * Write a uint8_t at offset into buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * @param data The uint8_t to be written.
         * 
         */
        static void writeU8(uint8_t *buffer, size_t offset, uint8_t data);

        /**
         * Write a uint32_t in little-endian at offset into buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * @param data The uint32_t to be written.
         * 
         */
        static void writeU32(uint8_t *buffer, size_t offset, uint32_t data);

        /**
         * Write a uint64_t in little-endian at offset into buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * @param data The uint64_t to be written.
         * 
         */
        static void writeU64(uint8_t *buffer, size_t offset, uint64_t data);

        /**
         * Write String at offset into buffer.
         * 
         * @param buffer A buffer containing bytes.
         * @param offset An offset in bytes into buffer.
         * @param string a String.
         * 
         */
        static void writeString(uint8_t *buffer, size_t offset, String string);

    };

}

#endif