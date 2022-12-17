#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <usbi3c.h>

static int verbose;
static int help;
static uint16_t vendor_id = 0;
static uint16_t product_id = 0;

static int process_args(int, char **);
static void print_help(FILE *);
static void print_usage(FILE *, char *);
static int strhextous_err(const char *, uint16_t *);
static int strhextous_err_endptr(const char *, char **, uint16_t *);

int main(int argc, char *argv[])
{
	int rc = 1;
	struct usbi3c_context *i3c_ctx = NULL;
	struct usbi3c_device **i3c_devs = NULL;

	rc = process_args(argc, argv);

	if (help) {
		print_help(stdout);
		print_usage(stdout, argv[0]);
		goto EXIT;
	} else if (rc != 0) {
		print_usage(stderr, argv[0]);
		goto EXIT;
	}

	/* Initialize the library */
	rc = 1;
	i3c_ctx = usbi3c_init();
	if (!i3c_ctx) {
		goto EXIT;
	}

	/*
	 * Search for a potential I3C host based on given criteria
	 */
	printf("Looking for a matching device\n");
	printf("----------------------------\n");
	printf("Device class: 0x%02X (USB I3C device class)\n", USBI3C_DeviceClass);
	if (vendor_id) {
		printf("Vendor ID: 0x%04X\n", vendor_id);
	} else {
		printf("Any vendor ID\n");
	}
	if (product_id) {
		printf("Product ID: 0x%04X\n", product_id);
	} else {
		printf("Any product ID\n");
	}
	printf("============================\n");
	/* Find and select the device for this library */
	if (usbi3c_get_devices(i3c_ctx, vendor_id, product_id, &i3c_devs) <= 0) {
		fprintf(stderr, "Couldn't select a device with the specified criteria\n");
		goto FREE_AND_EXIT_1;
	}
	printf("Selected this device\n");

	printf("Initializing I3C bus\n");
	rc = usbi3c_initialize_device(i3c_devs[0]);
	if (rc < 0) {
		fprintf(stderr, "Failed to initialize I3C bus\n");
		goto FREE_AND_EXIT;
	}

	printf("Attempting to retrieve target device table\n");
	uint8_t *target_table = NULL;
	int num_targets = 0;
	rc = 1;

	num_targets = usbi3c_get_address_list(i3c_devs[0], &target_table);
	if (num_targets < 0) {
		fprintf(stderr, "Failed to retrieve target table: %d\n",
			num_targets);
		goto FREE_AND_EXIT;
	} else if (0 == num_targets) {
		printf("No target devices found\n");
	} else if (NULL == target_table) {
		fprintf(stderr, "target table pointer is NULL!\n");
		goto FREE_AND_EXIT;
	} else {
		printf("Found %d targets\n", num_targets);
	}

	for (int i = 0; i < num_targets; i++) {
		uint8_t d = target_table[i];
		printf("Device %d:\n", i);
		printf("\tAddress: %d\n", d);
		printf("\tTarget Type: 0x%0X\n", usbi3c_get_target_type(i3c_devs[0], d));
		printf("\tBCR: 0x%02X\n", usbi3c_get_target_BCR(i3c_devs[0], d));
		printf("\tDCR: 0x%02X\n", usbi3c_get_target_DCR(i3c_devs[0], d));
	}

	/*
	 * Remember to clean up the malloc'ed
	 * target_table list afterward.
	 */
	free(target_table);

	rc = 0;

	/* Cleanup */
FREE_AND_EXIT:
	while (*i3c_devs) {
		struct usbi3c_device *i3c_dev = *i3c_devs;
		usbi3c_device_deinit(&i3c_dev);
		i3c_devs++;
	}
FREE_AND_EXIT_1:
	usbi3c_deinit(&i3c_ctx);
EXIT:
	return rc;
}

