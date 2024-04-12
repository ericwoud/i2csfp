# i2csfp
```
Usage: i2csfp I2CBUS command ...
   I2CBUS is i2c bus device name (/dev/i2c-x)
 Command one of:
   i2cdump
   eepromdump
   eepromfix
   byte
   c22m       Clause 22 MARVELL
   c22r       Clause 22 ROLLBALL at 0x56 (read-only?)
   c45        Clause 45
   rollball   Rollball protocol (Clause 45)
   rbpassword Extract Rollball eeprom password
   bruteforce

 i2csfp I2CBUS i2cdump BUS-ADDRESS
   BUS-ADDRESS is an integer 0x00 - 0x7f

 i2csfp I2CBUS eepromdump [LASTPAGE]
   LASTPAGE is the last page number to show, default 3

 i2csfp I2CBUS eepromfix [-p PASSWORD] [-e EXTCC] [-v VDNAME] [-n VDPN]
   -p PASSWORD specify password, without this option uses rbpassword
   -V VDNAME specify vendor name
   -N VDPN specify vendor pn
   -E EXTCC specify extended cc

 i2csfp I2CBUS byte read|write [-v] BUS-ADDRESS REGISTER [VALUE]
   -v verify write
   BUS-ADDRESS is an integer 0x00 - 0x7f
   REGISTER is an integer 0x00 - 0x7f
   VALUE is an integer 0x00 - 0xff

 i2csfp I2CBUS c22m read|write BUS-ADDRESS REGISTER [VALUE]
   BUS-ADDRESS is an integer 0x00 - 0x7f
   REGISTER is an integer 0x00 - 0x1f
   VALUE is an integer 0x00 - 0xffff

 i2csfp I2CBUS c22r read|write BUS-ADDRESS REGISTER [VALUE]
   BUS-ADDRESS is an integer 0x00 - 0x7f
   REGISTER is an integer 0x00 - 0x1f
   VALUE is an integer 0x00 - 0xffff

 i2csfp I2CBUS c45 read|write BUS-ADDRESS DEVAD REGISTER [VALUE]
   BUS-ADDRESS is an integer 0x00 - 0x7f
   DEVAD is an integer 0x00 - 0x1f
   REGISTER is an integer 0x00 - 0xffff
   VALUE is an integer 0x00 - 0xffff

 i2csfp I2CBUS rollball read|write DEVAD REGISTER [VALUE]
   DEVAD is an integer 0x00 - 0x1f
   REGISTER is an integer 0x00 - 0xffff
   VALUE is an integer 0x00 - 0xffff

 i2csfp I2CBUS rbpassword

 i2csfp I2CBUS bruteforce [-p] [MIN] [MAX]
   Runs brute force attack on sfp module
   -p specify password to start with (last 2 bytes zeroed)
   -E specify which attack: 1 (0x50) or 2 (0x56), default 1
   MIN is the first byte to try 0x00 - 0xff, default 0x00
   MAX is the last  byte to try 0x00 - 0xff, default 0xff
```
