/*-
 * Copyright (C) 2010, Romain Tartiere, Romuald Conty.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 * $Id$
 */

#ifndef __FREEFARE_INTERNAL_H__
#define __FREEFARE_INTERNAL_H__

#include "config.h"

#include <openssl/des.h>

/*
 * Endienness macros
 *
 * POSIX does not describe any API for endianness problems, and solutions are
 * mostly vendor-dependant.  Some operating systems provide a complete
 * framework for this (FreeBSD, OpenBSD), some provide nothing in the base
 * system (Mac OS), GNU/Linux systems may or may not provide macros to do the
 * conversion depending on the version of the libc.
 *
 * This is a PITA but unfortunately we have no other solution than doing all
 * this gymnastic.  Depending of what is defined if one or more of endian.h,
 * sys/endian.h and byteswap.h was included, define a set of macros to stick to
 * the set of macros provided by FreeBSD (it's a historic choice: development
 * was done on this operating system when endianness problems first had to be
 * dealt with).
 */

#if !defined(le32toh) && defined(bswap_32)
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define be32toh(x) bswap_32(x)
#    define htobe32(x) bswap_32(x)
#    define le32toh(x) (x)
#    define htole32(x) (x)
#  else
#    define be32toh(x) (x)
#    define htobe32(x) (x)
#    define le32toh(x) bswap_32(x)
#    define htole32(x) bswap_32(x)
#  endif
#endif

#if !defined(htole16) && defined(bswap_16)
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define be16toh(x) (bswap_16(x))
#    define htobe16(x) (bswap_16(x))
#    define htole16(x) (x)
#    define le16toh(x) (x)
#  else
#    define be16toh(x) (x)
#    define htobe16(x) (x)
#    define htole16(x) (bswap_16(x))
#    define le16toh(x) (bswap_16(x))
#  endif
#endif

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

struct mad_sector_0x00;
struct mad_sector_0x10;

void		 nxp_crc (uint8_t *crc, const uint8_t value);
MifareTag	 mifare_classic_tag_new (void);
void		 mifare_classic_tag_free (MifareTag tag);
MifareTag	 mifare_desfire_tag_new (void);
void		 mifare_desfire_tag_free (MifareTag tags);
MifareTag	 mifare_ultralight_tag_new (void);
void		 mifare_ultralight_tag_free (MifareTag tag);
uint8_t		 sector_0x00_crc8 (Mad mad);
uint8_t		 sector_0x10_crc8 (Mad mad);

typedef enum {
    MD_SEND,
    MD_RECEIVE
} MifareDirection;

void		*mifare_cryto_preprocess_data (MifareTag tag, void *data, size_t *nbytes, int communication_settings);
void		*mifare_cryto_postprocess_data (MifareTag tag, void *data, ssize_t *nbytes, int communication_settings);
void		 mifare_cbc_des (MifareDESFireKey key, uint8_t *data, size_t data_size, MifareDirection direction, int mac);
void		 rol8(uint8_t *data);
void		*assert_crypto_buffer_size (MifareTag tag, size_t nbytes);

#define MIFARE_ULTRALIGHT_PAGE_COUNT 16

struct supported_tag {
    uint8_t ATQA[2], SAK;
    enum mifare_tag_type type;
    const char *friendly_name;
};

/*
 * This structure is common to all supported MIFARE targets but shall not be
 * used directly (it's some kind of abstract class).  All members in this
 * structure are initialized by freefare_get_tags().
 *
 * Extra members in derived classes are initialized in the correpsonding
 * mifare_*_connect() function.
 */
struct mifare_tag {
    nfc_device_t *device;
    nfc_iso14443a_info_t info;
    const struct supported_tag *tag_info;
    int active;
};

struct mifare_classic_tag {
    struct mifare_tag __tag;

    MifareClassicKeyType last_authentication_key_type;

    /*
     * The following block numbers are on 2 bytes in order to use invalid
     * address and avoid false cache hit with inconsistent data.
     */
    struct {
      int16_t sector_trailer_block_number;
      uint16_t sector_access_bits;
      int16_t block_number;
      uint8_t block_access_bits;
    } cached_access_bits;
};

struct mifare_desfire_aid {
    uint8_t data[3];
};

struct mifare_desfire_key {
    uint8_t data[16];
    enum {
	T_DES,
	T_3DES
    } type;
    DES_key_schedule ks1;
    DES_key_schedule ks2;
};

struct mifare_desfire_tag {
    struct mifare_tag __tag;

    uint8_t last_picc_error;
    char *last_pcd_error;
    MifareDESFireKey session_key;
    uint8_t authenticated_key_no;
    uint8_t *crypto_buffer;
    size_t crypto_buffer_size;
};

MifareDESFireKey mifare_desfire_session_key_new (uint8_t rnda[8], uint8_t rndb[8], MifareDESFireKey authentication_key);

struct mifare_ultralight_tag {
    struct mifare_tag __tag;

    /* mifare_ultralight_read() reads 4 pages at a time (wrapping) */
    MifareUltralightPage cache[MIFARE_ULTRALIGHT_PAGE_COUNT + 3];
    uint8_t cached_pages[MIFARE_ULTRALIGHT_PAGE_COUNT];
};

/*
 * MifareTag assertion macros
 *
 * This macros provide a simple and unified way to perform various tests at the
 * beginning of the different targets functions.
 */
#define ASSERT_ACTIVE(tag) do { if (!tag->active) return errno = ENXIO, -1; } while (0)
#define ASSERT_INACTIVE(tag) do { if (tag->active) return errno = ENXIO, -1; } while (0)

#define ASSERT_MIFARE_CLASSIC(tag) do { if ((tag->tag_info->type != CLASSIC_1K) && (tag->tag_info->type != CLASSIC_4K)) return errno = ENODEV, -1; } while (0)
#define ASSERT_MIFARE_DESFIRE(tag) do { if (tag->tag_info->type != DESFIRE_4K) return errno = ENODEV, -1; } while (0)
#define ASSERT_MIFARE_ULTRALIGHT(tag) do { if (tag->tag_info->type != ULTRALIGHT) return errno = ENODEV, -1; } while (0)

/* 
 * MifareTag cast macros 
 *
 * This macros are intended to provide a convenient way to cast abstract
 * MifareTag structures to concrete Tags (e.g. MIFARE Classic tag).
 */
#define MIFARE_CLASSIC(tag) ((struct mifare_classic_tag *) tag)
#define MIFARE_DESFIRE(tag) ((struct mifare_desfire_tag *) tag)
#define MIFARE_ULTRALIGHT(tag) ((struct mifare_ultralight_tag *) tag)

/*
 * Access bits manipulation macros
 */
#define DB_AB(ab) ((ab == C_DEFAULT) ? C_000 : ab)
#define TB_AB(ab) ((ab == C_DEFAULT) ? C_100 : ab)

#endif /* !__FREEFARE_INTERNAL_H__ */
