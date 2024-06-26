/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "mbfilter_singlebyte.h"

#define CK(statement) do { if ((statement) < 0) return (-1); } while (0)

static inline uint32_t coalesce(uint32_t a, uint32_t b)
{
	return a ? a : b;
}

/* Helper for single-byte encodings which use a conversion table */
static int mbfl_conv_singlebyte_table(int c, mbfl_convert_filter *filter, int tbl_min, const unsigned short tbl[])
{
	if (c >= 0 && c < tbl_min) {
		CK((*filter->output_function)(c, filter->data));
	} else if (c < 0) {
		CK((*filter->output_function)(MBFL_BAD_INPUT, filter->data));
	} else {
		CK((*filter->output_function)(coalesce(tbl[c - tbl_min], MBFL_BAD_INPUT), filter->data));
	}
	return 0;
}

static int mbfl_conv_reverselookup_table(int c, mbfl_convert_filter *filter, int tbl_min, const unsigned short tbl[])
{
	if (c >= 0 && c < tbl_min) {
		CK((*filter->output_function)(c, filter->data));
	} else if (c < 0 || c == MBFL_BAD_INPUT) {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	} else {
		for (int i = 0; i < 256 - tbl_min; i++) {
			if (c == tbl[i]) {
				CK((*filter->output_function)(i + tbl_min, filter->data));
				return 0;
			}
		}
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}
	return 0;
}

/* Initialize data structures for a single-byte encoding */
#define DEF_SB(id, name, mime_name, aliases) \
	static int mbfl_filt_conv_##id##_wchar(int c, mbfl_convert_filter *filter); \
	static int mbfl_filt_conv_wchar_##id(int c, mbfl_convert_filter *filter); \
	static size_t mb_##id##_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state); \
	static void mb_wchar_to_##id(uint32_t *in, size_t len, mb_convert_buf *buf, bool end); \
	static const struct mbfl_convert_vtbl vtbl_##id##_wchar = { \
		mbfl_no_encoding_##id, \
		mbfl_no_encoding_wchar, \
		mbfl_filt_conv_common_ctor, \
		NULL, \
		mbfl_filt_conv_##id##_wchar, \
		mbfl_filt_conv_common_flush, \
		NULL \
	}; \
	static const struct mbfl_convert_vtbl vtbl_wchar_##id = { \
		mbfl_no_encoding_wchar, \
		mbfl_no_encoding_##id, \
		mbfl_filt_conv_common_ctor, \
		NULL, \
		mbfl_filt_conv_wchar_##id, \
		mbfl_filt_conv_common_flush, \
		NULL \
	}; \
	const mbfl_encoding mbfl_encoding_##id = { \
		mbfl_no_encoding_##id, \
		name, \
		mime_name, \
		aliases, \
		NULL, \
		MBFL_ENCTYPE_SBCS, \
		&vtbl_##id##_wchar, \
		&vtbl_wchar_##id, \
		mb_##id##_to_wchar, \
		mb_wchar_to_##id \
	}

/* For single-byte encodings which use a conversion table */
#define DEF_SB_TBL(id, name, mime_name, aliases, tbl_min, tbl) \
	static int mbfl_filt_conv_##id##_wchar(int c, mbfl_convert_filter *filter) { \
		return mbfl_conv_singlebyte_table(c, filter, tbl_min, tbl); \
	} \
	static int mbfl_filt_conv_wchar_##id(int c, mbfl_convert_filter *filter) { \
		return mbfl_conv_reverselookup_table(c, filter, tbl_min, tbl); \
	} \
	static size_t mb_##id##_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state) \
	{ \
		unsigned char *p = *in, *e = p + *in_len; \
		uint32_t *out = buf, *limit = buf + bufsize; \
		while (p < e && out < limit) { \
			unsigned char c = *p++; \
			*out++ = (c < tbl_min) ? c : coalesce(tbl[c - tbl_min], MBFL_BAD_INPUT); \
		} \
		*in_len = e - p; \
		*in = p; \
		return out - buf; \
	} \
	static void mb_wchar_to_##id(uint32_t *in, size_t len, mb_convert_buf *buf, bool end) \
	{ \
		unsigned char *out, *limit; \
		MB_CONVERT_BUF_LOAD(buf, out, limit); \
		MB_CONVERT_BUF_ENSURE(buf, out, limit, len); \
		while (len--) { \
			uint32_t w = *in++; \
			if (w < tbl_min) { \
				out = mb_convert_buf_add(out, w & 0xFF); \
			} else { \
				for (int i = 0; i < 256 - tbl_min; i++) { \
					if (w == tbl[i]) { \
						out = mb_convert_buf_add(out, i + tbl_min); \
						goto next_iteration; \
					} \
				} \
				MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_##id); \
				MB_CONVERT_BUF_ENSURE(buf, out, limit, len); \
	next_iteration: ; \
			} \
		} \
		MB_CONVERT_BUF_STORE(buf, out, limit); \
	} \
	DEF_SB(id, name, mime_name, aliases)

