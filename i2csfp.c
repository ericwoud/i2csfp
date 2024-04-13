//SPDX-License-Identifier: GPL-2.0
/*
 * i2csfp.c - A user-space program to access sfp module via i2c.
 * Copyright (C) 2024 Eric Woudstra <ericwouds@gmail.com>
 *
 *
 * Protocol functions based on mdio-i2c.c from linux kernel:
 * Copyright (C) 2015-2016 Russell King
 * Copyright (C) 2021 Marek Behun
 *
 * gcc -Wall -o i2csfp i2csfp.c --static
*/

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define EEPROMDELAY 50000 // microseconds

#define SIZEOFPATH 256

#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

int file = -1;
char * i2cname;
char i2cdevname[SIZEOFPATH+16];

/* EEPROM 0x50:
   20 char vendor_name[16];
   36 u8 extended_cc; fixup sets it to SFF8024_ECC_2_5GBASE_T = 0x1e
   37 char vendor_oui[3];
   40 char vendor_pn[16];
*/

static void help(void)
{
	fprintf(stderr,
		"Usage:"
		" i2csfp I2CBUS command ...\n"
		"   I2CBUS is one of:\n"
		"      sfp-X      for exclusive access (use restore when done)\n"
		"      /dev/i2c-X for shared acces with sfp cage\n"
		"   Command one of:\n"
		"     i2cdump\n"
		"     eepromdump\n"
		"     eepromfix\n"
		"     restore    Restores sfp cage after exclusive access\n"
		"     byte\n"
		"     c22m       Clause 22 MARVELL\n"
		"     c22r       Clause 22 ROLLBALL at 0x56 (read-only?)\n"
		"     c45        Clause 45\n"
		"     rollball   Rollball protocol (Clause 45)\n"
		"     rbpassword Extract Rollball eeprom password\n"
		"     bruteforce\n"
		"\n"
		" i2csfp I2CBUS i2cdump BUS-ADDRESS\n"
		"   BUS-ADDRESS is an integer 0x00 - 0x7f\n"
		"\n"
		" i2csfp I2CBUS eepromdump [LASTPAGE]\n"
		"   LASTPAGE is the last page number to show, default 3\n"
		"\n"
		" i2csfp I2CBUS eepromfix [-p PASSWORD] [-e EXTCC] [-v VDNAME] [-n VDPN]\n"
		"   -p PASSWORD specify password, without this option uses rbpassword\n"
		"   -V VDNAME specify vendor name\n"
		"   -N VDPN specify vendor pn\n"
		"   -E EXTCC specify extended cc\n"
		"\n"
		" i2csfp I2CBUS byte read|write [-v] BUS-ADDRESS REGISTER [VALUE]\n"
		"   -v verify write\n"
		"   BUS-ADDRESS is an integer 0x00 - 0x7f\n"
		"   REGISTER is an integer 0x00 - 0x7f\n"
		"   VALUE is an integer 0x00 - 0xff\n"
		"\n"
		" i2csfp I2CBUS c22m read|write BUS-ADDRESS REGISTER [VALUE]\n"
		"   BUS-ADDRESS is an integer 0x00 - 0x7f\n"
		"   REGISTER is an integer 0x00 - 0x1f\n"
		"   VALUE is an integer 0x00 - 0xffff\n"
		"\n"
		" i2csfp I2CBUS c22r read|write BUS-ADDRESS REGISTER [VALUE]\n"
		"   BUS-ADDRESS is an integer 0x00 - 0x7f\n"
		"   REGISTER is an integer 0x00 - 0x1f\n"
		"   VALUE is an integer 0x00 - 0xffff\n"
		"\n"
		" i2csfp I2CBUS c45 read|write BUS-ADDRESS DEVAD REGISTER [VALUE]\n"
		"   BUS-ADDRESS is an integer 0x00 - 0x7f\n"
		"   DEVAD is an integer 0x00 - 0x1f\n"
		"   REGISTER is an integer 0x00 - 0xffff\n"
		"   VALUE is an integer 0x00 - 0xffff\n"
		"\n"
		" i2csfp I2CBUS rollball read|write DEVAD REGISTER [VALUE]\n"
		"   DEVAD is an integer 0x00 - 0x1f\n"
		"   REGISTER is an integer 0x00 - 0xffff\n"
		"   VALUE is an integer 0x00 - 0xffff\n"
		"\n"
		" i2csfp I2CBUS rbpassword\n"
		"\n"
		" i2csfp I2CBUS bruteforce [-p] [MIN] [MAX]\n"
		"   Runs brute force attack on sfp module\n"
		"   -p specify password to start with (last 2 bytes zeroed)\n"
		"   -E specify which attack: 1 (0x50) or 2 (0x56), default 1\n"
		"   MIN is the first byte to try 0x00 - 0xff, default 0x00\n"
		"   MAX is the last  byte to try 0x00 - 0xff, default 0xff\n"
	);
}

