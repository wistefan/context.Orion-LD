/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/
#include <string.h>
#include <string>
#include <sstream>

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "common/MimeType.h"
#include "common/wsStrip.h"



/* ****************************************************************************
*
* mimeTypeToString -
*/
const char* mimeTypeToString(MimeType mimeType)
{
  switch (mimeType)
  {
  case JSON:              return "JSON";
  case JSONLD:            return "JSONLD";
  case GEOJSON:           return "GEOJSON";
  case TEXT:              return "TEXT";
  case HTML:              return "HTML";
  case MERGE_PATCH_JSON:  return "MERGEPATCHJSON";
  case NOMIMETYPE:        return "NOMIMETYPE";
  }

  return "Unknown mimeType";
}



/* ****************************************************************************
*
* stringToMimeType
*/
MimeType stringToMimeType(const std::string& s)
{
  if      (s == "JSON")            return JSON;
  else if (s == "JSONLD")          return JSONLD;
  else if (s == "GEOJSON")         return GEOJSON;
  else if (s == "TEXT")            return TEXT;
  else if (s == "HTML")            return HTML;
  else if (s == "MERGEPATCHJSON")  return MERGE_PATCH_JSON;
  else if (s == "NOMIMETYPE")      return NOMIMETYPE;

  return NOMIMETYPE;
}


#ifdef ORIONLD
/* ****************************************************************************
*
* longStringToMimeType
*/
MimeType longStringToMimeType(const char* s)
{
  if      (strcmp(s, "application/json")             == 0) return JSON;
  else if (strcmp(s, "application/ld+json")          == 0) return JSONLD;
  else if (strcmp(s, "application/geo+json")         == 0) return GEOJSON;
  else if (strcmp(s, "plain/text")                   == 0) return TEXT;
  else if (strcmp(s, "application/html")             == 0) return HTML;
  else if (strcmp(s, "application/merge-patch+json") == 0) return MERGE_PATCH_JSON;
  else if (strcmp(s, "NOMIMETYPE")                   == 0) return NOMIMETYPE;

  return NOMIMETYPE;
}
#endif



/* ****************************************************************************
*
* mimeTypeToLongString -
*/
const char* mimeTypeToLongString(MimeType mimeType)
{
  switch (mimeType)
  {
  case JSON:              return "application/json";
  case JSONLD:            return "application/ld+json";
  case GEOJSON:           return "application/geo+json";
  case TEXT:              return "plain/text";
  case HTML:              return "application/html";
  case MERGE_PATCH_JSON:  return "application/merge-patch+json";
  case NOMIMETYPE:        return "NOMIMETYPE";
  }

  return "NOMIMETYPE";
}