/* The grand-daddy of them all: ASCII */
static const char *ascii_aliases[] = {"ANSI_X3.4-1968", "iso-ir-6", "ANSI_X3.4-1986", "ISO_646.irv:1991", "US-ASCII", "ISO646-US", "us", "IBM367", "IBM-367", "cp367", "csASCII", NULL};
DEF_SB(ascii, "ASCII", "US-ASCII", ascii_aliases);

static int mbfl_filt_conv_ascii_wchar(int c, mbfl_convert_filter *filter)
{
	CK((*filter->output_function)((c < 0x80) ? c : MBFL_BAD_INPUT, filter->data));
	return 0;
}

static int mbfl_filt_conv_wchar_ascii(int c, mbfl_convert_filter *filter)
{
	if (c >= 0 && c < 0x80 && c != MBFL_BAD_INPUT) {
		CK((*filter->output_function)(c, filter->data));
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}
	return 0;
}

static size_t mb_ascii_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state)
{
	unsigned char *p = *in, *e = p + *in_len;
	uint32_t *out = buf, *limit = buf + bufsize;

	while (p < e && out < limit) {
		unsigned char c = *p++;
		*out++ = (c < 0x80) ? c : MBFL_BAD_INPUT;
	}

	*in_len = e - p;
	*in = p;
	return out - buf;
}

static void mb_wchar_to_ascii(uint32_t *in, size_t len, mb_convert_buf *buf, bool end)
{
	unsigned char *out, *limit;
	MB_CONVERT_BUF_LOAD(buf, out, limit);
	MB_CONVERT_BUF_ENSURE(buf, out, limit, len);

	while (len--) {
		uint32_t w = *in++;
		if (w < 0x80) {
			out = mb_convert_buf_add(out, w & 0xFF);
		} else {
			MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_ascii);
			MB_CONVERT_BUF_ENSURE(buf, out, limit, len);
		}
	}

	MB_CONVERT_BUF_STORE(buf, out, limit);
}

/* ISO-8859-X */

static const char *iso8859_1_aliases[] = {"ISO8859-1", "latin1", NULL};
DEF_SB(8859_1, "ISO-8859-1", "ISO-8859-1", iso8859_1_aliases);

static int mbfl_filt_conv_8859_1_wchar(int c, mbfl_convert_filter *filter)
{
	return (*filter->output_function)(c, filter->data);
}

static int mbfl_filt_conv_wchar_8859_1(int c, mbfl_convert_filter *filter)
{
	if (c >= 0 && c < 0x100 && c != MBFL_BAD_INPUT) {
		CK((*filter->output_function)(c, filter->data));
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}
	return 0;
}

static size_t mb_8859_1_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state)
{
	unsigned char *p = *in, *e = p + *in_len;
	uint32_t *out = buf, *limit = buf + bufsize;

	while (p < e && out < limit) {
		*out++ = *p++;
	}

	*in_len = e - p;
	*in = p;
	return out - buf;
}

