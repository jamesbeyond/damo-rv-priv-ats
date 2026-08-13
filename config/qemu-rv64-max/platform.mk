# QEMU virt platform configuration

# Cross compiler
CROSS_COMPILER ?= riscv64-unknown-elf-

# QEMU configuration
QEMU = qemu-system-riscv$(XLEN)
QEMU_CPU ?= max
QEMU_MEM ?= 4G
QEMU_SMP ?= 1
# aia-guests provides per-hart VS-level IMSIC guest interrupt files
# (GEILEN); QEMU defaults to 0, which leaves guest IMSIC state
# (stopei/VGEIN paths) untestable. Valid range 0..7.
QEMU_MCH ?= virt,aia=aplic-imsic,aia-guests=7
QEMU_OPTS = -machine $(QEMU_MCH) \
            -cpu $(QEMU_CPU) \
            -bios none \
            -kernel $(TARGET) \
            -m $(QEMU_MEM) \
            -smp $(QEMU_SMP) \
            -semihosting \
            -nographic

# Memory base (QEMU virt default)
MEM_BASE ?= 0x80000000
