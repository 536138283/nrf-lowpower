SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0x5a000
  RAM (rwx) : ORIGIN = 0x20002260, LENGTH = 0xdda0
}

SECTIONS
{
}

INCLUDE "nrf_common.ld"
