# PNX49xx-RAM-Dumper

Unofficial Samsung Swift (PNX49xx chipset) RAM Dumper for Linux.

## How to use

Dial the engineer secret code: *#7263867#* (*#RAMDUMP#*), white window will appear saying "*RAM Dump: ON*".

After this you'll be able to enter the RAM Dump mode by pressing a key combination of hash (#) + end key (hang up).

Phone will then display a tiny window with "*43020144*" red italic error code, blinking along with the keypad backlight. On newer Swift phones this window may look different and display way more info, than the older Swift phones are.

Then connect the phone to PC using UART serial cable, and then run the RAM Dumper program on PC.

## Examples of cmdline

**Run RAM Dump with default settings**: `ramdump-49xx`

**Print help**: `ramdump-49xx -h` or `ramdump-49xx --help`

**Run RAM Dump verbosely with 921600 baud**: `ramdump-49xx -vb3` or `ramdump-49xx -v -b3`