/* Handle command-line arguments */
static int process_args(int argc, char *argv[])
{
	int rc = 0;

	while (1) {
		int c;
		static struct option long_options[] = {
			{ "help", no_argument, &help, 1 },
			{ "verbose", no_argument, &verbose, 1 },
			{ "quiet", no_argument, &verbose, 0 },
			{ "vid", required_argument, 0, 'v' },
			{ "vendorid", required_argument, 0, 'v' },
			{ "vendor-id", required_argument, 0, 'v' },
			{ "vendor_id", required_argument, 0, 'v' },
			{ "pid", required_argument, 0, 'p' },
			{ "productid", required_argument, 0, 'p' },
			{ "product-id", required_argument, 0, 'p' },
			{ "product_id", required_argument, 0, 'p' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;

		c = getopt_long(argc, argv, "hp:v:", long_options,
				&option_index);

		if (-1 == c) {
			break;
		}

		switch (c) {
		case 0:
			if (long_options[option_index].flag != 0) {
				break;
			}
			printf("option %s", long_options[option_index].name);
			if (optarg) {
				printf(" with arg %s", optarg);
			}
			printf("\n");
			break;
		case 'h':
			help = 1;
			break;
		case 'v':
			/* vendor id */
			rc = -1;
			if (optarg) {
				rc = strhextous_err(optarg, &vendor_id);
				if (rc < 0) {
					fprintf(stderr, "Invalid vendor ID argument: %s\n\n", optarg);
				}
			}
			break;
		case 'p':
			/* product id */
			rc = -1;
			if (optarg) {
				rc = strhextous_err(optarg, &product_id);
				if (rc < 0) {
					fprintf(stderr, "Invalid product ID argument: %s\n\n", optarg);
				}
			}
			break;
		case '?':
			rc = -1;
			break;

		default:
			rc = -1;
		}
	}

	return rc;
}

static void print_help(FILE *stream)
{
	fprintf(stream, "Sample application demonstrating use of libusbi3c\n");
	fprintf(stream, "\n");

	return;
}

static void print_usage(FILE *stream, char *progname)
{
	fprintf(stream, "Usage: %s [OPTIONS] [CRITERIA]\n\n",
		progname);
	fprintf(stream, "OPTIONS:\n");
	fprintf(stream, "-h, --help\t\t\tPrint this message\n");
	fprintf(stream, "--verbose, --quiet\t\tEmit more/less information\n");
	fprintf(stream, "-i, --index=INDEX\t\t(numeric) Select INDEXth device found, after matching any other criteria\n");
	fprintf(stream, "\n");
	fprintf(stream, "CRITERIA\n");
	fprintf(stream, "- Limits the detected USB devices to those matching the specified criteria.\n");
	fprintf(stream, "- The Device Class of the USB device to search for is assumed to be 0x%02X (USB I3C Device Class).\n", USBI3C_DeviceClass);
	fprintf(stream, "- most expect a hex string\n");
	fprintf(stream, "\n");
	fprintf(stream, "-v, --vendor-id=VENDORID\tVendor ID (idVendor, 16 bits)\n");
	fprintf(stream, "-p, --product-id=PRODUCTID\tProduct ID (idProduct, 16 bits)\n");

	return;
}

static int strhextous_err_endptr(const char *str, char **endptr, uint16_t *value)
{
	long num;
	int err;

	errno = 0;
	num = strtol(str, endptr, 16);
	err = -errno;

	/* When the return value of strtol overflows the int type, don't overflow
	 * and return an overflow error code. */
	if (num > USHRT_MAX) {
		num = USHRT_MAX;
		err = -ERANGE;
	} else if (num < 0) {
		num = 0;
		err = -ERANGE;
	}

	*value = (uint16_t)num;

	return err;
}

static int strhextous_err(const char *str, uint16_t *value)
{
	char *endptr;
	int err = strhextous_err_endptr(str, &endptr, value);

	if (err) {
		return err;
	}

	if (*endptr != '\0' && !isspace(*endptr)) {
		return -EINVAL;
	}

	return 0;
}