static int i2c_transfer(int file, struct i2c_msg * msgs, int count)
{
	struct i2c_rdwr_ioctl_data msgset[1];
	int res;

	msgset[0].msgs = msgs;
	msgset[0].nmsgs = count;

	res = ioctl(file, I2C_RDWR, &msgset);
	if (res < 0) {
		fprintf(stderr, "Error: i2c_transfer() failed: %s\n", strerror(errno));
		return -errno;
	}

	return res;
}

static int i2c_write_byte(int file, uint8_t bus_addr, uint8_t reg, uint8_t val)
{
	struct i2c_msg msgs[1];
	uint8_t outbuf[2];

	outbuf[0] = reg;
	outbuf[1] = val;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(outbuf);
	msgs[0].buf = outbuf;

	return i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
}

static int i2c_read_byte(int file, uint8_t bus_addr, uint8_t reg)
{
	struct i2c_msg msgs[2];
	uint8_t outbuf[1], inbuf[1];
	int res;

	outbuf[0] = reg;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(outbuf);
	msgs[0].buf = outbuf;

	msgs[1].addr = bus_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(inbuf);
	msgs[1].buf = inbuf;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return inbuf[0];
}


static int i2c_mii_write_default_c22(int file, uint8_t phy_id, uint8_t reg, uint16_t val)
{
	struct i2c_msg msgs[1];
	uint8_t data[3];
	int res, bus_addr;

	bus_addr = (phy_id < 0x40) ? phy_id + 0x40 : phy_id;

	data[0] = reg;
	data[1] = val >> 8;
	data[2] = val;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(data);
	msgs[0].buf = data;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return 0;
}

static int i2c_mii_read_default_c22(int file, uint8_t phy_id, uint8_t reg)
{
	struct i2c_msg msgs[2];
	uint8_t addr[1], data[2];
	int res, bus_addr;

	bus_addr = (phy_id < 0x40) ? phy_id + 0x40 : phy_id;

	addr[0] = reg;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr);
	msgs[0].buf = addr;

	msgs[1].addr = bus_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(data);
	msgs[1].buf = data;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return data[0] << 8 | data[1];
}

static int i2c_mii_write_default_c45(int file, uint8_t phy_id, uint8_t devad, uint16_t reg, uint16_t val)
{
	struct i2c_msg msgs[1];
	uint8_t data[5];
	int res, bus_addr;

	bus_addr = (phy_id < 0x40) ? phy_id + 0x40 : phy_id;

	data[0] = devad;
	data[1] = reg >> 8;
	data[2] = reg;
	data[3] = val >> 8;
	data[4] = val;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(data);
	msgs[0].buf = data;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return 0;
}

static int i2c_mii_read_default_c45(int file, uint8_t phy_id, uint8_t devad, uint16_t reg)
{
	struct i2c_msg msgs[2];
	uint8_t addr[3], data[2];
	int res, bus_addr;

	bus_addr = (phy_id < 0x40) ? phy_id + 0x40 : phy_id;

	addr[0] = devad;
	addr[1] = reg >> 8;
	addr[2] = reg;

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(addr);
	msgs[0].buf = addr;

	msgs[1].addr = bus_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(data);
	msgs[1].buf = data;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return data[0] << 8 | data[1];
}

