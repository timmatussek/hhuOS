#ifndef __LfsNode_include__
#define __LfsNode_include__

#include "filesystem/core/FsNode.h"

class LfsNode : public FsNode
{
public:
    /**
     * Constructor.
     */
    LfsNode() = default;

    /**
     * Destructor.
     */
    ~LfsNode() override = default;

    /**
     * Overriding virtual function from FsNode.
     */
    String getName() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint8_t getFileType() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t getLength() override;

    /**
     * Overriding virtual function from FsNode.
     */
    Util::Array<String> getChildren() override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t readData(char *buf, uint64_t pos, uint64_t numBytes) override;

    /**
     * Overriding virtual function from FsNode.
     */
    uint64_t writeData(char *buf, uint64_t pos, uint64_t length) override;
};

#endif