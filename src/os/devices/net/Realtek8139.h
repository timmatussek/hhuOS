#ifndef __Realtek8139_include__
#define __Realtek8139_include__


#include <kernel/interrupts/InterruptHandler.h>
#include <devices/Pci.h>

class Realtek8139 : InterruptHandler {

    struct Registers {
        uint8_t     IDR0;
        uint8_t     IDR1;
        uint8_t     IDR2;
        uint8_t     IDR3;
        uint8_t     IDR4;
        uint8_t     IDR5;
        uint16_t    reserved0;
        uint8_t     MAR0;
        uint8_t     MAR1;
        uint8_t     MAR2;
        uint8_t     MAR3;
        uint8_t     MAR4;
        uint8_t     MAR5;
        uint8_t     MAR6;
        uint8_t     MAR7;
        uint32_t    TSD0;
        uint32_t    TSD1;
        uint32_t    TSD2;
        uint32_t    TSD3;
        uint32_t    TSAD0;
        uint32_t    TSAD1;
        uint32_t    TSAD2;
        uint32_t    TSAD3;
        uint32_t    RBSTART;
        uint16_t    ERBCR;
        uint8_t     ERSR;
        uint8_t     CR;
        uint16_t    CAPR;
        uint16_t    CBR;
        uint16_t    IMR;
        uint16_t    ISR;
        uint32_t    TCR;
        uint32_t    RCR;
        uint32_t    TCTR;
        uint32_t    MPC;
        uint8_t     CR9346;
        uint8_t     CONFIG0;
        uint8_t     CONFIG1;
        uint8_t     reserved1;
        uint32_t    TimerInt;
        uint8_t     MSR;
        uint8_t     CONFIG2;
        uint8_t     CONFIG3;
        uint8_t     reserved2;
        uint16_t    MULINT;
        uint8_t     RERID;
        uint8_t     reserved3;
        uint16_t    TSAD;
        uint16_t    BMCR;
        uint16_t    BMSR;
        uint16_t    ANAR;
        uint16_t    ANLPAR;
        uint16_t    ANER;
        uint16_t    DIS;
        uint16_t    FCSC;
        uint16_t    NWAYTR;
        uint16_t    REC;
        uint16_t    CSCR;
        uint16_t    reserved4;
        uint32_t    PHY1_PARM;
        uint32_t    TW_PARM;
        uint8_t     PHY2_PARM;
        uint32_t    reserved5;
        uint8_t     CRC0;
        uint8_t     CRC1;
        uint8_t     CRC2;
        uint8_t     CRC3;
        uint8_t     CRC4;
        uint8_t     CRC5;
        uint8_t     CRC6;
        uint8_t     CRC7;
        uint64_t    Wakeup0;
        uint64_t    Wakeup1;
        uint64_t    Wakeup2;
        uint64_t    Wakeup3;
        uint64_t    Wakeup4;
        uint64_t    Wakeup5;
        uint64_t    Wakeup6;
        uint64_t    Wakeup7;
        uint8_t     LSBCRC0;
        uint8_t     LSBCRC1;
        uint8_t     LSBCRC2;
        uint8_t     LSBCRC3;
        uint8_t     LSBCRC4;
        uint8_t     LSBCRC5;
        uint8_t     LSBCRC6;
        uint8_t     LSBCRC7;
        uint32_t    reserved6;
        uint8_t     CONFIG5;
    } __attribute__((packed));

public:

    Realtek8139();

    Realtek8139 (const Realtek8139 &other) = delete;

    Realtek8139 &operator=(const Realtek8139 &other) = delete;

    virtual ~Realtek8139() = default;

    void send(uint8_t *data, uint32_t length);

    void setup(const Pci::Device &dev);

    void trigger() override;

    void plugin();

    static const uint16_t VENDOR_ID = 0x10EC;

    static const uint16_t DEVICE_ID = 0x8139;

private:

    Logger &log;

    void reset();

    void initReceiveBuffer();

    void enableInterrupts();

    void configureReceiveBuffer();

    void configureTransmitBuffer();

    void enable();

    void advanceBuffer();

    bool isBufferEmpty();

    void receive();

    void readMac();

    uint8_t *mac;

    String macAddress;

    Pci::Device pciDevice;

    Registers *registers;

    IOport *ioRegisters;

    uint8_t currentTxBuffer = 0;

    uint8_t *txBuffers[4];

    uint8_t *physTxBuffers[4];

    uint8_t *rxBuffer;

    uint16_t rxBufferOffset = 0;

    static const uint8_t BUFFER_COUNT = 4;

    static const uint16_t RECEIVE_BUFFER_SIZE = 8192 + 16;

    static const uint32_t TRANSMIT_BUFFER_SIZE = 1024 * 1024 * 2;

    /**
     * Buffer Empty
     */
    static const uint8_t CR_BUFE   = 1U << 0U;

    /**
     * Transmitter Enable
     */
    static const uint8_t CR_TE     = 1U << 2U;

    /**
     * Receiver Enable
     */
    static const uint8_t CR_RE     = 1U << 3U;

    /**
     * Reset
     */
    static const uint8_t CR_RST    = 1U << 4U;

    /**
     * Receive OK
     */
    static const uint16_t INT_ROK           = 1U << 0U;

    /**
     * Receive Error
     */
    static const uint16_t INT_RER           = 1U << 1U;

    /**
     * Transmit OK
     */
    static const uint16_t INT_TOK           = 1U << 2U;

    /**
     * Transmit Error
     */
    static const uint16_t INT_TER           = 1U << 3U;

    /**
     * Rx Buffer Overflow
     */
    static const uint16_t INT_RXOVW         = 1U << 4U;

    /**
     * Packet Underrun / Link Change
     */
    static const uint16_t INT_PUN           = 1U << 5U;

    /**
     * Rx FIFO Overflow
     */
    static const uint16_t INT_FOVW          = 1U << 6U;

    /**
     * Cable Length Change
     */
    static const uint16_t INT_LenChg        = 1U << 13U;

    /**
     * Timeout
     */
    static const uint16_t INT_TimeOut       = 1U << 14U;

    /**
     * System Error
     */
    static const uint16_t INT_SERR          = 1U << 15U;

    static const uint32_t TCR_HWVERID_MASK = 0x7CC00000;
};


#endif
