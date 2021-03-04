export readonly CONST_QEMU_BIN="qemu-system-i386"
export readonly CONST_QEMU_MACHINE="pc"
export readonly CONST_QEMU_CPU="pentium"
export readonly CONST_QEMU_MIN_RAM="64M"
export readonly CONST_QEMU_BIOS=${OVMF_IMAGE:-"/usr/share/edk2-ovmf/ia32/OVMF_CODE.fd"}
export readonly CONST_GDB_ARCHITECTURE="i386"
export readonly CONST_GDB_DISASSEMBLY_FLAVOR="intel"
