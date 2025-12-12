import glob
import re
import os

def parse_header(filepath, device_type):
    definitions = []
    with open(filepath, 'r') as f:
        for line in f:
            match = re.match(r'#define\s+(USB_DEVICE_ID_RAZER_\w+)\s+(0x[0-9A-Fa-f]+)', line)
            if match:
                name = match.group(1)
                pid = match.group(2)
                definitions.append((name, pid, device_type))
    return definitions

files = {
    'driver/razermouse_driver.h': 'Mouse',
    'driver/razerkbd_driver.h': 'Keyboard',
    'driver/razerkraken_driver.h': 'Headset',
    'driver/razeraccessory_driver.h': 'Accessory'
}

all_defs = []

for fpath, dtype in files.items():
    if os.path.exists(fpath):
        defs = parse_header(fpath, dtype)
        all_defs.extend(defs)

with open('include/DeviceIds.h', 'w') as f:
    f.write('#pragma once\n\n')
    f.write('// Auto-generated from driver/ headers\n\n')

    # Write Defines
    for name, pid, dtype in all_defs:
        f.write(f'#ifndef {name}\n')
        f.write(f'#define {name} {pid}\n')
        f.write(f'#endif\n')

    f.write('\n#include <map>\n\n')
    f.write('enum class RazerDeviceType { Mouse, Keyboard, Headset, Accessory, Unknown };\n\n')

    f.write('inline RazerDeviceType GetRazerDeviceType(int pid) {\n')
    f.write('    switch(pid) {\n')
    for name, pid, dtype in all_defs:
        f.write(f'    case {name}: return RazerDeviceType::{dtype};\n')
    f.write('    default: return RazerDeviceType::Unknown;\n')
    f.write('    }\n')
    f.write('}\n')

print(f"Generated {len(all_defs)} device IDs.")
