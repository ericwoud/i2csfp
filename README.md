# i2csfp

Build with:
```
gcc -Wall -o i2csfp i2csfp.c --static
```

```
Usage: i2csfp I2CBUS command ...
   I2CBUS is one of:
	  sfp-X	  for exclusive access (use restore when done)
	  /dev/i2c-X for shared acces with sfp cage
   Command one of:
        listsfps       Lists active sfp cages
	 i2cdump        Same as the i2cdump command
	 eepromdump     Same as i2cdump, including pages at 0x51
	 eepromfix      Edit eeprom contents
	 restore	Restores sfp cage after exclusive access
	 byte           Byte access on i2c address
	 c22m	        Clause 22 MARVELL access on i2c address
	 c22r	        Clause 22 ROLLBALL at 0x56 (read-only?)
	 c45		Clause 45 access on i2c address
	 rollball       Rollball protocol (Clause 45 via 0x51)
        gpio           Get/set gpio input/ouput
	 rbpassword     Extract Rollball eeprom password
	 bruteforce     Find password using brute force

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

 i2csfp I2CBUS gpio GPIONAME [on|off]
   GPIONAME [on|off] one the outputs: tx-disable, rate-select0, rate-select1
   GPIONAME          one the inputs:  mod-def0, los, tx-fault

 i2csfp I2CBUS rbpassword

 i2csfp I2CBUS bruteforce [-p] [MIN] [MAX]
   Runs brute force attack on sfp module
   -p specify password to start with (last 2 bytes zeroed)
   -E specify which attack: 1 (0x50) or 2 (0x56), default 1
   MIN is the first byte to try 0x00 - 0xff, default 0x00
   MAX is the last  byte to try 0x00 - 0xff, default 0xff
```
