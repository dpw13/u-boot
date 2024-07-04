#include <common.h>

DECLARE_GLOBAL_DATA_PTR;

static int do_oem2env(cmd_tbl_t *cmdtp, int flag,
			int argc, char *const argv[])
{
	unsigned long loadaddr = env_get_ulong("loadaddr", 16, 0);
	unsigned char *p = (unsigned char *)loadaddr;
	unsigned int offset = 0;
	char commands[1024];
	char key[32];
	char buf[32];
	int slot = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	slot = simple_strtoul(argv[1], NULL, 10);
	if ((slot < 0) || (slot >= 4))
		return CMD_RET_USAGE;

	snprintf(commands, sizeof(commands),
			"sf probe 4:0;"
			"sf secure on;"
			"sf read 0x%08lx 0x%08x 0x%08x;"
			"sf secure off",
			loadaddr,
			offset + slot * 16,
			16);

	run_command_list(commands, -1, 0);

	snprintf(buf, sizeof(buf),
			/* 9999999999999999 */
			"%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
			p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
			p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);

	if (argc >= 3) {
		env_set(argv[2], buf);
	} else {
		snprintf(key, sizeof(key), "oem%d", slot);
		env_set(key, buf);
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	oem2env, 3, 0, do_oem2env,
	"Set OEM string to environment",
	"<slot> <key>"
);
