
#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/spi/spidev.h>
#include <pam.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>

#include "texture.h"

#define SPI_PACKET_IMAGE 1
#define SPI_PACKET_FLIP  2
#define SPI_PACKET_GAMMA 3
#define SPI_PACKET_BRITE 4
#define SPI_PACKET_TESTP 5

struct spi_packet_hdr {
	uint8_t type;
	uint8_t flip;
	union {
		struct {
			uint16_t top, left;
			uint16_t width, height;
		};
		struct {
			uint16_t arguments[4];
		};
	};
};

int draw_rect(int fd, unsigned int x, unsigned int y, unsigned int w,
	      unsigned int h, char *data, unsigned int stride, bool flip)
{
	int ret = 1, i;
	static struct spi_packet_hdr hdr = {
		.type = SPI_PACKET_IMAGE,
		.flip = 1,
	};
	static struct spi_ioc_transfer xfers[32] = {
		{
			.len = sizeof(hdr),
			.speed_hz = 0,
			.delay_usecs = 0,
			.cs_change = 0,
			.tx_nbits = 8,
			.rx_nbits = 8,
		},
		{
			.speed_hz = 0,
			.delay_usecs = 0,
			.cs_change = 0,
			.tx_nbits = 8,
			.rx_nbits = 8,
		},
	};

	hdr.flip = flip;
	hdr.top = y;
	hdr.left = x;
	hdr.width = w;
	hdr.height = h;

	xfers[0].tx_buf = (uint64_t)((uintptr_t)&hdr);
	for (i = 0; i < h ; i++) {
		xfers[i + 1].tx_buf = (uint64_t)((uintptr_t)(data + (stride * i)));
		xfers[i + 1].len = w * 4;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(h + 1), xfers);
	if (ret <= 0) {
		fprintf(stderr, "ioctl() error: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}
