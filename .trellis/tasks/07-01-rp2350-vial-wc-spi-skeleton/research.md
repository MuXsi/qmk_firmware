# Research Notes

- WCH official CH592 product page: `https://www.wch-ic.com/products/CH592.html`
- Local CH592F reference project:
  - `/home/s/mounriver-studio-projects/CH592F/APP/include/wireless_proto.h`
  - `/home/s/mounriver-studio-projects/CH592F/APP/wireless_proto.c`
  - `/home/s/mounriver-studio-projects/CH592F/APP/wireless_spi.c`
- The local CH592F project currently uses an older 64-byte SPI frame with `'C','W'` magic, 54-byte payload, 1-byte sequence, and CRC at the tail. This task intentionally defines the newer wireless coprocessor protocol instead.
- QMK/Vial SPI support comes from `drivers/spi_master.h` and `platforms/chibios/drivers/spi_master.c`; `SPI_DRIVER_REQUIRED = yes` enables the ChibiOS SPI driver path.