#define ROLLBALL_PWD_ADDR		0x7b
#define ROLLBALL_CMD_ADDR		0x80
#define ROLLBALL_DATA_ADDR		0x81

#define ROLLBALL_CMD_WRITE		0x01
#define ROLLBALL_CMD_READ		0x02
#define ROLLBALL_CMD_DONE		0x04

static int i2c_transfer_rollball(int file, struct i2c_msg * msgs, int count)
{
	int res, main_res, saved_page;

	saved_page = i2c_read_byte(file, 0x51, 0x7f);
	if (saved_page < 0) return saved_page;

	res = i2c_write_byte(file, 0x51, 0x7f, 0x03);
	if (res < 0) return res;

	main_res = i2c_transfer(file, msgs, count);

	res = i2c_write_byte(file, 0x51, 0x7f, saved_page);
	if (res < 0) return res;

	return main_res;
}

static int i2c_mii_write_rollball(int file, uint8_t devad, uint16_t reg, uint16_t val)
{
	struct i2c_msg msgs[2];
	uint8_t buf[6], cmdbuf[2], cmd_addr[1], cmd_res[1];
	int res, i;

	buf[0] = ROLLBALL_DATA_ADDR;
	buf[1] = devad;
	buf[2] = reg >> 8;
	buf[3] = reg;
	buf[4] = val >> 8;
	buf[5] = val;

	cmdbuf[0] = ROLLBALL_CMD_ADDR;
	cmdbuf[1] = ROLLBALL_CMD_WRITE;

	msgs[0].addr = 0x51;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buf);
	msgs[0].buf = buf;

	msgs[1].addr = 0x51;
	msgs[1].flags = 0;
	msgs[1].len = sizeof(cmdbuf);
	msgs[1].buf = cmdbuf;

	res = i2c_transfer_rollball(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	cmd_addr[0] = ROLLBALL_CMD_ADDR;

	msgs[0].addr = 0x51;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(cmd_addr);
	msgs[0].buf = cmd_addr;

	msgs[1].addr = 0x51;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(cmd_res);
	msgs[1].buf = cmd_res;

	for (i = 0; i < 10; ++i) {
		usleep(20000);

		res = i2c_transfer_rollball(file, msgs, ARRAY_SIZE(msgs));
		if (res < 0) return res;

		if (cmd_res[0] == ROLLBALL_CMD_DONE)
			return 0;
	}

	return -ETIMEDOUT;
}

static int i2c_mii_read_rollball(int file, uint8_t devad, uint16_t reg)
{
	struct i2c_msg msgs[2];
	uint8_t buf[4], cmdbuf[2], cmd_addr[1], cmd_res[6];
	int res, i;

	buf[0] = ROLLBALL_DATA_ADDR;
	buf[1] = devad;
	buf[2] = reg >> 8;
	buf[3] = reg;

	cmdbuf[0] = ROLLBALL_CMD_ADDR;
	cmdbuf[1] = ROLLBALL_CMD_READ;

	msgs[0].addr = 0x51;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buf);
	msgs[0].buf = buf;

	msgs[1].addr = 0x51;
	msgs[1].flags = 0;
	msgs[1].len = sizeof(cmdbuf);
	msgs[1].buf = cmdbuf;

	res = i2c_transfer_rollball(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	cmd_addr[0] = ROLLBALL_CMD_ADDR;

	msgs[0].addr = 0x51;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(cmd_addr);
	msgs[0].buf = cmd_addr;

	msgs[1].addr = 0x51;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(cmd_res);
	msgs[1].buf = cmd_res;

	for (i = 0; i < 10; ++i) {
		usleep(20000);

		res = i2c_transfer_rollball(file, msgs, ARRAY_SIZE(msgs));
		if (res < 0) return res;

		if (cmd_res[0] == ROLLBALL_CMD_DONE)
			return cmd_res[4] << 8 | cmd_res[5];;
	}

	return -ETIMEDOUT;
}

