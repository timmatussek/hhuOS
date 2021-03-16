#include "LfsDriver.h"
#include "LfsNode.h"

String LfsDriver::getTypeName()
{
    return TYPE_NAME;
}

bool LfsDriver::createFs(StorageDevice *device)
{
    return false;
}

bool LfsDriver::mount(StorageDevice *device)
{
    return false;
}

Util::SmartPointer<FsNode> LfsDriver::getNode(const String &path)
{
    return Util::SmartPointer<FsNode>(new LfsNode());
}

bool LfsDriver::createNode(const String &path, uint8_t fileType)
{
    return false;
}

bool LfsDriver::deleteNode(const String &path)
{
    return false;
}