static void mb_wchar_to_8859_1(uint32_t *in, size_t len, mb_convert_buf *buf, bool end)
{
	unsigned char *out, *limit;
	MB_CONVERT_BUF_LOAD(buf, out, limit);
	MB_CONVERT_BUF_ENSURE(buf, out, limit, len);

	while (len--) {
		uint32_t w = *in++;
		if (w < 0x100) {
			out = mb_convert_buf_add(out, w);
		} else {
			MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_8859_1);
			MB_CONVERT_BUF_ENSURE(buf, out, limit, len);
		}
	}

	MB_CONVERT_BUF_STORE(buf, out, limit);
}

static const char *iso8859_2_aliases[] = {"ISO8859-2", "latin2", NULL};
static const unsigned short iso8859_2_ucs_table[] = {
	0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7,
	0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
	0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7,
	0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
	0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
	0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
	0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
	0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
	0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
	0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
	0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
	0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9
};
DEF_SB_TBL(8859_2, "ISO-8859-2", "ISO-8859-2", iso8859_2_aliases, 0xA0, iso8859_2_ucs_table);

static const char *iso8859_3_aliases[] = {"ISO8859-3", "latin3", NULL};
static const unsigned short iso8859_3_ucs_table[] = {
	0x00A0, 0x0126, 0x02D8, 0x00A3, 0x00A4, 0x0000, 0x0124, 0x00A7,
	0x00A8, 0x0130, 0x015E, 0x011E, 0x0134, 0x00AD, 0x0000, 0x017B,
	0x00B0, 0x0127, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x0125, 0x00B7,
	0x00B8, 0x0131, 0x015F, 0x011F, 0x0135, 0x00BD, 0x0000, 0x017C,
	0x00C0, 0x00C1, 0x00C2, 0x0000, 0x00C4, 0x010A, 0x0108, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x0000, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x0120, 0x00D6, 0x00D7,
	0x011C, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x016C, 0x015C, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x0000, 0x00E4, 0x010B, 0x0109, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x0000, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x0121, 0x00F6, 0x00F7,
	0x011D, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x016D, 0x015D, 0x02D9
};
DEF_SB_TBL(8859_3, "ISO-8859-3", "ISO-8859-3", iso8859_3_aliases, 0xA0, iso8859_3_ucs_table);

static const char *iso8859_4_aliases[] = {"ISO8859-4", "latin4", NULL};
static const unsigned short iso8859_4_ucs_table[] = {
	0x00A0, 0x0104, 0x0138, 0x0156, 0x00A4, 0x0128, 0x013B, 0x00A7,
	0x00A8, 0x0160, 0x0112, 0x0122, 0x0166, 0x00AD, 0x017D, 0x00AF,
	0x00B0, 0x0105, 0x02DB, 0x0157, 0x00B4, 0x0129, 0x013C, 0x02C7,
	0x00B8, 0x0161, 0x0113, 0x0123, 0x0167, 0x014A, 0x017E, 0x014B,
	0x0100, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x012E,
	0x010C, 0x00C9, 0x0118, 0x00CB, 0x0116, 0x00CD, 0x00CE, 0x012A,
	0x0110, 0x0145, 0x014C, 0x0136, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x0172, 0x00DA, 0x00DB, 0x00DC, 0x0168, 0x016A, 0x00DF,
	0x0101, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x012F,
	0x010D, 0x00E9, 0x0119, 0x00EB, 0x0117, 0x00ED, 0x00EE, 0x012B,
	0x0111, 0x0146, 0x014D, 0x0137, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
	0x00F8, 0x0173, 0x00FA, 0x00FB, 0x00FC, 0x0169, 0x016B, 0x02D9
};
DEF_SB_TBL(8859_4, "ISO-8859-4", "ISO-8859-4", iso8859_4_aliases, 0xA0, iso8859_4_ucs_table);

static const char *iso8859_5_aliases[] = {"ISO8859-5", "cyrillic", NULL};
static const unsigned short iso8859_5_ucs_table[] = {
	0x00A0, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
	0x0408, 0x0409, 0x040A, 0x040B, 0x040C, 0x00AD, 0x040E, 0x040F,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
	0x2116, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457,
	0x0458, 0x0459, 0x045A, 0x045B, 0x045C, 0x00A7, 0x045E, 0x045F
};
DEF_SB_TBL(8859_5, "ISO-8859-5", "ISO-8859-5", iso8859_5_aliases, 0xA0, iso8859_5_ucs_table);

