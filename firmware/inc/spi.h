#pragma once

/* Kick off DMA transfer from RAM to SPI interfaces */
void kickoff_transfers(void);
void kickoff_transfer(unsigned int channel, unsigned int offset, int base);
void ssi_udma_channel_config(unsigned int channel);

void spi_init(void);
