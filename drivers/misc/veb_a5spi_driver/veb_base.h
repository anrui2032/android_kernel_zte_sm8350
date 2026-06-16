#ifndef __VEB_BASE_H__
#define __VEB_BASE_H__

#include <linux/spi/spi.h>

#include "veb_a5.h"

extern int check_sw(unsigned char *sw);

extern int veb_spi_recv(struct spi_device *spi, char *rx, int length);
extern int veb_spi_send(struct spi_device *spi, char *tx, int length);
extern int veb_spi_duplex(struct spi_device *spi, char *tx, char *rx, int length);

extern int veb_cmd_read(struct spi_device *, cmd_t *, char *, int);
extern int veb_cmd_write(struct spi_device *, cmd_t *, unsigned char *, int);
extern int veb_cmd_config(struct spi_device *, cmd_t *);
extern int veb_cmd_exchange(struct spi_device *, cmd_t *, unsigned char *, unsigned int, unsigned char *, unsigned int);
extern int veb_sym_crypto(struct spi_device *, cmd_t *, unsigned char *, unsigned char *, int);
extern int veb_sym_duplex_crypto(struct spi_device *, cmd_t *, unsigned char *, unsigned char *, int);
extern int veb_sym_withkey_cmd_write(struct spi_device *, cmd_t *, unsigned char *, veb_key_info *,
		unsigned char *, int);

#endif