static const char *iso8859_6_aliases[] = {"ISO8859-6", "arabic", NULL};
static const unsigned short iso8859_6_ucs_table[] = {
	0x00A0, 0x0000, 0x0000, 0x0000, 0x00A4, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x060C, 0x00AD, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x061B, 0x0000, 0x0000, 0x0000, 0x061F,
	0x0000, 0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627,
	0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
	0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637,
	0x0638, 0x0639, 0x063A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647,
	0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F,
	0x0650, 0x0651, 0x0652, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};
DEF_SB_TBL(8859_6, "ISO-8859-6", "ISO-8859-6", iso8859_6_aliases, 0xA0, iso8859_6_ucs_table);

static const char *iso8859_7_aliases[] = {"ISO8859-7", "greek", NULL};
static const unsigned short iso8859_7_ucs_table[] = {
	0x00A0, 0x2018, 0x2019, 0x00A3, 0x20AC, 0x20AF, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x037A, 0x00AB, 0x00AC, 0x00AD, 0x0000, 0x2015,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0384, 0x0385, 0x0386, 0x00B7,
	0x0388, 0x0389, 0x038A, 0x00BB, 0x038C, 0x00BD, 0x038E, 0x038F,
	0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397,
	0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
	0x03A0, 0x03A1, 0x0000, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7,
	0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
	0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7,
	0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
	0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7,
	0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0x0000
};
DEF_SB_TBL(8859_7, "ISO-8859-7", "ISO-8859-7", iso8859_7_aliases, 0xA0, iso8859_7_ucs_table);

static const char *iso8859_8_aliases[] = {"ISO8859-8", "hebrew", NULL};
static const unsigned short iso8859_8_ucs_table[] = {
	0x00A0, 0x0000, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
	0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x2017,
	0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7,
	0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
	0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7,
	0x05E8, 0x05E9, 0x05EA, 0x0000, 0x0000, 0x200E, 0x200F, 0x0000
};
DEF_SB_TBL(8859_8, "ISO-8859-8", "ISO-8859-8", iso8859_8_aliases, 0xA0, iso8859_8_ucs_table);

static const char *iso8859_9_aliases[] = {"ISO8859-9", "latin5", NULL};
static const unsigned short iso8859_9_ucs_table[] = {
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
	0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x011E, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0130, 0x015E, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x011F, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
	0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0131, 0x015F, 0x00FF
};
DEF_SB_TBL(8859_9, "ISO-8859-9", "ISO-8859-9", iso8859_9_aliases, 0xA0, iso8859_9_ucs_table);

static const char *iso8859_10_aliases[] = {"ISO8859-10", "latin6", NULL};
static const unsigned short iso8859_10_ucs_table[] = {
	0x00A0, 0x0104, 0x0112, 0x0122, 0x012A, 0x0128, 0x0136, 0x00A7,
	0x013B, 0x0110, 0x0160, 0x0166, 0x017D, 0x00AD, 0x016A, 0x014A,
	0x00B0, 0x0105, 0x0113, 0x0123, 0x012B, 0x0129, 0x0137, 0x00B7,
	0x013C, 0x0111, 0x0161, 0x0167, 0x017E, 0x2015, 0x016B, 0x014B,
	0x0100, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x012E,
	0x010C, 0x00C9, 0x0118, 0x00CB, 0x0116, 0x00CD, 0x00CE, 0x00CF,
	0x00D0, 0x0145, 0x014C, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x0168,
	0x00D8, 0x0172, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
	0x0101, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x012F,
	0x010D, 0x00E9, 0x0119, 0x00EB, 0x0117, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x0146, 0x014D, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x0169,
	0x00F8, 0x0173, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x0138
};
DEF_SB_TBL(8859_10, "ISO-8859-10", "ISO-8859-10", iso8859_10_aliases, 0xA0, iso8859_10_ucs_table);

