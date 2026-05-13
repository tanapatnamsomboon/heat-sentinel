#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

/* Hardware SPI Master */
void spi_init(void);
uint8_t spi_transfer(uint8_t data);

#endif /* HAL_SPI_H */