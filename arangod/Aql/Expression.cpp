////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, expression
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Expression.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Types.h"
#include "Aql/V8Executor.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/json.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the expression
////////////////////////////////////////////////////////////////////////////////

Expression::Expression (V8Executor* executor,
                        AstNode const* node)
  : _executor(executor),
    _node(node),
    _type(UNPROCESSED),
    _canThrow(true) {

  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
  
  analyzeExpression();
}

Expression::Expression (triagens::aql::Query* Q,
                        triagens::basics::Json const& json)
  : _executor(Q->executor()),
    _node(Q->registerNode(new AstNode(Q, json.get("expression")))),
    _type(UNPROCESSED) {

  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
  analyzeExpression();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the expression
////////////////////////////////////////////////////////////////////////////////

Expression::~Expression () {
  if (_type == V8) {
    delete _func;
  }
  else if (_type == JSON) {
    TRI_ASSERT(_data != nullptr);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
    // _json is freed automatically by AqlItemBlock
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Expression::variables () const {
  return Ast::getReferencedVariables(_node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::execute (AQL_TRANSACTION_V8* trx,
                              std::vector<TRI_document_collection_t const*>& docColls,
                              std::vector<AqlValue>& argv,
                              size_t startPos,
                              std::vector<Variable*> const& vars,
                              std::vector<RegisterId> const& regs) {

  TRI_ASSERT(_type != UNPROCESSED);

  // and execute
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_data != nullptr);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _data, Json::NOFREE));
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      return _func->execute(trx, docColls, argv, startPos, vars, regs);
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, nullptr, trx, docColls, argv, startPos, vars, regs);
    }
 
    case UNPROCESSED: {
      // fall-through to exception
    }
  }
      
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid simple expression");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (and, if appropriate, compile it into 
/// executable code)
////////////////////////////////////////////////////////////////////////////////

void Expression::analyzeExpression () {
  TRI_ASSERT(_type == UNPROCESSED);

  if (_node->isConstant()) {
    // generate a constant value
    _data = _node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (_data == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid json in simple expression");
    }

    _type = JSON;
    _canThrow = false;
  }
  else if (_node->isSimple()) {
    _type = SIMPLE;
    _canThrow = _node->canThrow();
  }
  else {
    // generate a V8 expression
    _func = _executor->generateExpression(_node);
    _type = V8;
    _canThrow = _node->canThrow();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpression (AstNode const* node,
                                              TRI_document_collection_t const** collection, 
                                              AQL_TRANSACTION_V8* trx,
                                              std::vector<TRI_document_collection_t const*>& docColls,
                                              std::vector<AqlValue>& argv,
                                              size_t startPos,
                                              std::vector<Variable*> const& vars,
                                              std::vector<RegisterId> const& regs) {

  if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    // array lookup, e.g. users.name
    TRI_ASSERT(node->numMembers() == 1);

    auto member = node->getMember(0);
    auto name = static_cast<char const*>(node->getData());

    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    auto j = result.extractArrayMember(trx, myCollection, name);
    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
  }
  
  else if (node->type == NODE_TYPE_INDEXED_ACCESS) {
    // list lookup, e.g. users[0]
    // note: it depends on the type of the value whether a list lookup or an array lookup is performed
    // for example, if the value is an array, then its elements might be access like this:
    // users['name'] or even users['0'] (as '0' is a valid attribute name, too)
    // if the value is a list, then string indexes might also be used and will be converted to integers, e.g.
    // users['0'] is the same as users[0], users['-2'] is the same as users[-2] etc.
    TRI_ASSERT(node->numMembers() == 2);

    auto member = node->getMember(0);
    auto index = node->getMember(1);

    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    if (result.isList()) {
      if (index->isNumericValue()) {
        auto j = result.extractListMember(trx, myCollection, index->getIntValue());
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      else if (index->isStringValue()) {
        char const* p = index->getStringValue();
        TRI_ASSERT(p != nullptr);

        try {
          // stoll() might throw an exception if the string is not a number
          int64_t position = static_cast<int64_t>(std::stoll(p));
          auto j = result.extractListMember(trx, myCollection, position);
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
        }
        catch (...) {
          // no number found. 
        }
      }
      // fall-through to returning null
    }
    else if (result.isArray()) {
      if (index->isNumericValue()) {
        std::string const indexString = std::to_string(index->getIntValue());
        auto j = result.extractArrayMember(trx, myCollection, indexString.c_str());
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      else if (index->isStringValue()) {
        char const* p = index->getStringValue();
        TRI_ASSERT(p != nullptr);

        auto j = result.extractArrayMember(trx, myCollection, p);
        return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
      }
      // fall-through to returning null
    }
      
    return AqlValue(new Json(Json::Null));
  }
  
  else if (node->type == NODE_TYPE_LIST) {
    auto list = new Json(Json::List);

    try {
      size_t const n = node->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        list->add(result.toJson(trx, myCollection));
      }
      return AqlValue(list);
    }
    catch (...) {
      delete list;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_ARRAY) {
    auto resultArray = new Json(Json::Array);

    try {
      size_t const n = node->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        TRI_ASSERT(member->type == NODE_TYPE_ARRAY_ELEMENT);
        auto key = member->getStringValue();
        member = member->getMember(0);

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        resultArray->set(key, result.toJson(trx, myCollection));
      }
      return AqlValue(resultArray);
    }
    catch (...) {
      delete resultArray;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_REFERENCE) {
    auto v = static_cast<Variable*>(node->getData());

    size_t i = 0;
    for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
      if ((*it)->name == v->name) {
        TRI_ASSERT(collection != nullptr);

        // save the collection info
        *collection = docColls[regs[i]]; 
        return argv[startPos + regs[i]];
      }
    }
    // fall-through to exception
  }
  
  else if (node->type == NODE_TYPE_VALUE) {
    auto j = node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (j == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j)); 
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unhandled type in simple expression");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