static const char *iso8859_13_aliases[] = {"ISO8859-13", NULL};
static const unsigned short iso8859_13_ucs_table[] = {
	0x00A0, 0x201D, 0x00A2, 0x00A3, 0x00A4, 0x201E, 0x00A6, 0x00A7,
	0x00D8, 0x00A9, 0x0156, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00C6,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x201C, 0x00B5, 0x00B6, 0x00B7,
	0x00F8, 0x00B9, 0x0157, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00E6,
	0x0104, 0x012E, 0x0100, 0x0106, 0x00C4, 0x00C5, 0x0118, 0x0112,
	0x010C, 0x00C9, 0x0179, 0x0116, 0x0122, 0x0136, 0x012A, 0x013B,
	0x0160, 0x0143, 0x0145, 0x00D3, 0x014C, 0x00D5, 0x00D6, 0x00D7,
	0x0172, 0x0141, 0x015A, 0x016A, 0x00DC, 0x017B, 0x017D, 0x00DF,
	0x0105, 0x012F, 0x0101, 0x0107, 0x00E4, 0x00E5, 0x0119, 0x0113,
	0x010D, 0x00E9, 0x017A, 0x0117, 0x0123, 0x0137, 0x012B, 0x013C,
	0x0161, 0x0144, 0x0146, 0x00F3, 0x014D, 0x00F5, 0x00F6, 0x00F7,
	0x0173, 0x0142, 0x015B, 0x016B, 0x00FC, 0x017C, 0x017E, 0x2019
};
DEF_SB_TBL(8859_13, "ISO-8859-13", "ISO-8859-13", iso8859_13_aliases, 0xA0, iso8859_13_ucs_table);

static const char *iso8859_14_aliases[] = {"ISO8859-14", "latin8", NULL};
static const unsigned short iso8859_14_ucs_table[] = {
	0x00A0, 0x1E02, 0x1E03, 0x00A3, 0x010A, 0x010B, 0x1E0A, 0x00A7,
	0x1E80, 0x00A9, 0x1E82, 0x1E0B, 0x1EF2, 0x00AD, 0x00AE, 0x0178,
	0x1E1E, 0x1E1F, 0x0120, 0x0121, 0x1E40, 0x1E41, 0x00B6, 0x1E56,
	0x1E81, 0x1E57, 0x1E83, 0x1E60, 0x1EF3, 0x1E84, 0x1E85, 0x1E61,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x0174, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x1E6A,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x0176, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x0175, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x1E6B,
	0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x0177, 0x00FF
};
DEF_SB_TBL(8859_14, "ISO-8859-14", "ISO-8859-14", iso8859_14_aliases, 0xA0, iso8859_14_ucs_table);

static const char *iso8859_15_aliases[] = {"ISO8859-15", NULL};
static const unsigned short iso8859_15_ucs_table[] = {
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x20AC, 0x00A5, 0x0160, 0x00A7,
	0x0161, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x017D, 0x00B5, 0x00B6, 0x00B7,
	0x017E, 0x00B9, 0x00BA, 0x00BB, 0x0152, 0x0153, 0x0178, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
	0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};
DEF_SB_TBL(8859_15, "ISO-8859-15", "ISO-8859-15", iso8859_15_aliases, 0xA0, iso8859_15_ucs_table);

static const char *iso8859_16_aliases[] = {"ISO8859-16", NULL};
static const unsigned short iso8859_16_ucs_table[] = {
	0x00A0, 0x0104, 0x0105, 0x0141, 0x20AC, 0x201E, 0x0160, 0x00A7,
	0x0161, 0x00A9, 0x0218, 0x00AB, 0x0179, 0x00AD, 0x017A, 0x017B,
	0x00B0, 0x00B1, 0x010C, 0x0142, 0x017D, 0x201D, 0x00B6, 0x00B7,
	0x017E, 0x010D, 0x0219, 0x00BB, 0x0152, 0x0153, 0x0178, 0x017C,
	0x00C0, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0106, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x0110, 0x0143, 0x00D2, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x015A,
	0x0170, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0118, 0x021A, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x0107, 0x00E6, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x0111, 0x0144, 0x00F2, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x015B,
	0x0171, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0119, 0x021B, 0x00FF
};
DEF_SB_TBL(8859_16, "ISO-8859-16", "ISO-8859-16", iso8859_16_aliases, 0xA0, iso8859_16_ucs_table);

