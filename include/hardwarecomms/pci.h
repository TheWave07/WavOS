#ifndef __WAVOS__HARDWARECOMMS__PCI_H
#define __WAVOS__HARDWARECOMMS__PCI_H
#include <common/types.h>
uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset);
void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset, uint32_t data);
void select_drivers();
#endif
