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
#ifndef __Lfs_include__
#define __Lfs_include__

#include "filesystem/core/FsDriver.h"

class Lfs
{
private:
    StorageDevice *device = nullptr;

    /**
     * Initilize caches with data from disk
     * 
     * @return false if disk does not conatin a valid lfs
     */
    bool readLfsFromDevice();

public:
    /**
     * Constructor.
     */
    Lfs(StorageDevice *device);

    /**
     * Destructor.
     */
    ~Lfs();

    /**
     * Resets the current lfs to be a fresh empty one.
     */
    void reset();

    /**
     * Forces all caches to be written to disk.
     * 
     * @return true if successful
     */
    bool flush();

    /**
     * Read bytes from the file's data.
     * If (pos + numBytes) is greater than the data's length, END_OF_FILE shall be appended.
     * If pos is greater than the data's length, END_OF_FILE shall be the first and only character,
     * that is written to buf.
     *
     * @param path The path to a file
     * @param buf The buffer to write to (Needs to be allocated already!)
     * @param pos The offset
     * @param numBytes The amount of bytes to read
     * 
     * @return The amount of actually read bytes
     */
    uint64_t readData(const String &path, char *buf, uint64_t pos, uint64_t numBytes);

    /**
     * Write bytes to the file's data. If the offset points right into the existing data,
     * it shall be overwritten with the new data. If the new data does not fit, the data size shall be increased.
     * 
     * @param path The path to a file
     * @param buf The data to write
     * @param pos The offset
     * @param numBytes The amount of bytes to write
     * 
     * @return The amount of actually written bytes
     */
    uint64_t writeData(const String &path, char *buf, uint64_t pos, uint64_t length);

    /**
     * Create a new empty file or directory at a given path.
     * The parent-directory of the new file must exist beforehand.
     * 
     * @param path The path to a file
     * @param fileType The filetype
     * 
     * @return true on success.
     */
    bool createNode(const String &path, uint8_t fileType);

    /**
     * Delete an existing file or directory at a given path.
     * The file must be a regular file or an empty folder (a leaf in the filesystem tree).
     * 
     * @param path The path to a file
     * 
     * @return true on success.
     */
    bool deleteNode(const String &path);

    /**
     * Returns the filetype.
     * 
     * @param path The path to a file
     */
    uint8_t getFileType(const String &path);

    /**
     * Get the length (in bytes) of the file's data.
     * If the node is a directory, this should always return 0.
     * 
     * @param path The path to a file
     */
    uint64_t getLength(const String &path);

    /**
     * Get the files's children.
     * 
     * @param path The path to a file
     */
    Util::Array<String> getChildren(const String &path);
};

#endif