static int i2c_mii_init_rollball(int file)
{
	struct i2c_msg msgs[1];
	uint8_t data[5];
	int res;

/*
	for (i = 0; i < 4; ++i) {
		res = i2c_write_byte(file, 0x51, ROLLBALL_PWD_ADDR + i, 0xff);
		if (res < 0) return res;
	}
*/
	data[0] = ROLLBALL_PWD_ADDR;
	data[1] = 0xff;
	data[2] = 0xff;
	data[3] = 0xff;
	data[4] = 0xff;

	msgs[0].addr = 0x51;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(data);
	msgs[0].buf = data;

	res = i2c_transfer(file, msgs, ARRAY_SIZE(msgs));
	if (res < 0) return res;

	return 0;
}

static int checksum(int file, int start, int end, bool fix)
{
	int c, res, sum = 0;

	for (c = start; c <= end; c++) {
		res = i2c_read_byte(file, 0x50, c);
		if (res < 0) return res;
		sum = (sum + res) & 0xff;
	}
	res = i2c_read_byte(file, 0x50, c);
	if (res < 0) return res;
	if (res == sum) {
		printf("Checksum 0x%02x-0x%02x matched %02x\n", start, end, sum);
	} else {
		printf("Checksum 0x%02x-0x%02x failed, set at %02x, but should be %02x\n",
			start, end, res, sum);
		if (fix) {
			printf("Writing checksum %02x\n", sum);
			res = i2c_write_byte(file, 0x50, c, sum);
			usleep(EEPROMDELAY);
			if (res < 0) return res;
		}
	}

	return 0;
}

static int checksums(int file, bool fix)
{
	int res;

	res = checksum(file, 0x0, 0x3e, fix);
	if (res < 0) return res;

	res = checksum(file, 0x40, 0x5e, fix);
	if (res < 0) return res;

	return 0;
}

static int fillstring(int file, const char *str, int start, int size)
{
	int c, res, i=0, val;

	for (c = start; c < (start + size); c++) {
		if (i < strlen(str)) val = str[i++]; else val = ' ';
		res = i2c_write_byte(file, 0x50, c, val);
		usleep(EEPROMDELAY);
		if (res < 0) return res;
	}
	return 0;
}

static int fillpassword(int file, unsigned int pw)
{
	int res;

	res = i2c_write_byte(file, 0x51, 0x7b, (pw>>24) & 0xff);
	if (res < 0) return res;
	res = i2c_write_byte(file, 0x51, 0x7c, (pw>>16) & 0xff);
	if (res < 0) return res;
	res = i2c_write_byte(file, 0x51, 0x7d, (pw>> 8) & 0xff);
	if (res < 0) return res;
	res = i2c_write_byte(file, 0x51, 0x7e, (pw	) & 0xff);
	if (res < 0) return res;

	return 0;
}

static int rbpassword(int file, uint32_t * pw)
{
	int res, saved_page;

	saved_page = i2c_read_byte(file, 0x51, 0x7f);
	if (saved_page < 0) return saved_page;

	res = i2c_write_byte(file, 0x51, 0x7f, 0x03);
	if (res < 0) return res;

	*pw = 0;
	res = i2c_read_byte(file, 0x51, 0xfc);
	*pw |= res << 24;
	res = i2c_read_byte(file, 0x51, 0xfd);
	*pw |= res << 16;
	res = i2c_read_byte(file, 0x51, 0xfe);
	*pw |= res << 8;
	res = i2c_read_byte(file, 0x51, 0xff);
	*pw |= res;

	return i2c_write_byte(file, 0x51, 0x7f, saved_page);
}

static void printheader()
{
	printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f   "
		"0123456789abcdef\n");
}

static int printline(int file, uint8_t bus_addr, int offs)
{
	uint8_t chars[16];
	char str[17];
	int res, i;

	for (i = 0; i < sizeof(chars); i++) {
		res = i2c_read_byte(file, bus_addr, (offs + i) & 0xff);
		if (res < 0) return res;
		chars[i] = res;
		if ((res < 32) || (res > 126) ) res = '.';
		str[i] = res;
	}
	str[i] = 0;

	printf("%02x: "
		"%02x %02x %02x %02x "
		"%02x %02x %02x %02x "
		"%02x %02x %02x %02x "
		"%02x %02x %02x %02x "
		"  %s\n",
		offs,
		chars[ 0], chars[ 1], chars[ 2], chars[ 3],
		chars[ 4], chars[ 5], chars[ 6], chars[ 7],
		chars[ 8], chars[ 9], chars[10], chars[11],
		chars[12], chars[13], chars[14], chars[15],
		str
		);

	return 0;
}

