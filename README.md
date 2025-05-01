# i2csfp

USE WITH CAUTION!!!

## Features

1. i2cdump
2. eeprom dump, showing different pages
3. edit eeprom data
4. read/write using protocols: byte, c22m, c22r, c45, rollball
5. Extract rollball password
6. Brute force attack eeprom

It can edit eeprom data of the OEM modules so it can match another string, so you do not need to add another quirk if the same hardware already has a quirk but uses different id string.

I've only briefly tested it:

[ericwoud/i2csfp (github.com)](https://github.com/ericwoud/i2csfp)

It is still in beta-stage.

I have added pre-build static executables to the release page here.
[https://github.com/ericwoud/i2csfp/releases/tag/0.1](https://github.com/ericwoud/i2csfp/releases/tag/0.1)

One could do:
```
i2csfp sfp-X eepromfix -V Turris -N RTSFP-2.5G -E 0x1e
```
For example, to use the OEM module, using the newly added patch for the Turris module, although I still have to try the result myself ;)

Because of this code in linux kernel,
```
bool sfp_may_have_phy(struct sfp_bus *bus, const struct sfp_eeprom_id *id)
{
	if (id->base.e1000_base_t)
		return true;

	if (id->base.phys_id != SFF8024_ID_DWDM_SFP) {
		switch (id->base.extended_cc) {
		case SFF8024_ECC_10GBASE_T_SFI:
		case SFF8024_ECC_10GBASE_T_SR:
		case SFF8024_ECC_5GBASE_T:
		case SFF8024_ECC_2_5GBASE_T:
			return true;
		}
	}

	return false;
}
```
We need to set extended_cc to 0x1e for 2.5G module phy to be recognised.

## Build with
```
gcc -Wall -o i2csfp i2csfp.c --static
```

## Usage
```p
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

 i2csfp I2CBUS byte read|write [-p PASSWORD] [-v] BUS-ADDRESS REGISTER [VALUE]
   -p PASSWORD specify password
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
