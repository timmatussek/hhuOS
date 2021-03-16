#ifndef __LfsDriver_include__
#define __LfsDriver_include__

#include "filesystem/core/FsDriver.h"

extern "C"
{
#include "lib/libc/string.h"
}

/**
 * An implementation of FsDriver for a log-structured file system.
 */
class LfsDriver : public FsDriver
{
private:
    StorageDevice *device = nullptr;

    static const constexpr char *TYPE_NAME = "LfsDriver";

public:
    PROTOTYPE_IMPLEMENT_CLONE(LfsDriver);

    /**
     * Constructor.
     */
    LfsDriver() = default;

    /**
     * Destructor.
     */
    ~LfsDriver() override = default;

    /**
     * Overriding virtual function from FsDriver.
     */
    String getTypeName() override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool createFs(StorageDevice *device) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool mount(StorageDevice *device) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    Util::SmartPointer<FsNode> getNode(const String &path) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool createNode(const String &path, uint8_t fileType) override;

    /**
     * Overriding virtual function from FsDriver.
     */
    bool deleteNode(const String &path) override;
};

#endif
