////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_V8__GLOBALS_H
#define ARANGODB_V8_V8__GLOBALS_H 1

#include "Basics/Common.h"

#include <v8.h>

#include "Basics/ReadWriteLock.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for creating a v8 symbol for the specified string
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_SYMBOL(name) \
  v8::String::NewSymbol(name, (int) strlen(name))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for creating a v8 string for the specified string
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_STRING(name) \
  v8::String::New(name)

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for current v8 globals
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_CURRENT_GLOBALS                                  \
  v8::Isolate* isolate = v8::Isolate::GetCurrent();             \
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData(); \
  while (0)


////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for current v8 globals and scope
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_CURRENT_GLOBALS_AND_SCOPE                        \
  v8::Isolate* isolate = v8::Isolate::GetCurrent();             \
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData(); \
  v8::HandleScope scope;                                        \
  while (0)


////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an exception with an error code
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION(scope, code)                 \
  return scope.Close(v8::ThrowException(              \
    TRI_CreateErrorObject(__FILE__, __LINE__, code)))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an exception and returning
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_MESSAGE(scope, code, message)               \
  return scope.Close(v8::ThrowException(                             \
    TRI_CreateErrorObject(__FILE__, __LINE__, code, message, true)))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing a usage exception and returning
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_USAGE(scope, usage)                            \
  do {                                                                  \
    std::string msg = "usage: ";                                        \
    msg += usage;                                                       \
    return scope.Close(                                                 \
      v8::ThrowException(                                               \
        TRI_CreateErrorObject(__FILE__, __LINE__,                       \
                              TRI_ERROR_BAD_PARAMETER,                  \
                              msg.c_str())));                           \
  }                                                                     \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an internal exception and returning
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_INTERNAL(scope, message)                            \
  return scope.Close(v8::ThrowException(                                     \
    TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_INTERNAL, message)));

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing a parameter exception and returning
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_PARAMETER(scope, message)                                 \
  return scope.Close(v8::ThrowException(                                           \
    TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_BAD_PARAMETER, message)));

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an out-of-memory exception and returning
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_MEMORY(scope)                                   \
  return scope.Close(v8::ThrowException(                                 \
    TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_OUT_OF_MEMORY)));

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an exception for an system error
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_EXCEPTION_SYS(scope, message)                        \
  do {                                                              \
    TRI_set_errno(TRI_ERROR_SYS_ERROR);                             \
    std::string msg = message;                                      \
    msg += ": ";                                                    \
    msg += TRI_LAST_ERROR_STR;                                      \
    return scope.Close(v8::ThrowException(                          \
      TRI_CreateErrorObject(                                        \
        __FILE__, __LINE__,                                         \
        TRI_errno(),                                                \
        msg.c_str())));                                             \
  }                                                                 \
  while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing an error
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_ERROR(scope, message) \
  return scope.Close(v8::ThrowException(v8::Exception::Error(v8::String::New(message))))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing a range error
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_RANGE_ERROR(scope, message) \
  return scope.Close(v8::ThrowException(v8::Exception::RangeError(v8::String::New(message))))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing a syntax error
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_SYNTAX_ERROR(scope, message) \
  return scope.Close(v8::ThrowException(v8::Exception::SyntaxError(v8::String::New(message))))

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for throwing a type error
////////////////////////////////////////////////////////////////////////////////

#define TRI_V8_TYPE_ERROR(scope, message) \
  return scope.Close(v8::ThrowException(v8::Exception::TypeError(v8::String::New(message))))

////////////////////////////////////////////////////////////////////////////////
/// @brief "not yet implemented" handler for sharding
////////////////////////////////////////////////////////////////////////////////