static const char *cp1251_aliases[] = {"CP1251", "CP-1251", "WINDOWS-1251", NULL};
static const unsigned short cp1251_ucs_table[] = {
	0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
	0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
	0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x0000, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
	0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
	0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
	0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
	0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F
};
DEF_SB_TBL(cp1251, "Windows-1251", "Windows-1251", cp1251_aliases, 0x80, cp1251_ucs_table);

static const char *cp1252_aliases[] = {"cp1252", NULL};
static const unsigned short cp1252_ucs_table[] = {
	0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
	0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
};
DEF_SB(cp1252, "Windows-1252", "Windows-1252", cp1252_aliases);

static int mbfl_filt_conv_wchar_cp1252(int c, mbfl_convert_filter *filter)
{
	if (c < 0 || c == MBFL_BAD_INPUT) {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	} else if (c >= 0x100) {
		for (int n = 0; n < 32; n++) {
			if (c == cp1252_ucs_table[n]) {
				CK((*filter->output_function)(0x80 + n, filter->data));
				return 0;
			}
		}
		CK(mbfl_filt_conv_illegal_output(c, filter));
	} else if (c <= 0x7F || c >= 0xA0) {
		CK((*filter->output_function)(c, filter->data));
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}
	return 0;
}

static int mbfl_filt_conv_cp1252_wchar(int c, mbfl_convert_filter *filter)
{
	int s;
	if (c >= 0x80 && c < 0xA0) {
		s = coalesce(cp1252_ucs_table[c - 0x80], MBFL_BAD_INPUT);
	} else {
		s = c;
	}
	CK((*filter->output_function)(s, filter->data));
	return 0;
}

static size_t mb_cp1252_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state)
{
	unsigned char *p = *in, *e = p + *in_len;
	uint32_t *out = buf, *limit = buf + bufsize;

	while (p < e && out < limit) {
		unsigned char c = *p++;

		if (c >= 0x80 && c < 0xA0) {
			*out++ = coalesce(cp1252_ucs_table[c - 0x80], MBFL_BAD_INPUT);
		} else {
			*out++ = c;
		}
	}

	*in_len = e - p;
	*in = p;
	return out - buf;
}

static void mb_wchar_to_cp1252(uint32_t *in, size_t len, mb_convert_buf *buf, bool end)
{
	unsigned char *out, *limit;
	MB_CONVERT_BUF_LOAD(buf, out, limit);
	MB_CONVERT_BUF_ENSURE(buf, out, limit, len);

	while (len--) {
		uint32_t w = *in++;

		if (w >= 0x100) {
			for (int i = 0; i < 32; i++) {
				if (w == cp1252_ucs_table[i]) {
					out = mb_convert_buf_add(out, i + 0x80);
					goto continue_cp1252;
				}
			}
			MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_cp1252);
			MB_CONVERT_BUF_ENSURE(buf, out, limit, len);
		} else if (w <= 0x7F || w >= 0xA0) {
			out = mb_convert_buf_add(out, w);
		} else {
			MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_cp1252);
			MB_CONVERT_BUF_ENSURE(buf, out, limit, len);
		}
		continue_cp1252: ;
	}

	MB_CONVERT_BUF_STORE(buf, out, limit);
}

