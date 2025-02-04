/*
*
* Copyright 2015 Telefonica Investigacion y Desarrollo, S.A.U
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
#include <string>
#include <vector>

#include "orionld/common/orionldState.h"             // orionldState

#include "common/statistics.h"
#include "common/clockFunctions.h"
#include "common/string.h"

#include "rest/ConnectionInfo.h"
#include "rest/OrionError.h"
#include "rest/uriParamNames.h"
#include "rest/EntityTypeInfo.h"
#include "ngsi/ParseData.h"
#include "apiTypesV2/Entities.h"
#include "serviceRoutinesV2/getEntities.h"
#include "serviceRoutines/postQueryContext.h"
#include "alarmMgr/alarmMgr.h"



/* ****************************************************************************
*
* getEntities - 
*
* GET /v2/entities
*
* Payload In:  None
* Payload Out: Entities
*
* URI parameters:
*   - limit=NUMBER
*   - offset=NUMBER
*   - count=true/false
*   - id
*   - idPattern
*   - q
*   - mq
*   - geometry
*   - coords
*   - georel
*   - attrs
*   - metadata
*   - options=keyValues
*   - type=TYPE
*   - type=TYPE1,TYPE2,...TYPEN
*
* 01. Fill in QueryContextRequest
* 02. Call standard op postQueryContext
* 03. Render Entities response
* 04. Cleanup and return result
*/
std::string getEntities
(
  ConnectionInfo*            ciP,
  int                        components,
  std::vector<std::string>&  compV,
  ParseData*                 parseDataP
)
{
  Entities     entities;
  std::string  answer;
  std::string  pattern     = ".*";  // all entities, default value
  std::string  geometry    = ciP->uriParam["geometry"];
  std::string  coords      = ciP->uriParam["coords"];
  std::string  georel      = ciP->uriParam["georel"];
  std::string  out;
  char*  id          = orionldState.uriParams.id;
  char*  idPattern   = orionldState.uriParams.idPattern;
  char*  type        = orionldState.uriParams.type;
  char*  q           = orionldState.uriParams.q;
  char*  mq          = orionldState.uriParams.mq;
  char*  typePattern = orionldState.uriParams.typePattern;

  if ((idPattern != NULL) && (id != NULL))
  {
    OrionError oe(SccBadRequest, "Incompatible parameters: id, IdPattern", "BadRequest");

    TIMED_RENDER(answer = oe.toJson());
    orionldState.httpStatusCode = oe.code;
    return answer;
  }
  else if (id != NULL)
  {
    pattern = "";

    // FIXME P5: a more efficient query could be possible (this ends as a regex
    // at MongoDB and regex are *expensive* in performance terms)
    std::vector<std::string> idsV;

    stringSplit(id, ',', idsV);

    for (unsigned int ix = 0; ix != idsV.size(); ++ix)
    {
      if (ix != 0)
      {
        pattern += "|^";
      }
      else
      {
        pattern += "^";
      }

      pattern += idsV[ix] + "$";
    }
  }
  else if (idPattern != NULL)
  {
    pattern = idPattern;
  }

  if ((typePattern != NULL) && (type != NULL))
  {
    OrionError oe(SccBadRequest, "Incompatible parameters: type, typePattern", "BadRequest");

    TIMED_RENDER(answer = oe.toJson());
    orionldState.httpStatusCode = oe.code;
    return answer;
  }

  //
  // Making sure geometry, georel and coords are not used individually
  //
  if ((coords != "") && (geometry == ""))
  {
    OrionError oe(SccBadRequest, "Invalid query: URI param /coords/ used without /geometry/", "BadRequest");

    TIMED_RENDER(out = oe.toJson());
    orionldState.httpStatusCode = oe.code;
    return out;
  }
  else if ((geometry != "") && (coords == ""))
  {
    OrionError oe(SccBadRequest, "Invalid query: URI param /geometry/ used without /coords/", "BadRequest");

    TIMED_RENDER(out = oe.toJson());
    orionldState.httpStatusCode = oe.code;
    return out;
  }

  if ((georel != "") && (geometry == ""))
  {
    OrionError oe(SccBadRequest, "Invalid query: URI param /georel/ used without /geometry/", "BadRequest");

    TIMED_RENDER(out = oe.toJson());
    orionldState.httpStatusCode = oe.code;
    return out;
  }


  //
  // If URI param 'geometry' is present, create a new scope.
  // The fill() method of the scope checks the validity of the info in:
  // - geometry
  // - georel
  // - coords
  //
  if (geometry != "")
  {
    Scope*       scopeP = new Scope(SCOPE_TYPE_LOCATION, "");
    std::string  errorString;

    if (scopeP->fill(orionldState.apiVersion, geometry, coords, georel, &errorString) != 0)
    {
      OrionError oe(SccBadRequest, std::string("Invalid query: ") + errorString, "BadRequest");

      TIMED_RENDER(out = oe.toJson());
      orionldState.httpStatusCode = oe.code;

      scopeP->release();
      delete scopeP;

      return out;
    }

    parseDataP->qcr.res.restriction.scopeVector.push_back(scopeP);
  }



  //
  // String filter in URI param 'q' ?
  // If so, put it in a new Scope and parse the q-string.
  // The plain q-string is saved in Scope::value, just in case.
  // Might be useful for debugging, if nothing else.
  //
  if (q != NULL)
  {
    Scope*       scopeP = new Scope(SCOPE_TYPE_SIMPLE_QUERY, q);
    std::string  errorString;

    scopeP->stringFilterP = new StringFilter(SftQ);
    if (scopeP->stringFilterP->parse(q, &errorString) == false)
    {
      OrionError oe(SccBadRequest, errorString, "BadRequest");

      alarmMgr.badInput(clientIp, errorString);
      scopeP->release();
      delete scopeP;

      TIMED_RENDER(out = oe.toJson());
      orionldState.httpStatusCode = oe.code;
      return out;
    }

    parseDataP->qcr.res.restriction.scopeVector.push_back(scopeP);
  }


  //
  // Metadata string filter in URI param 'mq' ?
  // If so, put it in a new Scope and parse the mq-string.
  // The plain mq-string is saved in Scope::value, just in case.
  // Might be useful for debugging, if nothing else.
  //
  if (mq != NULL)
  {
    Scope*       scopeP = new Scope(SCOPE_TYPE_SIMPLE_QUERY_MD, mq);
    std::string  errorString;

    scopeP->mdStringFilterP = new StringFilter(SftMq);
    if (scopeP->mdStringFilterP->parse(mq, &errorString) == false)
    {
      OrionError oe(SccBadRequest, errorString, "BadRequest");

      alarmMgr.badInput(clientIp, errorString);
      scopeP->release();
      delete scopeP;

      TIMED_RENDER(out = oe.toJson());
      orionldState.httpStatusCode = oe.code;
      return out;
    }

    parseDataP->qcr.res.restriction.scopeVector.push_back(scopeP);
  }


  //
  // 01. Fill in QueryContextRequest - type "" is valid for all types
  //

  //
  // URI param 'type', three options:
  // 1. Not used, so empty
  // 2. Used with a single type name, so add it to the fill
  // 3. Used and with more than ONE typename

  if ((typePattern != NULL) && (*typePattern != 0))
  {
    bool      isIdPattern = (idPattern != NULL || pattern == ".*");
    EntityId* entityId    = new EntityId(pattern, typePattern, isIdPattern ? "true" : "false", true);

    parseDataP->qcr.res.entityIdVector.push_back(entityId);
  }
  else if (ciP->uriParamTypes.size() == 0)
  {
    parseDataP->qcr.res.fill(pattern, "", "true", EntityTypeEmptyOrNotEmpty, "");
  }
  else if (ciP->uriParamTypes.size() == 1)
  {
    parseDataP->qcr.res.fill(pattern, type, "true", EntityTypeNotEmpty, "");
  }
  else if (ciP->uriParamTypes.size() == 1)
  {
    parseDataP->qcr.res.fill(pattern, type, "true", EntityTypeNotEmpty, "");
  }
  else
  {
    //
    // More than one type listed in URI param 'type':
    // Add an entity per type to QueryContextRequest::entityIdVector
    //
    for (unsigned int ix = 0; ix < ciP->uriParamTypes.size(); ++ix)
    {
      EntityId* entityId = new EntityId(pattern, ciP->uriParamTypes[ix], "true");

      parseDataP->qcr.res.entityIdVector.push_back(entityId);
    }
  }


  // 02. Call standard op postQueryContext
  answer = postQueryContext(ciP, components, compV, parseDataP);

  // 03. Render Entities response
  if (parseDataP->qcrs.res.contextElementResponseVector.size() == 0)
  {
    orionldState.httpStatusCode = SccOk;
    answer = "[]";
  }
  else
  {
    entities.fill(&parseDataP->qcrs.res);

    if (entities.oe.code != SccNone)
    {
      TIMED_RENDER(answer = entities.oe.toJson());
      orionldState.httpStatusCode = entities.oe.code;
    }
    else
    {
      TIMED_RENDER(answer = entities.render());
      orionldState.httpStatusCode = SccOk;
    }
  }

  // 04. Cleanup and return result
  entities.release();
  parseDataP->qcr.res.release();

  return answer;
}
