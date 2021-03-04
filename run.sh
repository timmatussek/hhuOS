#!/bin/bash

readonly CONST_PLATFORM=${PLATFORM:-"x86/bios"}
readonly CONST_QEMU_ARGS="-boot d -cdrom hhuOS.iso -vga std -monitor stdio"

# shellcheck disable=SC1090
source "platform/${CONST_PLATFORM}/run.sh"

QEMU_RAM="${CONST_QEMU_MIN_RAM}"
QEMU_GDB_PORT=""

check_file() {
  local file=$1

  if [ ! -f "$file" ]; then
    printf "File '%s' does not exist! Did you run build.sh?\\n" "${file}"
    exit 1
  fi
}

parse_ram() {
  local memory=$1

  QEMU_RAM="${memory}"
}

parse_debug() {
  local port=$1
  local config=""

  if [ -n "${CONST_GDB_ARCHITECTURE}" ]; then
    config="${config}set architecture ${CONST_GDB_ARCHITECTURE}\\n"
  fi

  if [ -n "${CONST_GDB_DISASSEMBLY_FLAVOR}" ]; then
    config="${config}set disassembly-flavor ${CONST_GDB_DISASSEMBLY_FLAVOR}\\n"
  fi

  config="${config}target remote 127.0.0.1:${port}"
  echo "${config}" >/tmp/gdbcommands."$(id -u)"

  QEMU_GDB_PORT="${port}"
}

start_gdb() {
  gdb -x "/tmp/gdbcommands.$(id -u)" "src/${PLATFORM}/loader/boot/hhuOS.bin"
  exit $?
}

print_usage() {
  printf "Usage: ./run.sh [OPTION...]
    Available options:
    -a, --architecture
        Set the architecture, which qemu should emulate ([i386,x86] | [x86_64,x64]) (Default: i386)
    -m, --machine
        Set the machine profile, which qemu should emulate ([pc] | [pc-kvm]) (Defualt: pc)
    -b, --bios
        Set the BIOS, which qemu should use ([bios,default] | [efi] | [/path/to/bios.file]) (Default: bios)
    -r, --ram
        Set the amount of ram, which qemu should use (e.g. 256, 1G, ...) (Default: 256M)
    -d, --debug
        Set the port, on which qemu should listen for GDB clients (default: disabled)
    -h, --help
        Show this help message\\n"
}

parse_args() {
  while [ "${1}" != "" ]; do
    local arg=$1
    local val=$2

    case $arg in
    -r | --ram)
      parse_ram "$val"
      ;;
    -d | --debug)
      parse_debug "$val"
      ;;
    -g | --gdb)
      start_gdb
      ;;
    -h | --help)
      print_usage
      exit 0
      ;;
    *)
      printf "Unknown option '%s'\\n" "${arg}"
      print_usage
      exit 1
      ;;
    esac
    shift 2
  done
}

run_qemu() {
  local command="${CONST_QEMU_BIN} ${CONST_QEMU_ARGS}"

  if [ -n "${CONST_QEMU_MACHINE}" ]; then
    command="${command} -machine ${CONST_QEMU_MACHINE}"
  fi

  if [ -n "${CONST_QEMU_CPU}" ]; then
    command="${command} -cpu ${CONST_QEMU_CPU}"
  fi

  if [ -n "${QEMU_RAM}" ]; then
    command="${command} -m ${QEMU_RAM}"
  fi

  if [ -n "${CONST_QEMU_BIOS}" ]; then
    command="${command} -bios ${CONST_QEMU_BIOS}"
  fi

  printf "Running: %s\\n" "${command}"

  if [ -n "${QEMU_GDB_PORT}" ]; then
    $command -gdb tcp::"${QEMU_GDB_PORT}" -S &
  else
    $command
  fi
}

check_file hhuOS.iso

parse_args "$@"

QEMU_ARGS="${QEMU_ARGS}"

run_qemu