static const char *cp1254_aliases[] = {"CP1254", "CP-1254", "WINDOWS-1254", NULL};
static const unsigned short cp1254_ucs_table[] = {
	0x20AC, 0X0000, 0X201A, 0X0192, 0X201E, 0X2026, 0X2020, 0X2021,
	0X02C6, 0X2030, 0X0160, 0X2039, 0X0152, 0X0000, 0X0000, 0X0000,
	0X0000, 0X2018, 0X2019, 0X201C, 0X201D, 0X2022, 0X2013, 0X2014,
	0X02DC, 0X2122, 0X0161, 0X203A, 0X0153, 0X0000, 0X0000, 0X0178,
	0X00A0, 0X00A1, 0X00A2, 0X00A3, 0X00A4, 0X00A5, 0X00A6, 0X00A7,
	0X00A8, 0X00A9, 0X00AA, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00AF,
	0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X00B4, 0X00B5, 0X00B6, 0X00B7,
	0X00B8, 0X00B9, 0X00BA, 0X00BB, 0X00BC, 0X00BD, 0X00BE, 0X00BF,
	0X00C0, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X00C7,
	0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
	0X011E, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X00D7,
	0X00D8, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X0130, 0X015E, 0X00DF,
	0X00E0, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X00E7,
	0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
	0X011F, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X00F7,
	0X00F8, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X0131, 0X015F, 0X00FF
};
DEF_SB_TBL(cp1254, "Windows-1254", "Windows-1254", cp1254_aliases, 0x80, cp1254_ucs_table);

static const char *cp866_aliases[] = {"CP-866", "IBM866", "IBM-866", NULL};
static const unsigned short cp866_ucs_table[] = {
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
	0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
	0x0401, 0x0451, 0x0404, 0x0454, 0x0407, 0x0457, 0x040E, 0x045E,
	0x00B0, 0x2219, 0x00B7, 0x221A, 0x2116, 0x00A4, 0x25A0, 0x00A0
};
DEF_SB_TBL(cp866, "CP866", "CP866", cp866_aliases, 0x80, cp866_ucs_table);

static const char *cp850_aliases[] = {"CP-850", "IBM850", "IBM-850", NULL};
static const unsigned short cp850_ucs_table[] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
	0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
	0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
	0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0,
	0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3,
	0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
	0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x0131, 0x00CD, 0x00CE,
	0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
	0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
	0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
	0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8,
	0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0
};
DEF_SB_TBL(cp850, "CP850", "CP850", cp850_aliases, 0x80, cp850_ucs_table);

static const char *koi8r_aliases[] = {"KOI8R", NULL};
static const unsigned short koi8r_ucs_table[] = {
	0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
	0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
	0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248,
	0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
	0x2550, 0x2551, 0x2552, 0x0451, 0x2553, 0x2554, 0x2555, 0x2556,
	0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x255C, 0x255D, 0x255E,
	0x255F, 0x2560, 0x2561, 0x0401, 0x2562, 0x2563, 0x2564, 0x2565,
	0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x256B, 0x256C, 0x00A9,
	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A
};
DEF_SB_TBL(koi8r, "KOI8-R", "KOI8-R", koi8r_aliases, 0x80, koi8r_ucs_table);

static const char *koi8u_aliases[] = {"KOI8U", NULL};
static const unsigned short koi8u_ucs_table[] = {
	0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
	0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
	0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248,
	0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
	0x2550, 0x2551, 0x2552, 0x0451, 0x0454, 0x2554, 0x0456, 0x0457,
	0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x0491, 0x255D, 0x255E,
	0x255F, 0x2560, 0x2561, 0x0401, 0x0404, 0x2563, 0x0406, 0x0407,
	0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x0490, 0x256C, 0x00A9,
	0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
	0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
	0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
	0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
	0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
	0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
	0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
	0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A
};
DEF_SB_TBL(koi8u, "KOI8-U", "KOI8-U", koi8u_aliases, 0x80, koi8u_ucs_table);

