#ifndef HHUOS_ATADEVICE_H
#define HHUOS_ATADEVICE_H

#include <devices/storage/StorageDevice.h>
#include "AtaController.h"

class AtaDevice : public StorageDevice {

public:

    AtaDevice(AtaController &controller, uint8_t driveNumber, const String &name);

    AtaDevice(const AtaDevice &copy) = delete;

    ~AtaDevice() override = default;

    /**
     * Overriding function from StorageDevice.
     */
    String getHardwareName() override;

    /**
     * Overriding function from StorageDevice.
     */
    uint32_t getSectorSize() override;

    /**
     * Overriding function from StorageDevice.
     */
    uint64_t getSectorCount() override;

protected:

    virtual bool identify(uint8_t identifyCommand);

    bool checkDoubleWordIO(uint16_t *originalBuf, uint8_t identifyCommand);

    void copyStringFromIdentifyBuffer(uint8_t *dest, uint16_t *src, uint32_t words);

protected:

    Logger *log = nullptr;

    AtaController &controller;

    uint8_t driveNumber;

    bool supportsChs = false;
    bool supportsLba28 = false;
    bool supportsLba48 = false;
    bool supportsDoubleWordIO = false;

    uint64_t sectorCount = 0;
    uint32_t sectorSize = 0;

    uint8_t serialNumber[20];
    uint8_t firmareRevision[8];
    uint8_t modelNumber[40];

};

#endif
