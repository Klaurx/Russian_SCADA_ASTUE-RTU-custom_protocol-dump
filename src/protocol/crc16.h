/*
 * crc16.h
 *
 * CRC algorithm interface for USOTM and Modbus frame validation.
 * See crc16.c for full reconstruction notes and confidence levels.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

uint16_t crc16_modbus(const uint8_t *buf, size_t len);
uint16_t crc16_ccitt(const uint8_t *buf, size_t len);

int usotm_frame_check(const uint8_t *buf, uint16_t len,
                      uint16_t (*crc_fn)(const uint8_t *, size_t));