static int i2cdump(int file, int busaddr)
{
	int i;

	printheader();
	for (i = 0; i < 16; i++)
		printline(file, busaddr, i<<4);

	return 0;
}

static int printeeprom(int file, int lastpage)
{
	int saved_page, i, j;

	printf("0x50:\n");
	printheader();
	for (i = 0; i < 16; i++)
		printline(file, 0x50, i<<4);
	printf("0x51:\n");
	printheader();
	for (i = 0; i < 8; i++)
		printline(file, 0x51, i<<4);

	saved_page = i2c_read_byte(file, 0x51, 0x7f);
	if (saved_page < 0) return saved_page;


	for (j = 0; j <= lastpage; j++) {
		i2c_write_byte(file, 0x51, 0x7f, j);
		printf("0x51 PAGE 0x%02x:\n", j);
		printheader();
		for (i = 8; i < 16; i++)
			printline(file, 0x51, i<<4);
	}

	return i2c_write_byte(file, 0x51, 0x7f, saved_page);
}

static int bfread_1(int file)
{
	return i2c_read_byte(file, 0x50, 0x14);
}
static int bfwrite_1(int file, int value)
{
	return i2c_write_byte(file, 0x50, 0x14, value);
}
static int bfmod_1(int value)
{
	return value ^ 1;
}
static int bfread_2(int file)
{
	return i2c_mii_read_default_c22(file, 0x50, 0x14);
}
static int bfwrite_2(int file, int value)
{
	return i2c_mii_write_default_c22(file, 0x56, 8, value);
}
static int bfmod_2(int value)
{
	return value ^ 0x20;
}

static int bruteforcepart(int file, int value, bool check, int min, int max,
			int (*bfread)(), int (*bfwrite)(), int (*bfmod)())
{
	unsigned int c,d;
	int res;
	for (c = min; c <= max; c++) {
//		printf("Checking c=%02x\n", c);
		res = i2c_write_byte(file, 0x51, 0x7d, c);
		if (res < 0) return res;
		for (d = min; d <= max; d++) {
			res = i2c_write_byte(file, 0x51, 0x7e, d);
			if (res < 0) return res;
			res = bfwrite(file, value);
			if (res < 0) return res;
			if (check) {
				res = bfread(file);
				if (res < 0) return res;
				else if (res == value) {
					printf("Readback matched at 0xXXXX%02x%02x\n", c, d);
					return 1;
				}
			}
		}
	}

	if (!check) {
		res = bfread(file);
		if (res < 0) return res;
		else if (res == value) {
			printf("Readback matched at SOMEWHERE!\n");
			return 1;
		}
	}
	return 0;
}

static int runbruteforce(int file, unsigned int start, int min, int max,
			int (*bfread)(), int (*bfwrite)(), int (*bfmod)())
{
	unsigned int a,b;
	int res, orig;
	if (max < min) max = min;
	uint8_t sa = start >> 24; if ((sa < min) || (sa > max)) sa = min;
	uint8_t sb = start >> 16; if ((sb < min) || (sb > max)) sb = min;

	orig = bfread(file);
	if (orig < 0) return orig;

	for (a = sa; a <= max; a++) {
		res = i2c_write_byte(file, 0x51, 0x7b, a);
		if (res < 0) return res;
		for (b = sb; b <= max; b++) {
			res = i2c_write_byte(file, 0x51, 0x7c, b);
			if (res < 0) return res;
			printf("Checking 0x%02x%02xXXXX (%d/%d)\n", a, b,
				1 + (a  -min) * (1+max-min) + (b-min),
				  (1+max-min) * (1+max-min) );
			fflush(stdout);
			res = bruteforcepart(file, bfmod(orig), false, min, max, bfread, bfwrite, bfmod);
			if (res < 0) {
				fprintf(stderr, "Error: bruteforcepart failed\n");
				return res;
			} else if (res == 1) {
				/* FOUND */
				res = bruteforcepart(file, orig, true, min, max, bfread, bfwrite, bfmod);
				if (res < 0) {
					fprintf(stderr, "Error: bruteforcepart failed\n");
					return res;
				}
			return 1;
			}
		}
	}
	return 0;
}

