#include <hardwarecomms/pci.h>
#include <common/types.h>
#include <hardwarecomms/portio.h>
#include <io/screen.h>
enum PCI_IO {
    PCI_DATA_PORT = 0xCFC,
    PCI_CMD_PORT   = 0xCF8,
};

typedef enum BaseAddressRegisterType {
    BAR_MEM_MAP = 0,
    BAR_IO = 1,
} bar_type_t;

typedef struct pci_entry_desc {
    uint16_t portBase;
    uint16_t interrupt;

    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendorID;
    uint16_t deviceID;

    uint8_t classID;
    uint8_t subclassID;
    uint8_t interfaceID;

    uint8_t revision;

} pci_entry_desc_t;

typedef struct base_addrs_reg {
    bool prefetchable;
    uint8_t* addrs;
    uint32_t size;
    bar_type_t type;
} bar_t;

//reades a certain pci device
uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset)
{
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldev = (uint32_t)device;
    uint32_t lfunc = (uint32_t)function;
    uint32_t id =(uint32_t) 
    (0x01 << 31
     | (lbus << 16)
     | (ldev << 11)
     | (lfunc << 8)
     | (registeroffset & 0xFC));
    
    outl(PCI_CMD_PORT, id);
    uint32_t res = inl(PCI_DATA_PORT);
    return res >> (8 * (registeroffset % 4));
}

void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset, uint32_t data)
{
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldev = (uint32_t)device;
    uint32_t lfunc = (uint32_t)function;
    uint32_t id =(uint32_t) 
    (0x01 << 31
     | (lbus << 16)
     | (ldev << 11)
     | (lfunc << 8)
     | (registeroffset & 0xFC));
    outl(PCI_CMD_PORT, id);
    outl(PCI_DATA_PORT, data);
}

//checkes if a certain device has functions
bool device_has_funcs(uint8_t bus, uint8_t device)
{
    return pci_read(bus, device, 0, 0x0E) & (1<<7);
}

pci_entry_desc_t create_entry(uint8_t bus, uint8_t device, uint8_t function)
{
    pci_entry_desc_t entry;

    entry.bus = bus;
    entry.device = device;
    entry.function = function;

    entry.vendorID = pci_read(bus, device, function, 0x00);
    entry.deviceID = pci_read(bus, device, function, 0x02);

    entry.classID = pci_read(bus, device, function, 0x0b);
    entry.subclassID = pci_read(bus, device, function, 0x0a);
    entry.interfaceID = pci_read(bus, device, function, 0x09);

    entry.revision = pci_read(bus, device, function, 0x08);
    entry.interrupt = pci_read(bus, device, function, 0x3c);

    return entry;
}

void print_entry(pci_entry_desc_t entry)
{
        terminal_write_string("PCI BUS ");
        terminal_write_int(entry.bus, 16);
        
        terminal_write_string(", DEVICE ");
        terminal_write_int(entry.device, 16);

        terminal_write_string(", FUNCTION ");
        terminal_write_int(entry.function, 16);
        
        terminal_write_string(" = VENDOR ");
        terminal_write_int(entry.vendorID, 16);

        terminal_write_string(", DEVICE ");
        terminal_write_int(entry.deviceID, 16);

        terminal_write_string(", BAR ");
        terminal_write_int(entry.portBase, 16);
        
        terminal_write_string("\n");
}

bar_t get_base_address_register(uint8_t bus, uint8_t device, uint8_t func, size_t bar)
{
    bar_t res;

    uint32_t headerType = pci_read(bus, device, func, 0x0E) & 0x07F;
    size_t maxBARs = 6 - (4*headerType);
    if (bar >= maxBARs)
        return res;
    
    uint32_t barVal = pci_read(bus, device, func, 0x10 + 4*bar);
    res.type = (barVal & 0x1) ? BAR_IO : BAR_MEM_MAP;

    if (res.type == BAR_MEM_MAP) {
        switch ((barVal >> 1) & 0x3)
        {
        case 0:
        case 1:
        case 2:
            break;
        }
    } else {
        res.addrs = (uint8_t*)(barVal & ~0x3);
        res.prefetchable = false;
    }

    return res;
}

void select_drivers()
{
    for (size_t bus = 0; bus < 8; bus++)
    {
        for (size_t device = 0; device < 32; device++)
        {
            size_t func_count = device_has_funcs(bus, device)? 8 : 1;
            for (size_t func = 0; func < func_count; func++)
            {
                pci_entry_desc_t entry = create_entry(bus, device, func);
                if (entry.vendorID == 0x0000 || entry.vendorID == 0xFFFF)
                    continue;
            
                for (size_t barNum = 0; barNum < 6; barNum++)
                {
                    bar_t bar = get_base_address_register(bus, device, func, barNum);
                    if(bar.addrs && (bar.type == BAR_IO))
                        entry.portBase = (uint32_t)bar.addrs;
                }
                print_entry(entry);
            }
            
        }
        
    }
}


