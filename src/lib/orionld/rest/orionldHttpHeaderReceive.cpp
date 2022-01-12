/*
*
* Copyright 2021 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/
extern "C"
{
#include "kbase/kMacros.h"                                       // K_VEC_SIZE
}

#include "logMsg/logMsg.h"                                       // LM_*
#include "logMsg/traceLevels.h"                                  // Lmt*

#include "common/string.h"                                       // toLowercase
#include "common/wsStrip.h"                                      // wsStrip
#include "common/MimeType.h"                                     // mimeTypeParse
#include "orionld/common/orionldState.h"                         // orionldState
#include "orionld/common/orionldErrorResponse.h"                 // OrionldBadRequestData, ...
#include "orionld/rest/orionldHttpHeaderReceive.h"               // Own interface



// -----------------------------------------------------------------------------
//
// strSplit -
//
static int strSplit(char* s, char delimiter, char** outV, int outMaxItems)
{
  if (s == NULL)
    return 0;

  if (*s == 0)
    return 0;

  int   outIx = 0;
  char* start = s;

  //
  // Loop over 's':
  // - Search for the delimiter
  // - zero-terminate
  // - assign and
  // - continue
  //
  while (*s != 0)
  {
    if (*s == delimiter)
    {
      *s = 0;
      outV[outIx] = wsStrip(start);
      start = &s[1];

      // Check that the scope starts with a slash, etc ...
      // if (scopeCheck(outV[outIx]) == false) ...

      if (++outIx > outMaxItems)
        return -1;
    }

    ++s;
  }

  outV[outIx] = wsStrip(start);

  ++outIx;

  return outIx;
}



// -----------------------------------------------------------------------------
//
// scopeHeader -
//
static void scopeHeader(const char* value)
{
  orionldState.scopes = strSplit((char*) value, ',', orionldState.scopeV, K_VEC_SIZE(orionldState.scopeV));
  if (orionldState.scopes == -1)
  {
    LM_W(("Bad Input (too many scopes)"));
    orionldErrorResponseCreate(OrionldBadRequestData, "Bad value for HTTP header /NGSILD-Scope/", value);
    orionldState.httpStatusCode = 400;
  }
}



// -----------------------------------------------------------------------------
//
// acceptHeader -
//
static MimeType acceptHeader(const char* accept)
{
  MimeType contentType = JSON;

  LM_TMP(("KZ: Accept header: %s", accept));

  if      (strcasecmp(accept, "application/json")    == 0)  contentType = JSON;
  else if (strcasecmp(accept, "application/ld+json") == 0)  contentType = JSONLD;

  return contentType;
}



// -----------------------------------------------------------------------------
//
// contentTypeParse -
//
// NOTE
//   Non-static as used also in src/lib/rest/rest.cpp
//
MimeType contentTypeParse(const char* contentType, char** charsetP)
{
  char* s;
  char* cP = (char*) contentType;

  if ((s = strstr(cP, ";")) != NULL)
  {
    *s = 0;
    ++s;
    s = wsStrip(s);

    if ((charsetP != NULL) && (strncmp(s, "charset=", 8) == 0))
      *charsetP = &s[8];
  }

  cP = wsStrip(cP);

  if      (strcmp(cP, "*/*") == 0)                          return JSON;
  else if (strcmp(cP, "text/json") == 0)                    return JSON;
  else if (strcmp(cP, "application/json") == 0)             return JSON;
  else if (strcmp(cP, "application/ld+json") == 0)          return JSONLD;
  else if (strcmp(cP, "application/geo+json") == 0)         return GEOJSON;
  else if (strcmp(cP, "application/html") == 0)             return HTML;
  else if (strcmp(cP, "application/merge-patch+json") == 0) return MERGE_PATCH_JSON;
  else if (strcmp(cP, "text/plain") == 0)                   return TEXT;
  else
    orionldState.in.invalidContentType = cP;

  return JSON;
}



// -----------------------------------------------------------------------------
//
// tenantHeader -
//
static char* tenantHeader(const char* value)
{
  if (multitenancy == true)  // Has the broker been started with multi-tenancy enabled (it's disabled by default)
  {
    toLowercase((char*) value);
    return (char*) value;
  }

  // Tenant used when tenant is not supported by the broker - silently ignored for NGSIv2/v2, error for NGSI-LD
  if (orionldState.apiVersion == NGSI_LD_V1)
  {
    LM_E(("tenant in use but tenant support is not enabled for the broker"));
    orionldState.httpStatusCode = 400;
    orionldErrorResponseCreate(OrionldBadRequestData, "Tenants not supported", "tenant in use but tenant support is not enabled for the broker");
  }

  return (char*) "tenant-is-not-enabled";
}



// -----------------------------------------------------------------------------
//
// orionldHttpHeaderReceive -
//
MHD_Result orionldHttpHeaderReceive(void* cbDataP, MHD_ValueKind kind, const char* key, const char* value)
{
  LM_TMP(("KZ: Header '%s' = '%s', orionldState.httpStatusCode == %d", key, value, orionldState.httpStatusCode));

  if      (strcasecmp(key, "NGSILD-Scope")       == 0) scopeHeader(value);
  else if (strcasecmp(key, "Ngsiv2-AttrsFormat") == 0) orionldState.in.attrsFormat   = (char*) value;
  else if (strcasecmp(key, "X-Auth-Token")       == 0) orionldState.in.xAuthToken    = (char*) value;
  else if (strcasecmp(key, "Fiware-Correlator")  == 0) orionldState.in.correlator    = (char*) value;
  else if (strcasecmp(key, "Expect")             == 0) orionldState.in.expect        = (char*) value;
  else if (strcasecmp(key, "User-Agent")         == 0) orionldState.in.userAgent     = (char*) value;
  else if (strcasecmp(key, "Host")               == 0) orionldState.in.host          = (char*) value;
  else if (strcasecmp(key, "Connection")         == 0) orionldState.in.connection    = (char*) value;
  else if (strcasecmp(key, "Origin")             == 0) orionldState.in.origin        = (char*) value;
  else if (strcasecmp(key, "Prefer")             == 0) orionldState.in.prefer        = (char*) value;
  else if (strcasecmp(key, "Link")               == 0) { orionldState.link           = (char*) value; orionldState.linkHttpHeaderPresent = true; }
  else if (strcasecmp(key, "X-Auth-Token")       == 0) orionldState.in.xauthToken    = (char*) value;
  else if (strcasecmp(key, "Authorization")      == 0) orionldState.in.authorization = (char*) value;
  else if (strcasecmp(key, "X-Real-IP")          == 0) orionldState.in.realIp        = (char*) value;
  else if (strcasecmp(key, "X-Forwarded-For")    == 0) orionldState.in.xForwardedFor = (char*) value;
  else if (strcasecmp(key, "Fiware-Servicepath") == 0) {}
  else if (strcasecmp(key, "Content-Type")       == 0) orionldState.in.contentType   = contentTypeParse(value, NULL);
  else if (strcasecmp(key, "Content-Length")     == 0) orionldState.in.contentLength = atoi(value);
  else if (strcasecmp(key, "Accept")             == 0) orionldState.out.contentType  = acceptHeader(value);
  else if (strcasecmp(key, "NGSILD-Tenant")      == 0) orionldState.tenantName       = tenantHeader(value);
  else if (strcasecmp(key, "Fiware-Service")     == 0) orionldState.tenantName       = tenantHeader(value);

  LM_TMP(("KZ: Header '%s' = '%s', orionldState.httpStatusCode == %d", key, value, orionldState.httpStatusCode));

  return MHD_YES;
}
