/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <errno.h>
#include <getopt.h>
#include <gpiod.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_DELAY_MS 500U

static volatile sig_atomic_t stop_requested;

static void handle_signal(int signal_number)
{
	(void)signal_number;
	stop_requested = 1;
}

static void print_usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [-l] [-d delay-ms] <gpiochip> <line-offset>\n"
		"\n"
		"  -d delay-ms  Time each LED state is held (default: %u ms)\n"
		"  -l           Treat the GPIO line as active-low\n"
		"  -h           Show this help\n",
		program, DEFAULT_DELAY_MS);
}

static int parse_unsigned(const char *text, unsigned int *value)
{
	char *end;
	unsigned long parsed;

	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno != 0 || end == text || *end != '\0' || parsed > UINT_MAX)
		return -1;

	*value = (unsigned int)parsed;
	return 0;
}

static int sleep_for_ms(unsigned int delay_ms)
{
	struct timespec remaining = {
		.tv_sec = delay_ms / 1000U,
		.tv_nsec = (long)(delay_ms % 1000U) * 1000000L,
	};

	while (nanosleep(&remaining, &remaining) < 0) {
		if (errno != EINTR)
			return -1;
		if (stop_requested)
			return 0;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct gpiod_chip *chip = NULL;
	struct gpiod_line *line = NULL;
	struct sigaction action = {
		.sa_handler = handle_signal,
	};
	const char *chip_path;
	unsigned int line_offset;
	unsigned int delay_ms = DEFAULT_DELAY_MS;
	int active_low = 0;
	int request_flags = 0;
	int option;
	int value = 0;
	int exit_status = EXIT_FAILURE;

	while ((option = getopt(argc, argv, "d:lh")) != -1) {
		switch (option) {
		case 'd':
			if (parse_unsigned(optarg, &delay_ms) < 0 ||
			    delay_ms == 0U) {
				fprintf(stderr, "Invalid delay: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'l':
			active_low = 1;
			break;
		case 'h':
			print_usage(argv[0]);
			return EXIT_SUCCESS;
		default:
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (argc - optind != 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	chip_path = argv[optind];
	if (parse_unsigned(argv[optind + 1], &line_offset) < 0) {
		fprintf(stderr, "Invalid line offset: %s\n", argv[optind + 1]);
		return EXIT_FAILURE;
	}

	chip = gpiod_chip_open(chip_path);
	if (chip == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n",
			chip_path, strerror(errno));
		goto out;
	}

	line = gpiod_chip_get_line(chip, line_offset);
	if (line == NULL) {
		fprintf(stderr, "Failed to get line %u from %s: %s\n",
			line_offset, chip_path, strerror(errno));
		goto out;
	}

	if (active_low)
		request_flags |= GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW;

	if (gpiod_line_request_output_flags(line, "esp-linux-blink",
					    request_flags, 0) < 0) {
		fprintf(stderr, "Failed to request line %u: %s\n",
			line_offset, strerror(errno));
		goto out;
	}

	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) < 0 ||
	    sigaction(SIGTERM, &action, NULL) < 0) {
		fprintf(stderr, "Failed to install signal handler: %s\n",
			strerror(errno));
		goto out;
	}

	printf("Blinking %s line %u every %u ms%s. Press Ctrl+C to stop.\n",
	       chip_path, line_offset, delay_ms,
	       active_low ? " (active-low)" : "");

	while (!stop_requested) {
		value = !value;
		if (gpiod_line_set_value(line, value) < 0) {
			fprintf(stderr, "Failed to set line value: %s\n",
				strerror(errno));
			goto out;
		}

		printf("LED %s\n", value ? "ON" : "OFF");
		fflush(stdout);

		if (sleep_for_ms(delay_ms) < 0) {
			fprintf(stderr, "Failed while waiting: %s\n",
				strerror(errno));
			goto out;
		}
	}

	exit_status = EXIT_SUCCESS;

out:
	if (line != NULL && gpiod_line_is_requested(line)) {
		if (gpiod_line_set_value(line, 0) < 0)
			fprintf(stderr, "Failed to turn the LED off: %s\n",
				strerror(errno));
		gpiod_line_release(line);
	}
	if (chip != NULL)
		gpiod_chip_close(chip);

	return exit_status;
}
