////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8_V8__CONV_H
#define ARANGODB_V8_V8__CONV_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "V8/v8-globals.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the keys of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_KeysJson(v8::Isolate* isolate, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the values of a TRI_json_t* object into a V8 array
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ValuesJson(v8::Isolate* isolate, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_json_t into a V8 object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ObjectJson(v8::Isolate* isolate, TRI_json_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a V8 value to a json_t value
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_ObjectToJson(v8::Isolate*, v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a string
////////////////////////////////////////////////////////////////////////////////

std::string TRI_ObjectToString(v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to an int64_t
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_ObjectToInt64(v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts an V8 object to a uint64_t
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_ObjectToUInt64(v8::Handle<v8::Value> const, bool const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a double
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble(v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a double with error handling
////////////////////////////////////////////////////////////////////////////////

double TRI_ObjectToDouble(v8::Handle<v8::Value> const, bool& error);

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a V8 object to a boolean
////////////////////////////////////////////////////////////////////////////////

bool TRI_ObjectToBoolean(v8::Handle<v8::Value> const);

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the V8 conversion module
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Conversions(v8::Handle<v8::Context>);

#endif
