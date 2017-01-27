/* lookup.c - implementation of IDNA2008 lookup functions
   Copyright (C) 2011-2016 Simon Josefsson

   Libidn2 is free software: you can redistribute it and/or modify it
   under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version.

   or both in parallel, as here.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include "idn2.h"

#include <errno.h>		/* errno */
#include <stdlib.h>		/* malloc, free */

#include "punycode.h"

#include <unitypes.h>
#include <uniconv.h>		/* u8_strconv_from_locale */
#include <unistr.h>		/* u8_to_u32, u32_cpy, ... */

/**
 * idn2_to_unicode_8z4z:
 * @input: Input zero-terminated UTF-8 string.
 * @output: Newly allocated UTF-32/UCS-4 output string.
 * @flags: Currently unused.
 *
 * Perform punycode decoding on domain name @input.
 *
 * @output may be NULL to test lookup of @input without allocating memory.
 *
 * Returns:
 *   %IDN2_OK: The conversion was successful.
 *   %IDN2_TOO_BIG_DOMAIN: The domain is too long.
 *   %IDN2_TOO_BIG_LABEL: A label is would have been too long.
 *   %IDN2_ENCODING_ERROR: Character conversion failed.
 *   %IDN2_MALLOC: Memory allocation failed.
 **/
int
idn2_to_unicode_8z4z (const char * input, uint32_t ** output, int flags _GL_UNUSED)
{
  uint32_t *domain_u32;
  int rc;

  if (!input)
    {
      if (output)
	*output = NULL;
      return IDN2_OK;
    }

  /* split into labels and check */
  uint32_t out_u32[IDN2_DOMAIN_MAX_LENGTH + 1];
  size_t out_len = 0;
  const char *e, *s;

  for (e = s = input; *e; s = e)
    {
      uint32_t label_u32[IDN2_LABEL_MAX_LENGTH];
      size_t label_len = IDN2_LABEL_MAX_LENGTH;

      while (*e && *e != '.')
	e++;

      if (e - s >= 4 && (s[0] == 'x' || s[0] == 'X')
	  && (s[1] == 'n' || s[1] == 'N')
	  && s[2] == '-' && s[3] == '-')
	{
	  s += 4;

	  rc =
	    _idn2_punycode_decode (e - s, (char *) s, &label_len, label_u32);
	  if (rc)
	    return rc;

	  if (out_len + label_len + (*e == '.') > IDN2_DOMAIN_MAX_LENGTH)
	    return IDN2_TOO_BIG_DOMAIN;

	  u32_cpy (out_u32 + out_len, label_u32, label_len);
	}
      else
        {
	  /* convert UTF-8 input to UTF-32 */
	  if (!(domain_u32 = u8_to_u32 (s, e - s, NULL, &label_len)))
	    {
	      if (errno == ENOMEM)
		return IDN2_MALLOC;
	      return IDN2_ENCODING_ERROR;
	    }

	  if (label_len > IDN2_LABEL_MAX_LENGTH)
	    {
	      free (domain_u32);
	      return IDN2_TOO_BIG_LABEL;
	    }

	  if (out_len + label_len + (*e == '.') > IDN2_DOMAIN_MAX_LENGTH)
	    {
	      free (domain_u32);
	      return IDN2_TOO_BIG_DOMAIN;
	    }

	  u32_cpy (out_u32 + out_len, domain_u32, label_len);
	  free (domain_u32);
	}

      out_len += label_len;
      if (*e)
	{
	  out_u32[out_len++] = '.';
	  e++;
	}
    }

  if (output)
    {
      uint32_t *_out;

      out_u32[out_len] = 0;

      _out = u32_cpy_alloc (out_u32, out_len + 1);
      if (!_out)
        {
	  if (errno == ENOMEM)
	    return IDN2_MALLOC;
	  return IDN2_ENCODING_ERROR;
	}

      *output = _out;
    }

  return IDN2_OK;
}

int
idn2_to_unicode_4z4z (const uint32_t * input, uint32_t ** output, int flags)
{
  uint8_t *input_u8;
  uint32_t *output_u32;
  size_t length;
  int rc;

  if (!input)
    {
      if (output)
	*output = NULL;
      return IDN2_OK;
    }

  input_u8 = u32_to_u8 (input, u32_strlen(input) + 1, NULL, &length);
  if (!input_u8)
    {
      if (errno == ENOMEM)
	return IDN2_MALLOC;
      return IDN2_ENCODING_ERROR;
    }

  rc = idn2_to_unicode_8z4z (input_u8, &output_u32, flags);
  free (input_u8);

  if (rc == IDN2_OK)
    {
      if (output)
	*output = output_u32;
      else
	free(output_u32);
    }

  return rc;
}

int
idn2_to_unicode_44i (const uint32_t * in, size_t inlen, uint32_t * out, size_t * outlen, int flags)
{
  uint32_t *input_u32;
  uint32_t *output_u32;
  size_t len;
  int rc;

  if (!in)
    {
      if (outlen)
	*outlen = 0;
      return IDN2_OK;
    }

  input_u32 = malloc ((inlen + 1) * sizeof(uint32_t));
  if (!input_u32)
    return IDN2_MALLOC;

  u32_cpy (input_u32, in, inlen);
  input_u32[inlen] = 0;

  rc = idn2_to_unicode_4z4z (input_u32, &output_u32, flags);
  free (input_u32);
  if (rc != IDN2_OK)
    return rc;

  len = u32_strlen (output_u32);
  if (out && outlen)
    u32_cpy (out, output_u32, len < *outlen ? len : *outlen);
  free (output_u32);

  if (outlen)
    *outlen = len;

  return IDN2_OK;
}

int
idn2_to_unicode_8z8z (const char * input, char ** output, int flags)
{
  uint32_t *output_u32;
  uint8_t *output_u8;
  size_t length;
  int rc;

  rc = idn2_to_unicode_8z4z (input, &output_u32, flags);
  if (rc != IDN2_OK || !input)
    return rc;

  output_u8 = u32_to_u8 (output_u32, u32_strlen(output_u32) + 1, NULL, &length);
  free (output_u32);

  if (!output_u8)
    {
      if (errno == ENOMEM)
	return IDN2_MALLOC;
      return IDN2_ENCODING_ERROR;
    }

  if (output)
    *output = (char *) output_u8;
  else
    free (output_u8);

  return IDN2_OK;
}

int
idn2_to_unicode_8zlz (const char * input, char ** output, int flags)
{
  int rc;
  uint8_t *output_u8, *output_l8;

  rc = idn2_to_unicode_8z8z (input, (char **) &output_u8, flags);
  if (rc != IDN2_OK || !input)
    return rc;

  output_l8 = u8_strconv_to_locale (output_u8);
  free (output_u8);

  if (output)
    *output = (char *) output_l8;
  else
    free (output_l8);

  return IDN2_OK;
}

int
idn2_to_unicode_lzlz (const char * input, char ** output, int flags)
{
  uint8_t *input_l8;
  int rc;

  if (!input)
    {
      if (output)
	*output = NULL;
      return IDN2_OK;
    }

  input_l8 = u8_strconv_from_locale (input);
  if (!input_l8)
    {
      if (errno == ENOMEM)
	return IDN2_MALLOC;
      return IDN2_ENCODING_ERROR;
    }

  rc = idn2_to_unicode_8zlz (input_l8, output, flags);
  free (input_l8);

  return rc;
}