static const char *armscii8_aliases[] = {"ArmSCII8", "ARMSCII-8", "ARMSCII8", NULL};
static const unsigned short armscii8_ucs_table[] = {
	0x00A0, 0x0000, 0x0587, 0x0589, 0x0029, 0x0028, 0x00BB, 0x00AB,
	0x2014, 0x002E, 0x055D, 0x002C, 0x002D, 0x058A, 0x2026, 0x055C,
	0x055B, 0x055E, 0x0531, 0x0561, 0x0532, 0x0562, 0x0533, 0x0563,
	0x0534, 0x0564, 0x0535, 0x0565, 0x0536, 0x0566, 0x0537, 0x0567,
	0x0538, 0x0568, 0x0539, 0x0569, 0x053A, 0x056A, 0x053B, 0x056B,
	0x053C, 0x056C, 0x053D, 0x056D, 0x053E, 0x056E, 0x053F, 0x056F,
	0x0540, 0x0570, 0x0541, 0x0571, 0x0542, 0x0572, 0x0543, 0x0573,
	0x0544, 0x0574, 0x0545, 0x0575, 0x0546, 0x0576, 0x0547, 0x0577,
	0x0548, 0x0578, 0x0549, 0x0579, 0x054A, 0x057A, 0x054B, 0x057B,
	0x054C, 0x057C, 0x054D, 0x057D, 0x054E, 0x057E, 0x054F, 0x057F,
	0x0550, 0x0580, 0x0551, 0x0581, 0x0552, 0x0582, 0x0553, 0x0583,
	0x0554, 0x0584, 0x0555, 0x0585, 0x0556, 0x0586, 0x055A, 0x0000
};
static const unsigned char ucs_armscii8_table[] = {
	0xA5, 0xA4, 0x2A, 0x2B, 0xAB, 0xAC, 0xA9, 0x2F
};
DEF_SB(armscii8, "ArmSCII-8", "ArmSCII-8", armscii8_aliases);

static int mbfl_filt_conv_armscii8_wchar(int c, mbfl_convert_filter *filter)
{
	CK((*filter->output_function)((c < 0xA0) ? c : coalesce(armscii8_ucs_table[c - 0xA0], MBFL_BAD_INPUT), filter->data));
	return 0;
}

static int mbfl_filt_conv_wchar_armscii8(int c, mbfl_convert_filter *filter)
{
	if (c >= 0x28 && c <= 0x2F) {
		CK((*filter->output_function)(ucs_armscii8_table[c - 0x28], filter->data));
	} else if (c < 0 || c == MBFL_BAD_INPUT) {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	} else if (c < 0xA0) {
		CK((*filter->output_function)(c, filter->data));
	} else {
		for (int n = 0; n < 0x60; n++) {
			if (c == armscii8_ucs_table[n]) {
				CK((*filter->output_function)(0xA0 + n, filter->data));
				return 0;
			}
		}
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}
	return 0;
}

static size_t mb_armscii8_to_wchar(unsigned char **in, size_t *in_len, uint32_t *buf, size_t bufsize, unsigned int *state)
{
	unsigned char *p = *in, *e = p + *in_len;
	uint32_t *out = buf, *limit = buf + bufsize;

	while (p < e && out < limit) {
		unsigned char c = *p++;
		*out++ = (c < 0xA0) ? c : coalesce(armscii8_ucs_table[c - 0xA0], MBFL_BAD_INPUT);
	}

	*in_len = e - p;
	*in = p;
	return out - buf;
}

static void mb_wchar_to_armscii8(uint32_t *in, size_t len, mb_convert_buf *buf, bool end)
{
	unsigned char *out, *limit;
	MB_CONVERT_BUF_LOAD(buf, out, limit);
	MB_CONVERT_BUF_ENSURE(buf, out, limit, len);

	while (len--) {
		uint32_t w = *in++;

		if (w >= 0x28 && w <= 0x2F) {
			out = mb_convert_buf_add(out, ucs_armscii8_table[w - 0x28]);
		} else if (w < 0xA0) {
			out = mb_convert_buf_add(out, w);
		} else {
			for (int i = 0; i < 0x60; i++) {
				if (w == armscii8_ucs_table[i]) {
					out = mb_convert_buf_add(out, 0xA0 + i);
					goto continue_armscii8;
				}
			}
			MB_CONVERT_ERROR(buf, out, limit, w, mb_wchar_to_armscii8);
			MB_CONVERT_BUF_ENSURE(buf, out, limit, len);
		}
		continue_armscii8: ;
	}

	MB_CONVERT_BUF_STORE(buf, out, limit);
}
