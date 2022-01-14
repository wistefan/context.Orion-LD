#ifndef SRC_LIB_ORIONLD_COMMON_ORIONLDSTATE_H_
#define SRC_LIB_ORIONLD_COMMON_ORIONLDSTATE_H_

/*
*
* Copyright 2019 FIWARE Foundation e.V.
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
#include <time.h>                                                // struct timespec
#include <semaphore.h>                                           // sem_t
#include <mongoc/mongoc.h>                                       // MongoDB C Client Driver
#include <microhttpd.h>                                          // MHD_Connection

#include "orionld/db/dbDriver.h"                                 // database driver header
#include "orionld/db/dbConfiguration.h"                          // DB_DRIVER_MONGOC

extern "C"
{
#include "kjson/kjson.h"                                         // Kjson
#include "kjson/KjNode.h"                                        // KjNode
}

#include "common/globals.h"                                      // ApiVersion
#include "common/MimeType.h"                                     // MimeType
#include "rest/HttpStatusCode.h"                                 // HttpStatusCode
#include "rest/Verb.h"                                           // Verb

#include "orionld/common/performance.h"                          // REQUEST_PERFORMANCE
#include "orionld/common/QNode.h"                                // QNode
#include "orionld/common/OrionldResponseBuffer.h"                // OrionldResponseBuffer
#include "orionld/types/OrionldProblemDetails.h"                 // OrionldProblemDetails
#include "orionld/types/OrionldGeoIndex.h"                       // OrionldGeoIndex
#include "orionld/types/OrionldGeoJsonType.h"                    // OrionldGeoJsonType
#include "orionld/types/OrionldPrefixCache.h"                    // OrionldPrefixCache
#include "orionld/types/OrionldTenant.h"                         // OrionldTenant
#include "orionld/troe/troe.h"                                   // TroeMode
#include "orionld/context/OrionldContext.h"                      // OrionldContext



// -----------------------------------------------------------------------------
//
// QNODE_SIZE - maximum number of QNodes allowed
//
#define QNODE_SIZE 100



// -----------------------------------------------------------------------------
//
// ORIONLD_VERSION -
//
#define ORIONLD_VERSION 1.1.0-PRE-22



// -----------------------------------------------------------------------------
//
// ORIONLD_DEFAULT_DATASET_ID -
//
#define ORIONLD_DEFAULT_DATASET_ID "urn:ngsi-ld:default:datasetId"



// -----------------------------------------------------------------------------
//
// Forward declarations -
//
struct OrionLdRestService;
struct ConnectionInfo;



// -----------------------------------------------------------------------------
//
// OrionldUriParamOptions - flags for all possible members in URI Param ?options=x,y,z
//
typedef struct OrionldUriParamOptions
{
  bool noOverwrite;
  bool update;
  bool replace;
  bool keyValues;
  bool values;         // Only NGSIv2
  bool uniqueValues;   // Only NGSIv2
  bool dateCreated;    // Only NGSIv2
  bool dateModified;   // Only NGSIv2
  bool append;         // Only NGSIv2
  bool noAttrDetail;   // Only NGSIv2
  bool upsert;         // Only NGSIv2
  bool sysAttrs;
} OrionldUriParamOptions;



// -----------------------------------------------------------------------------
//
// OrionldUriParams -
//
typedef struct OrionldUriParams
{
  char*     id;
  char*     type;
  char*     typePattern;
  char*     idPattern;
  char*     attrs;
  char*     options;
  int       offset;
  int       limit;
  bool      count;
  char*     q;
  char*     mq;
  char*     geometry;
  char*     coordinates;
  char*     georel;
  char*     geoloc;
  char*     geoproperty;
  char*     geometryProperty;
  char*     datasetId;
  bool      deleteAll;
  char*     timeproperty;
  char*     timerel;
  char*     timeAt;
  char*     endTimeAt;
  bool      details;
  uint32_t  mask;
  bool      prettyPrint;
  int       spaces;
  char*     subscriptionId;
  bool      location;
  char*     url;
  bool      reload;
  char*     exists;
  char*     notExists;
  char*     metadata;
  char*     orderBy;
  bool      collapse;
  bool      reset;
  char*     attributeFormat;
  char*     level;
} OrionldUriParams;



// -----------------------------------------------------------------------------
//
// OrionldNotificationInfo -
//
typedef struct OrionldNotificationInfo
{
  char*     subscriptionId;
  MimeType  mimeType;
  KjNode*   attrsForNotification;
  char*     reference;
  int       fd;
  bool      connected;
  bool      allOK;
} OrionldNotificationInfo;


typedef enum OrionldPhase
{
  OrionldPhaseStartup = 1,
  OrionldPhaseServing
} OrionldPhase;



// -----------------------------------------------------------------------------
//
// OrionldStateOut - data for the response
//
typedef struct OrionldStateOut
{
  // Outgoing HTTP headers
  MimeType  contentType;

#if 0
  char*     httpHeaderV[10];    // Buffer to be used if less than 10 headers
  char**    httpHeader;         // Points to httpHeaderV, reallocated if necessary
  int       httpHeaderSize;     // Max number of headers (reallocation if necessary)
  int       httpHeaderIx;       // Current index of 'httpHeader'
#endif
} OrionldStateOut;



// -----------------------------------------------------------------------------
//
// OrionldStateIn - data of the request
//
typedef struct OrionldStateIn
{
  // Incoming HTTP headers
  MimeType  contentType;
} OrionldStateIn;

// -----------------------------------------------------------------------------
//
// OrionldConnectionState - the state of the connection
//
// This struct contains all the state of a connection, like the Kjson pointer, the pointer to
// the RestService of the request or the urlPath of the request or ...
// Basically EVERYTHING that is a 'characteristics' for the connection.
// These fields/variables will be set once, initially, when the request arrived and after that will only be read.
// It makes very little sense to send these variables to each and every function where they are to be used.
// Much easier and faster to simply store them in a thread global struct.
//
typedef struct OrionldConnectionState
{
  OrionldPhase            phase;
  MHD_Connection*         mhdConnection;
  ConnectionInfo*         ciP;                    // To Be Removed
  struct timespec         timestamp;              // The time when the request entered
  double                  requestTime;            // Same same, but at a floating point
  char                    requestTimeString[64];  // ISO8601 representation of 'requestTime'
  int                     httpStatusCode;
  Kjson                   kjson;
  Kjson*                  kjsonP;
  KAlloc                  kalloc;
  char                    kallocBuffer[8 * 1024];
  char*                   requestPayload;
  KjNode*                 requestTree;
  KjNode*                 responseTree;
  char*                   responsePayload;
  bool                    responsePayloadAllocated;
  char*                   tenantName;
  OrionldTenant*          tenantP;
  char*                   servicePath;
  bool                    linkHttpHeaderPresent;
  char*                   link;
  bool                    linkHeaderAdded;
  bool                    noLinkHeader;
  char*                   preferHeader;
  char*                   authorizationHeader;
  OrionldContext*         contextP;
  ApiVersion              apiVersion;
  int                     requestNo;

  KjNode*                 geoAttr[10];                 // Preallocated array of GeoProperties
  KjNode**                geoAttrV;                    // Array of GeoProperty attributes
  int                     geoAttrs;
  int                     geoAttrMax;

  char*                   geoType;
  KjNode*                 geoCoordsP;
  char*                   entityId;
  OrionldUriParamOptions  uriParamOptions;
  OrionldUriParams        uriParams;
  char*                   errorAttributeArrayP;
  char                    errorAttributeArray[512];
  int                     errorAttributeArrayUsed;
  int                     errorAttributeArraySize;
  OrionLdRestService*     serviceP;
  char*                   wildcard[2];
  char*                   urlPath;
  char*                   httpVersion;
  Verb                    verb;
  bool                    badVerb;     // ToDo: verb == NOVERB should cover this
  char*                   verbString;
  bool                    acceptJson;
  bool                    acceptJsonld;
  bool                    acceptGeojson;
  bool                    ngsildContent;
  KjNode*                 payloadContextNode;
  KjNode*                 payloadIdNode;
  KjNode*                 payloadTypeNode;
  char                    contextId[256];
  QNode                   qNodeV[QNODE_SIZE];
  int                     qNodeIx;
  mongo::BSONObj*         qMongoFilterP;
  char*                   jsonBuf;           // Used by kjTreeFromBsonObj

#if 0
  //
  // Array of KjNode trees that are to freed when the request thread ends
  //
  KjNode*                 delayedKjFreeVec[50];
  int                     delayedKjFreeVecIndex;
  int                     delayedKjFreeVecSize;
#endif

  //
  // Array of allocated buffers that are to be freed when the request thread ends
  //
  void*                   delayedFreeVec[1001];  // FIXME: try to make this number smaller ...
  int                     delayedFreeVecIndex;
  int                     delayedFreeVecSize;

  //
  // Special "delayed free" field for orionldRequestSend that does reallocs and it's simpler this way
  //
  void*                   delayedFreePointer;

  int                     notificationRecords;
  OrionldNotificationInfo notificationInfo[16];
  bool                    notify;
  OrionldPrefixCache      prefixCache;
  OrionldResponseBuffer   httpResponse;

  //
  // MongoDB stuff - Context Cache uses mongoc regardless of which mongo client lib is in use
  //
  mongoc_uri_t*           mongoUri;
  mongoc_client_t*        mongoClient;
  mongoc_database_t*      mongoDatabase;

  //
  // Instructions for mongoBackend
  //
  KjNode*                 creDatesP;
  bool                    onlyCount;
  KjNode*                 datasets;

  //
  // General Behavior
  //
  bool                    forwardAttrsCompacted;

  //
  // TRoE
  //
  bool                    noDbUpdate;        // If nothing changed in DB - troe is not invoked
  bool                    troeError;
  KjNode*                 duplicateArray;
  KjNode*                 troeIgnoreV[20];
  unsigned int            troeIgnoreIx;
  KjNode*                 batchEntities;
  KjNode*                 dbAttrWithDatasetsP;  // Used in TRoE for DELETE Attribute with ?deleteAll=true
  TroeMode                troeOpMode;           // Used in troePostEntities as both POST /entities and POST /temporal/entities use troePostEntities


  //
  // GeoJSON - help vars for the case:
  // - Accept: application/geo+json
  // - URI param 'attrs' used
  // - The geometryproperty is not part of 'attrs' URI param
  //
  KjNode*                 geoPropertyNode;     // Must point to the "value" of the GeoProperty (for Retrieve Entity only)
  bool                    geoPropertyMissing;  // The gro-property is really not present in the DB - must be NULL is the response (for Retrieve Entity only)
  KjNode*                 geoPropertyNodes;    // object with "entityId": { <GeoProperty value> }, one per entity (for Query Entities


  OrionldStateOut out;
  OrionldStateIn  in;

  // NGSI-LD Scope (or NGSIv2 ServicePath)
  char* scopeV[10];
  int   scopes;

  // Attribute Format
  char* attrsFormat;

  // X-Auth-Token
  char* xAuthToken;

  // FIWARE Correlator
  char* correlator;

  //
  // Error Handling
  //
  OrionldProblemDetails   pd;
} OrionldConnectionState;



#ifdef REQUEST_PERFORMANCE
// -----------------------------------------------------------------------------
//
// Timestamps - timestamps for performance tests
//
typedef struct Timestamps
{
  struct timespec reqStart;               // Start of          Request
  struct timespec reqEnd;                 // End of            Request
  struct timespec parseStart;             // Start of          Request Payload body JSON parse
  struct timespec parseEnd;               // End of            Request Payload body JSON parse
  struct timespec serviceRoutineStart;    // Start of          Service Routine
  struct timespec serviceRoutineEnd;      // End of            Service Routine
  struct timespec mongoBackendStart;      // Start of          Mongo Backend Command
  struct timespec mongoBackendEnd;        // End of            Mongo Backend Command
  struct timespec dbStart;                // Start of    Main  DB query/update
  struct timespec dbEnd;                  // End of      Main  DB query/update
  struct timespec extraDbStart;           // Start of "extra"  DB query, e.g. query before an update
  struct timespec extraDbEnd;             // End of   "extra"  DB query, e.g. query before an update
  struct timespec notifStart;             // Start of          Sending of Notifications
  struct timespec notifEnd;               // End of            Sending of Notifications
  struct timespec notifDbStart;           // Start of          DB query for Notifications
  struct timespec notifDbEnd;             // End of            DB query for Notifications
  struct timespec forwardStart;           // Start of          Sending of Forwarded messages
  struct timespec forwardEnd;             // End of            Sending of Forwarded messages
  struct timespec forwardDbStart;         // Start of          DB query for Forwaring
  struct timespec forwardDbEnd;           // End of            DB query for Forwaring
  struct timespec renderStart;            // Start of          Resonse Payload body render JSON
  struct timespec renderEnd;              // End of            Resonse Payload body render JSON
  struct timespec restReplyStart;         // Start of          REST Reply
  struct timespec restReplyEnd;           // End of            REST Reply
  struct timespec troeStart;              // Start of          TRoE processing
  struct timespec troeEnd;                // End of            TRoE processing
  struct timespec requestPartEnd;         // End of            MHD-1-2-3 processing
  struct timespec requestCompletedStart;  // Start of          Request Completed
  struct timespec srStart[50];            // Start of          Service Routine Sample
  struct timespec srEnd[50];              // End of            Service Routine Sample
  char*           srDesc[50];             // Description for   Service Routine Sample
  double          mongoConnectAccumulated;
  int             getMongoConnectionCalls;
} Timestamps;

extern __thread Timestamps timestamps;
#endif


// -----------------------------------------------------------------------------
//
// orionldState -
//
extern __thread OrionldConnectionState orionldState;



// -----------------------------------------------------------------------------
//
// Global state
//
extern char*             coreContextUrl;
extern char              orionldHostName[128];
extern int               orionldHostNameLen;
extern char              kallocBuffer[32 * 1024];
extern int               requestNo;                // Never mind protecting with semaphore. Just a debugging help
extern KAlloc            kalloc;
extern Kjson             kjson;
extern Kjson*            kjsonP;
extern uint16_t          portNo;
extern char              dbHost[];                 // From orionld.cpp
extern char              dbName[];                 // From orionld.cpp
extern int               dbNameLen;
extern char              dbUser[];                 // From orionld.cpp
extern char              dbPwd[];                  // From orionld.cpp
extern bool              multitenancy;             // From orionld.cpp
extern int               contextDownloadAttempts;  // From orionld.cpp
extern int               contextDownloadTimeout;   // From orionld.cpp
extern bool              troe;                     // From orionld.cpp
extern char              troeHost[256];            // From orionld.cpp
extern unsigned short    troePort;                 // From orionld.cpp
extern char              troeUser[256];            // From orionld.cpp
extern char              troePwd[256];             // From orionld.cpp
extern int               troePoolSize;             // From orionld.cpp
extern char              pgPortString[16];
extern bool              forwarding;               // From orionld.cpp
extern const char*       orionldVersion;
extern OrionldGeoIndex*  geoIndexList;
extern OrionldPhase      orionldPhase;
extern bool              orionldStartup;           // For now, only used inside sub-cache routines
extern bool              idIndex;                  // From orionld.cpp
extern bool              noNotifyFalseUpdate;      // From orionld.cpp
extern char              mongoServerVersion[32];



// -----------------------------------------------------------------------------
//
// Global variables for Mongo C Driver
//
extern mongoc_collection_t*  mongoEntitiesCollectionP;
extern mongoc_collection_t*  mongoRegistrationsCollectionP;
extern mongoc_collection_t*  mongoContextsCollectionP;       // The Context Cache module uses mongoc regardless
extern sem_t                 mongoContextsSem;



// -----------------------------------------------------------------------------
//
// orionldStateInit - initialize the thread-local variable orionldState
//
extern void orionldStateInit(MHD_Connection* connection);



// -----------------------------------------------------------------------------
//
// orionldStateRelease - release the thread-local variable orionldState
//
extern void orionldStateRelease(void);



// ----------------------------------------------------------------------------
//
// orionldStateErrorAttributeAdd -
//
extern void orionldStateErrorAttributeAdd(const char* attributeName);



// -----------------------------------------------------------------------------
//
// orionldStateDelayedKjFreeEnqueue -
//
extern void orionldStateDelayedKjFreeEnqueue(KjNode* tree);



// -----------------------------------------------------------------------------
//
// orionldStateDelayedFreeEnqueue -
//
extern void orionldStateDelayedFreeEnqueue(void* allocatedBuffer);



// -----------------------------------------------------------------------------
//
// orionldStateDelayedFreeCancel -
//
extern void orionldStateDelayedFreeCancel(void* allocatedBuffer);

#endif  // SRC_LIB_ORIONLD_COMMON_ORIONLDSTATE_H_
