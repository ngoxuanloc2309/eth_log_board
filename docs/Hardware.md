# Hardware Resource Mapping

| Peripheral | Interface | Target | Configuration / ownership |
|------------|-----------|--------|---------------------------|
| SPI1 | `sx_spi_if_t` | SPI device drivers | STM32H5 master, 8-bit, mode 0, MSB first, software NSS, prescaler 8; serialized by `sx_spi` |
| W25Q SPI flash | `w25q_driver` | LittleFS `file_system` | Device-specific driver; operations are serialized by the filesystem mutex |
| PD6 (`SPI1_CS`) | Board W25Q chip-select callback | W25Q flash | Active low; initialized inactive and asserted only for a W25Q transaction |

SPI1 pins are configured by CubeMX: PD7 MOSI, PB3 SCK, and PB4 MISO. The
configured SPI1 interrupt priority is 5. The W25Q adapter obtains its bus from
`sx_spi_get_interface()` and uses FreeRTOS delays while polling flash-ready
status.