#define TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection) \
  if (collection != 0 && ! collection->_isLocal) {                     \
    TRI_V8_EXCEPTION(scope, TRI_ERROR_NOT_IMPLEMENTED);                \
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief globals stored in the isolate
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_v8_global_s {

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

  TRI_v8_global_s (v8::Isolate*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

  ~TRI_v8_global_s ();

// -----------------------------------------------------------------------------
// --SECTION--                                                           HELPERS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief barrier mapping for weak pointers
////////////////////////////////////////////////////////////////////////////////

  std::map< void*, v8::Persistent<v8::Value> > JSBarriers;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection mapping for weak pointers
////////////////////////////////////////////////////////////////////////////////

  std::map< void*, v8::Persistent<v8::Value> > JSCollections;

// -----------------------------------------------------------------------------
// --SECTION--                                       JAVASCRIPT OBJECT TEMPLATES
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief agency template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> AgencyTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief clusterinfo template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> ClusterInfoTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief server state template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> ServerStateTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster comm template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> ClusterCommTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> ArangoErrorTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> SleepAndRequeueTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue function template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::FunctionTemplate> SleepAndRequeueFuncTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief general cursor template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> GeneralCursorTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_shaped_json_t template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> ShapedJsonTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_col_t template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> VocbaseColTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_t template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::ObjectTemplate> VocbaseTempl;

// -----------------------------------------------------------------------------
// --SECTION--                                     JAVASCRIPT FUNCTION TEMPLATES
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Buffer template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::FunctionTemplate> BufferTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief fast Buffer constructor
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::Function> FastBufferConstructor;

// -----------------------------------------------------------------------------
// --SECTION--                                              JAVASCRIPT FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Buffer template
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::Function> ExecuteFileCallback;

// -----------------------------------------------------------------------------
// --SECTION--                                              JAVASCRIPT CONSTANTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief "Buffer" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> BufferConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "DELETE" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> DeleteConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "GET" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> GetConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "HEAD" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> HeadConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "OPTIONS" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> OptionsConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "PATCH" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PatchConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "POST" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PostConstant;

////////////////////////////////////////////////////////////////////////////////
/// @brief "PUT" constant
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PutConstant;

// -----------------------------------------------------------------------------
// --SECTION--                                              JAVASCRIPT KEY NAMES
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief "address" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> AddressKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "bodyFromFile" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> BodyFromFileKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "body" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> BodyKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "client" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ClientKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "clientTransactionID" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ClientTransactionIDKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "code" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> CodeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "compatibility" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> CompatibilityKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "contentType" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ContentTypeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "cookies" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> CookiesKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "coordTransactionID" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> CoordTransactionIDKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "database" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> DatabaseKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "doCompact" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> DoCompactKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "domain" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> DomainKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "error" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ErrorKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "errorMessage" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ErrorMessageKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "errorNum" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ErrorNumKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "headers" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> HeadersKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "httpOnly" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> HttpOnlyKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "id" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> IdKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "isSystem" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> IsSystemKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "isVolatile" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> IsVolatileKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "journalSize" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> JournalSizeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "keyOptions" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> KeyOptionsKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "length" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> LengthKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "lifeTime" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> LifeTimeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "name" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> NameKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "operationID" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> OperationIDKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "parameters" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ParametersKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "path" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PathKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "prefix" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PrefixKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "port" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PortKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "portType" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> PortTypeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "protocol" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ProtocolKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "requestBody" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> RequestBodyKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "requestType" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> RequestTypeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "responseCode" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ResponseCodeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "secure" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> SecureKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "server" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ServerKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "shardID" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ShardIDKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "sleep" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> SleepKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "status" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> StatusKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "suffix" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> SuffixKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "timeout" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> TimeoutKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "transformations" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> TransformationsKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "url" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> UrlKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "user" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> UserKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "value" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> ValueKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "version" key
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> VersionKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "waitForSync" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> WaitForSyncKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_from" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _FromKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_dbName" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _DbNameKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_id" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _IdKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_key" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _KeyKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_oldRev" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _OldRevKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_rev" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _RevKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "_to" key name
////////////////////////////////////////////////////////////////////////////////

  v8::Persistent<v8::String> _ToKey;

// -----------------------------------------------------------------------------
// --SECTION--                                                          DATABASE
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief currently request object (might be invalid!)
////////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Value> _currentRequest;

////////////////////////////////////////////////////////////////////////////////
/// @brief currently response object (might be invalid!)
////////////////////////////////////////////////////////////////////////////////

  v8::Handle<v8::Value> _currentResponse;

////////////////////////////////////////////////////////////////////////////////
/// @brief currently running transaction
////////////////////////////////////////////////////////////////////////////////

  void* _currentTransaction;

////////////////////////////////////////////////////////////////////////////////
/// @brief resolver of currently running transaction
////////////////////////////////////////////////////////////////////////////////

  void* _resolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to the server (TRI_server_t*)
////////////////////////////////////////////////////////////////////////////////

  void* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to the vocbase (TRI_vocbase_t*)
////////////////////////////////////////////////////////////////////////////////

  void* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not useDatabase() is allowed
////////////////////////////////////////////////////////////////////////////////

  bool _allowUseDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not dead objects (ex-wrapped V8 objects) have been
/// deallocated in the context
////////////////////////////////////////////////////////////////////////////////

  bool _hasDeadObjects;

// -----------------------------------------------------------------------------
// --SECTION--                                                           GENERAL
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to the startup loader (JSLoader*)
////////////////////////////////////////////////////////////////////////////////

  void* _loader;

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel has been caught
////////////////////////////////////////////////////////////////////////////////

  bool _canceled;

}
TRI_v8_global_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  GLOBAL FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a global context
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_t* TRI_CreateV8Globals(v8::Isolate*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a method to the prototype of an object
////////////////////////////////////////////////////////////////////////////////

template <typename TARGET>
void TRI_V8_AddProtoMethod (TARGET tpl,
                            const char* const name,
                            v8::InvocationCallback callback,
                            const bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->PrototypeTemplate()->Set(TRI_V8_SYMBOL(name),
                                  v8::FunctionTemplate::New(callback),
                                  v8::DontEnum);
  }

  // normal method
  else {
    tpl->PrototypeTemplate()->Set(TRI_V8_SYMBOL(name),
                                  v8::FunctionTemplate::New(callback));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a method to an object
////////////////////////////////////////////////////////////////////////////////

template <typename TARGET>
inline void TRI_V8_AddMethod (TARGET tpl,
                              const char* const name,
                              v8::Handle<v8::FunctionTemplate> callback,
                              const bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->Set(TRI_V8_SYMBOL(name),
             callback->GetFunction(),
             v8::DontEnum);
  }

  // normal method
  else {
    tpl->Set(TRI_V8_SYMBOL(name),
             callback->GetFunction());
  }
}

template <typename TARGET>
inline void TRI_V8_AddMethod (TARGET tpl,
                              const char* const name,
                              v8::InvocationCallback callback,
                              const bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->Set(TRI_V8_SYMBOL(name),
             v8::FunctionTemplate::New(callback)->GetFunction(),
             v8::DontEnum);
  }

  // normal method
  else {
    tpl->Set(TRI_V8_SYMBOL(name),
             v8::FunctionTemplate::New(callback)->GetFunction());
  }
}

template <>
inline void TRI_V8_AddMethod (v8::Handle<v8::FunctionTemplate> tpl,
                              const char* const name,
                              v8::InvocationCallback callback,
                              const bool isHidden) {
  TRI_V8_AddMethod(tpl->GetFunction(), name, callback, isHidden);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a method to an object
////////////////////////////////////////////////////////////////////////////////

void TRI_AddMethodVocbase (v8::Handle<v8::ObjectTemplate> tpl,
                           const char* const name,
                           v8::Handle<v8::Value>(*func)(v8::Arguments const&),
                           const bool isHidden = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Value>(*func)(v8::Arguments const&),
                                   const bool isHidden = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Function> func,
                                   const bool isHidden = false);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global read-only variable to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalVariableVocbase (v8::Handle<v8::Context> context,
                                   const char* const name,
                                   v8::Handle<v8::Value> value);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
