#ifndef HHUOS_PATAPIDEVICE_H
#define HHUOS_PATAPIDEVICE_H

#include "AtaDevice.h"

class PatapiDevice : public AtaDevice {

public:

    explicit PatapiDevice(AtaController &controller, uint8_t driveNumber);

    PatapiDevice(const PatapiDevice &copy) = delete;

    ~PatapiDevice() override = default;

    static bool isValid(AtaController &controller, uint8_t driveNumber);

    /**
     * Overriding function from StorageDevice.
     */
    bool read(uint8_t *buff, uint32_t sector, uint32_t count) override;

    /**
     * Overriding function from StorageDevice.
     */
    bool write(const uint8_t *buff, uint32_t sector, uint32_t count) override;

};

#endif