static void exiterror(char * str)
{
	if (file >= 0) close(file);
	fprintf(stderr, str);
	exit(1);
}

static void exithelp(char * str)
{
	if (file >= 0) close(file);
	fprintf(stderr, str);
	help();
	exit(1);
}


static int sysreadbe32(char * path)
{
	int fd, res;
	__be32 num;

	fd = open(path, O_RDONLY);
	if (fd < 0) return fd;
	res = read(fd, &num, sizeof(num));
	close(fd);
	if (res < 0) return res;
	return be32toh(num);
}
/*
static int sysreadstring(char * path, char* buf, int len)
{
	int fd, res;

	fd = open(path, O_RDONLY);
	if (fd < 0) return fd;
	res = read(fd, buf, len);
	close(fd);
	return res;
}
*/
static int syswritestring(char * path, char* buf, int len)
{
	int fd, res;

	fd = open(path, O_WRONLY);
	if (fd < 0) return fd;
	res = write(fd, buf, len);
	close(fd);
	return res;
}

static int findi2cdev(int phandle)
{
	DIR *dirpos;
	struct dirent* entry;
	char path[SIZEOFPATH+64];

	if ((dirpos = opendir("/sys/bus/i2c/devices")) == NULL) return -1;
	while ((entry = readdir(dirpos)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		snprintf(path, SIZEOFPATH+64 -1, "/sys/bus/i2c/devices/%s/of_node/phandle", entry->d_name);
		if (sysreadbe32(path) == phandle) {
			snprintf(i2cdevname, SIZEOFPATH+16, "/dev/%s", entry->d_name);
			closedir(dirpos);
			return 0;
		}
	}
	closedir(dirpos);
	return -1;
}


int main(int argc, char *argv[])
{
	char *end;
	int busaddr, devad, dregister, value;
	int opt, res = 0, verify = 0;
	const char *password = NULL;
	const char *vendorname = NULL;
	const char *vendorpn = NULL;
	const char *extcc = NULL;
	uint32_t password_hex = 0;
	int extcc_hex = 0;
	char path[SIZEOFPATH];

	while ((opt = getopt(argc, argv, "p:V:N:E:vh")) != -1) {
		switch (opt) {
		case 'p': password = optarg;   break;
		case 'V': vendorname = optarg; break;
		case 'N': vendorpn = optarg;   break;
		case 'E': extcc = optarg;      break;
		case 'v': verify = 1; break;
		case 'h':
		case '?': help(); exit(opt == '?');
		}
	}



	if (password) {
		password_hex = strtol(password, &end, 0) & 0xffffffff;
		if (*end) exithelp("Error: Password is not a number!\n");
	}
	if (extcc) {
		extcc_hex = strtol(extcc, &end, 0) & 0xff;
		if (*end) exithelp("Error: Ext CC is not a number!\n");
	}

	if (argc < optind + 2) exithelp("Error: Not enough arguments!!\n");


	if (argv[optind][0] == '/') {
		i2cname = argv[optind];
	} else {
		snprintf(path, SIZEOFPATH-1, "/sys/devices/platform/%s/of_node/i2c-bus", argv[optind]);
		findi2cdev(sysreadbe32(path));
		i2cname = i2cdevname;
		syswritestring("/sys/bus/platform/drivers/sfp/unbind", argv[optind], strlen(argv[optind]));
	}

	file = open(i2cname, O_RDWR);
	if (file < 0) {
		fprintf(stderr, "Error: Could not open file "
				"`%s': %s\n", argv[optind], strerror(errno));
		exit(1);
	}

	if (!strcmp(argv[optind+1], "byte")) {

		if (argc < optind + 5) exithelp("Error: Not enough arguments!!\n");

		busaddr = strtol(argv[optind+3], &end, 0);
		if (*end || busaddr < 0) exithelp("Error: bus address is not a number!\n");

		dregister = strtol(argv[optind+4], &end, 0);
		if (*end || dregister < 0 || dregister > 0xff) exithelp("Error: dregister invalid!\n");

		if (argv[optind+2][0] == 'r') {
			res = i2c_read_byte(file, busaddr, dregister);
			if (res < 0) fprintf(stderr, "Error: i2c_read_byte failed\n");
			else printf("0x%02x\n", res);
		} else if (argv[optind+2][0] == 'w') {
			if (argc < optind + 6) exithelp("Error: Not enough arguments!!\n");

			value = strtol(argv[optind+5], &end, 0);
			if (*end || value < 0 || value > 0xff) exithelp("Error: value invalid!\n");

			res = i2c_write_byte(file, busaddr, dregister, value);
			if (res < 0) fprintf(stderr, "Error: i2c_write_byte failed\n");
		}
		if (verify) {
			res = i2c_read_byte(file, busaddr, dregister);
			if (res < 0) fprintf(stderr, "Error: i2c_read_byte failed\n");
			else if (res != value) printf("Warning - data mismatch - wrote "
					"0x%02X, read back 0x%02X\n", value, res);
			else printf("Value 0x%02X written, readback matched\n", value);
		}

	} else if (!strncmp(argv[optind+1], "c22", 3)) {

		if (argc < optind + 5) exithelp("Error: Not enough arguments!!\n");

		busaddr = strtol(argv[optind+3], &end, 0);
		if (*end || busaddr < 0) exithelp("Error: bus address is not a number!\n");

		dregister = strtol(argv[optind+4], &end, 0);
		if (*end || dregister < 0 || dregister > 0x1f) exithelp("Error: dregister invalid!\n");

		/* ROLLBALL 0x56 has slightly different implementation then MARVELL */
		if (argv[optind+1][3] == 'r') dregister = dregister << 1;

		if (argv[optind+2][0] == 'r') {
			res = i2c_mii_read_default_c22(file, busaddr, dregister);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_read_default_c22 failed\n");
			else printf("0x%04x\n", res);
		} else if (argv[optind+2][0] == 'w') {
			if (argc < optind + 6) exithelp("Error: Not enough arguments!!\n");

			value = strtol(argv[optind+5], &end, 0);
			if (*end || value < 0 || value > 0xffff) exithelp("Error: value invalid!\n");

			res = i2c_mii_write_default_c22(file, busaddr, dregister, value);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_write_default_c22 failed\n");
		}

	} else if (!strcmp(argv[optind+1], "c45")) {

		if (argc < optind + 6) exithelp("Error: Not enough arguments!!\n");

		busaddr = strtol(argv[optind+3], &end, 0);
		if (*end || busaddr < 0) exithelp("Error: bus address is not a number!\n");

		devad = strtol(argv[optind+4], &end, 0);
		if (*end || devad < 0 || devad > 0x1f) exithelp("Error: device address is not a number!\n");

		dregister = strtol(argv[optind+5], &end, 0);
		if (*end || dregister < 0 || dregister > 0xffff) exithelp("Error: dregister invalid!\n");

		if (argv[optind+2][0] == 'r') {
			res = i2c_mii_read_default_c45(file, busaddr, devad, dregister);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_read_default_c45 failed\n");
			else printf("0x%04x\n", res);
		} else if (argv[optind+2][0] == 'w') {
			if (argc < optind + 7) exithelp("Error: Not enough arguments!!\n");

			value = strtol(argv[optind+6], &end, 0);
			if (*end || value < 0 || value > 0xffff) exithelp("Error: value invalid!\n");

			res = i2c_mii_write_default_c45(file, busaddr, devad, dregister, value);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_write_default_c45 failed\n");
		}

	} else if (!strcmp(argv[optind+1], "rollball")) {

		if (argc < optind + 5) exithelp("Error: Not enough arguments!!\n");

		devad = strtol(argv[optind+3], &end, 0);
		if (*end || devad < 0 || devad > 0x1f) exithelp("Error: device address is not a number!\n");

		dregister = strtol(argv[optind+4], &end, 0);
		if (*end || dregister < 0 || dregister > 0xffff) exithelp("Error: dregister invalid!\n");

		res = i2c_mii_init_rollball(file);
		if (res < 0) fprintf(stderr, "Error: i2c_mii_init_rollball failed\n");

		if (argv[optind+2][0] == 'r') {
			res = i2c_mii_read_rollball(file, devad, dregister);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_read_rollball failed\n");
			else printf("0x%04x\n", res);
		} else if (argv[optind+2][0] == 'w') {
			if (argc < optind + 6) exithelp("Error: Not enough arguments!!\n");

			value = strtol(argv[optind+5], &end, 0);
			if (*end || value < 0 || value > 0xffff) exithelp("Error: value invalid!\n");

			res = i2c_mii_write_rollball(file, devad, dregister, value);
			if (res < 0) fprintf(stderr, "Error: i2c_mii_write_rollball failed\n");
		}

	} else if (!strcmp(argv[optind+1], "bruteforce")) {

		int min = 0x00, max = 0xff;

		if (argc >= optind + 4) {
			min = strtol(argv[optind+2], &end, 0);
			if (*end || min < 0 || min > 0xff) exithelp("Error: MIN invalid!\n");
			max = strtol(argv[optind+3], &end, 0);
			if (*end || max < 0 || max > 0xff) exithelp("Error: MAX invalid!\n");
		}
		if (!extcc) {
			res = runbruteforce(file, password_hex, min, max, bfread_1, bfwrite_1, bfmod_1);
		} else if (extcc_hex == 1) {
			res = runbruteforce(file, password_hex, min, max, bfread_1, bfwrite_1, bfmod_1);
		} else if (extcc_hex == 2) {
			res = runbruteforce(file, password_hex, min, max, bfread_2, bfwrite_2, bfmod_2);
		}
		if (res < 0) fprintf(stderr, "Error: bruteforce failed\n");

	} else if (!strcmp(argv[optind+1], "rbpassword")) {

		rbpassword(file, &password_hex);
		printf("RollBall Password used: 0x%08x\n", password_hex);

	} else if (!strcmp(argv[optind+1], "i2cdump")) {

		if (argc < optind + 3) exithelp("Error: Not enough arguments!!\n");

		busaddr = strtol(argv[optind+2], &end, 0);
		if (*end || busaddr < 0) exithelp("Error: bus address is not a number!\n");

		i2cdump(file, busaddr);

	} else if (!strcmp(argv[optind+1], "eepromdump")) {

		int lastpage = 3;

		if (argc >= optind + 3) {
			lastpage = strtol(argv[optind+2], &end, 0);
			if (*end || lastpage < 0 || lastpage > 0xff) exithelp("Error: LASTPAGE invalid!\n");
		}

		printeeprom(file, lastpage);

	} else if (!strcmp(argv[optind+1], "eepromfix")) {

		checksums(file, false);

		if (!password) {
			rbpassword(file, &password_hex);
			printf("RollBall Password used: 0x%08x\n", password_hex);
		}

		res = fillpassword(file, password_hex);
		if (res < 0) exiterror("Error: Cannot fill in password!\n");

		if (vendorname) {
			fillstring(file, vendorname, 20, 16);
			printf("Changed Vendor name to: %.16s\n", vendorname);

		}
		if (extcc) {
			i2c_write_byte(file, 0x50, 36, extcc_hex);
			usleep(EEPROMDELAY);
			printf("Changed EXT_CC to: 0x%02x\n", extcc_hex);
		}
		if (vendorpn) {
			fillstring(file, vendorpn, 40, 16);
			printf("Changed Vendor PN to: %.16s\n", vendorpn);
		}

		checksums(file, true);

		fillpassword(file, 0xffffffff);
	} else if (!strcmp(argv[optind+1], "restore")) {
		if (argv[optind][0] != '/') {
			syswritestring("/sys/bus/platform/drivers/sfp/bind", argv[optind], strlen(argv[optind]));
		}
	}

	if (file >= 0) close(file);
	exit(0);
}
