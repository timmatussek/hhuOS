
#include "LfsNode.h"

String LfsNode::getName()
{
    return "";
}

uint8_t LfsNode::getFileType()
{
    return 0;
}

uint64_t LfsNode::getLength()
{
    return 0;
}

Util::Array<String> LfsNode::getChildren()
{
    return Util::Array<String>(0);
}

uint64_t LfsNode::readData(char *buf, uint64_t pos, uint64_t numBytes)
{
    return 0;
}

uint64_t LfsNode::writeData(char *buf, uint64_t pos, uint64_t length)
{
    return 0;
}