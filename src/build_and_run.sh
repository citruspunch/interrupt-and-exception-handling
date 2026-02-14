#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

TARGET=${TARGET:-qemu}
BUILD_DIR="bin"

mkdir -p "$BUILD_DIR"

echo "Cleaning up previous build files..."
rm -f "$BUILD_DIR"/*.o "$BUILD_DIR"/program.elf "$BUILD_DIR"/program.bin

if [ "$TARGET" = "qemu" ]; then
    CFLAGS="-c -O2 -Wall -ffreestanding -nostdlib -nostartfiles -msoft-float -mcpu=cortex-a8 -marm -DTARGET_QEMU -I OS"
    LDFLAGS="-nostartfiles -nostdlib -T linker_qemu.ld -msoft-float -mcpu=cortex-a8 -marm"
else
    CFLAGS="-c -O2 -Wall -ffreestanding -nostdlib -nostartfiles -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard -DTARGET_BBB -I OS"
    LDFLAGS="-nostartfiles -T linker_bbb.ld -mcpu=cortex-a8 -mfpu=neon -mfloat-abi=hard"
fi

echo "=== Building for target: $TARGET ==="

echo "Assembling OS/root.s..."
arm-none-eabi-as -o "$BUILD_DIR"/root.o OS/root.s

echo "Compiling OS/os.c..."
arm-none-eabi-gcc $CFLAGS -o "$BUILD_DIR"/os.o OS/os.c

echo "Linking object files..."
arm-none-eabi-gcc $LDFLAGS -o "$BUILD_DIR"/program.elf "$BUILD_DIR"/root.o "$BUILD_DIR"/os.o -lgcc

echo "Converting ELF to binary..."
arm-none-eabi-objcopy -O binary "$BUILD_DIR"/program.elf "$BUILD_DIR"/program.bin

if [ "$TARGET" = "qemu" ]; then
    echo "Running QEMU..."
    qemu-system-arm -M versatilepb -cpu cortex-a8 -nographic -kernel "$BUILD_DIR"/program.elf
else
    echo "Build complete for BeagleBone Black target."
fi
