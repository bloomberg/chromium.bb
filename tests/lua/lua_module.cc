/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// This is the lua sample. It allows bindings of objects between Javascript and
// lua. See lua.html for an example.
// It runs as a native client NPAPI plug-in.

extern "C" {
#include "lua-5.1.4/src/lua.h"
#include "lua-5.1.4/src/lauxlib.h"
#include "lua-5.1.4/src/lualib.h"
}
#include <nacl/nacl_npapi.h>
#include <nacl/npupp.h>
#include <string>

// Basic idea: we create a Lua state for each plug-in instance.
// Native types (nil/null, bools, numbers, strings) are trivially mapped into
// one another.
// Objects created on the Lua side are wrapped into NPLuaObjects if they are
// tables or functions, and exposed to the browser, property lookups mapped to
// field lookups (in the case of tables), invokes are mapped into calling the
// function (in the case of a function, or a table with __call implemented in
// the meta table).
// Objects created on the browser side (NPObjects) are wrapped into a user data
// type that provide the metatable functions to forward property lookups and
// calls onto the NPObject. See members of the lua_npobject_wrapper for that.
// Any time a Lua object gets wrapped into a NPObject and vice versa, a
// mapping, both directions, is added to the Lua registry, to be able to re-use
// these wrappers.
//
// NOTE: as far as accessing fields/property by integer (e.g. array look-ups),
// Lua uses the 1..n notation, whereas JavaScript uses the 0..n-1 notation, but
// no conversion is being done here.

// Key to store the NPP pointer in the registry. There is exactly one NPP per
// Lua state (and vice versa).
// Note: the pointer is used as the key, not the string, to ensure uniqueness.
static const char *kNPPKey = "NPP";

// Gets the NPP (plugin instance) from the Lua state.
static NPP GetNPP(lua_State *lua) {
  lua_pushlightuserdata(lua, const_cast<char *>(kNPPKey));
  lua_rawget(lua, LUA_REGISTRYINDEX);
  NPP npp = static_cast<NPP>(const_cast<void *>(lua_topointer(lua, -1)));
  lua_pop(lua, 1);
  return npp;
}

// Push a NPvariant into the Lua stack, after conversion / wrapping.
void PushVariant(lua_State *lua, const NPVariant &variant);
// Pops the top object from the Lua stack, and converts / wraps it into a
// NPVariant.
void PopVariant(lua_State *lua, NPVariant *variant);

// The lua_npobject_wrapper namespace contains functions relating to the
// userdata objects wrapping NPObjects.
namespace lua_npobject_wrapper {

// Userdata type: a single pointer to a NPObject.
typedef NPObject *Data;

// The handle to the userdata's metatable.
static const char *kHandle = "NPObject Wrapper";

// Creates a wrapper userdata for a NPObject, and pushes it onto the Lua stack.
// This will ref the NPObject.
static void Create(lua_State *lua, NPObject *object) {
  Data *ref = static_cast<Data *>(lua_newuserdata(lua, sizeof(Data)));
  NPN_RetainObject(object);
  *ref = object;
  luaL_getmetatable(lua, kHandle);
  lua_setmetatable(lua, -2);

  // registry[lua_object] = np_object.
  lua_pushvalue(lua, -1);
  lua_pushlightuserdata(lua, object);
  lua_rawset(lua, LUA_REGISTRYINDEX);

  // registry[np_object] = lua_object.
  lua_pushlightuserdata(lua, object);
  lua_pushvalue(lua, -2);
  lua_rawset(lua, LUA_REGISTRYINDEX);
}

// Gets the userdata object that is at the top of the Lua stack.
static Data *GetTop(lua_State *lua) {
  return static_cast<Data *>(luaL_checkudata(lua, 1, kHandle));
}

// Gets the NPObject wrapped by the userdata object that is at the top of the
// Lua stack.
static NPObject *GetNPObject(lua_State *lua) {
  return *GetTop(lua);
}

// metatable.__gc implentation for the userdata object. This will clean up the
// registry mappings, and release the NPObject.
//
// Input stack:
// 1- userdata object.
//
// Output stack:
// (empty)
static int Gc(lua_State *lua) {
  Data *ref = GetTop(lua);

  // registry[lua_object] = nil.
  lua_pushvalue(lua, 1);
  lua_pushnil(lua);
  lua_rawset(lua, LUA_REGISTRYINDEX);

  // registry[np_object] = nil.
  lua_pushlightuserdata(lua, *ref);
  lua_pushnil(lua);
  lua_rawset(lua, LUA_REGISTRYINDEX);

  NPN_ReleaseObject(*ref);
  *ref = NULL;
  return 0;
}

// metatable.__tostring implementation for the userdata object. Provides a
// string representation of it.
//
// Input stack:
// 1- userdata object.
//
// Output stack:
// 1- string representation.
static int ToString(lua_State *lua) {
  NPObject *object = GetNPObject(lua);
  lua_pushfstring(lua, "NPObject(%p)", object);
  return 1;
}

// C closure for invoking a method on a NPObject. This function wraps a
// NPObject method, when accessed as a property. It has 2 upvalues, the
// NPObject and the NPIdentifier representing the function.
// When this function gets called, it will call NPN_Invoke on the NPObject,
// after having converted arguments coming from the Lua stack. If successful,
// it will push the result onto the Lua stack (after conversion), otherwise it
// will raise a Lua error.
//
// Upvalues:
// 1- The wrapped NPObject containing the method.
// 2- The NPIdentifier of the method name.
//
// Input stack:
// 1..n- The method parameters.
//
// Output stack:
// 1- The method result.
static int LuaInvoke(lua_State *lua) {
  unsigned int arg_count = lua_gettop(lua);
  NPObject *object = static_cast<NPObject *>(const_cast<void *>(
      lua_topointer(lua, lua_upvalueindex(1))));
  NPIdentifier id = static_cast<NPIdentifier>(const_cast<void *>(
      lua_topointer(lua, lua_upvalueindex(2))));
  NPP npp = GetNPP(lua);
  NPVariant *args = NULL;
  if (arg_count > 0) {
    args = new NPVariant[arg_count];
    for (unsigned int i = 0; i < arg_count; ++i) {
      lua_pushvalue(lua, i + 1);
      PopVariant(lua, args + i);
    }
  }
  NPVariant result;
  bool r = NPN_Invoke(npp, object, id, args, arg_count, &result);
  if (args) {
    for (unsigned int i = 0; i < arg_count; ++i) {
      NPN_ReleaseVariantValue(args + i);
    }
    delete args;
  }
  if (!r) {
    return luaL_error(lua, "error while calling function on NPObject");
  }
  PushVariant(lua, result);
  NPN_ReleaseVariantValue(&result);
  return 1;
}

// metatable.__index implementation for the userdata object. This does the
// property lookup on the NPObject, and if successful, converts the result, and
// pushes it onto the stack. If the NPObject doesn't have a property with the
// field name, but has a method, it will push a C closure that will call that
// method on that object when invoked. It will push nil in case of failure.
//
// Input stack:
// 1- The userdata object.
// 2- The field name.
//
// Output stack:
// 1- The result.
static int Index(lua_State *lua) {
  NPObject *object = GetNPObject(lua);
  const char *field = lua_tostring(lua, 2);
  if (!field) {
    return luaL_error(lua, "keys must be string or number");
  }
  NPP npp = GetNPP(lua);
  NPIdentifier id = NPN_GetStringIdentifier(field);
  NPVariant result;
  if (NPN_GetProperty(npp, object, id, &result)) {
    PushVariant(lua, result);
    NPN_ReleaseVariantValue(&result);
  } else {
    if (NPN_HasMethod(npp, object, id)) {
      lua_pushlightuserdata(lua, object);
      lua_pushlightuserdata(lua, id);
      lua_pushcclosure(lua, LuaInvoke, 2);
    } else {
      lua_pushnil(lua);
    }
  }
  return 1;
}

// metatable.__newindex implementation for the userdata object. This will set
// a property with the field name on the NPObject, with the converted lua
// value.
//
// Input stack:
// 1- The userdata object.
// 2- The field name.
// 3- The field value.
//
// Output stack:
// (empty)
static int NewIndex(lua_State *lua) {
  NPObject *object = GetNPObject(lua);
  const char *field = lua_tostring(lua, 2);
  if (!field) {
    return luaL_error(lua, "keys must be string or number");
  }
  NPP npp = GetNPP(lua);
  NPIdentifier id = NPN_GetStringIdentifier(field);
  NPVariant value;
  lua_pushvalue(lua, 3);
  PopVariant(lua, &value);
  bool r = NPN_SetProperty(npp, object, id, &value);
  NPN_ReleaseVariantValue(&value);
  if (!r) {
    return luaL_error(lua, "SetProperty failed");
  }
  return 0;
}

// metatable.__call implementation for the userdata object. This will call
// InvokeDefault on the NPObject, after converting / wrapping the parameters.
// If successful, it will push the converted / wrapped return value, otherwise
// it will raise a Lua error.
//
// Input stack:
// 1- The userdata object.
// 2..n+1- The function parameters.
//
// Output stack:
// 1- The return value.
static int Call(lua_State *lua) {
  unsigned int arg_count = lua_gettop(lua) - 1;
  NPObject *object = GetNPObject(lua);
  NPP npp = GetNPP(lua);
  NPVariant *args = NULL;
  if (arg_count > 0) {
    args = new NPVariant[arg_count];
    for (unsigned int i = 0; i < arg_count; ++i) {
      lua_pushvalue(lua, i + 2);
      PopVariant(lua, args + i);
    }
  }
  NPVariant result;
  bool r = NPN_InvokeDefault(npp, object, args, arg_count, &result);
  if (args) {
    for (unsigned int i = 0; i < arg_count; ++i) {
      NPN_ReleaseVariantValue(args + i);
    }
    delete args;
  }
  if (!r) {
    return luaL_error(lua, "error while calling NPObject");
  }
  PushVariant(lua, result);
  NPN_ReleaseVariantValue(&result);
  return 1;
}

static const luaL_Reg kRegitration[] = {
  {"__index", Index},
  {"__newindex", NewIndex},
  {"__call", Call},
  {"__gc", Gc},
  {"__tostring", ToString},
  {NULL, NULL}
};

// Registers the userdata object, by creating a metatable with the above
// implementations.
static void Register(lua_State *lua) {
  luaL_newmetatable(lua, kHandle);
  luaL_register(lua, NULL, kRegitration);
  lua_pop(lua, 1);
}

}  // namespace lua_npobject_wrapper

// A NPObject wrapper for Lua tables and functions.
class NPLuaObject : public NPObject {
 public:
  // Creates a NPLuaObject from a table or a function present at the top of the
  // Lua stack. This does *not* pop the Lua object from the Lua stack.
  static NPLuaObject *Create(lua_State *lua);

  // Pushes the lua object wrapped by this NPLuaObject onto the Lua stack.
  void PushObject() {
    lua_pushlightuserdata(lua_, this);
    lua_rawget(lua_, LUA_REGISTRYINDEX);
    // DCHECK(lua_istable(lua_, -1) || lua_isfunction(lua_, 1));
  }

  static NPClass np_class_;
 private:
  explicit NPLuaObject() : lua_(NULL) {}
  ~NPLuaObject();

  static NPObject *Allocate(NPP npp, NPClass *npclass) {
    return new NPLuaObject();
  }
  static void Deallocate(NPObject *object) {
    delete static_cast<NPLuaObject *>(object);
  }

  static bool HasMethod(NPObject *header, NPIdentifier name);
  static bool Invoke(NPObject *header, NPIdentifier name,
                     const NPVariant *args, uint32_t argCount,
                     NPVariant *result);
  static bool InvokeDefault(NPObject *header, const NPVariant *args,
                            uint32_t argCount, NPVariant *result);
  static bool HasProperty(NPObject *header, NPIdentifier name);
  static bool GetProperty(NPObject *header, NPIdentifier name,
                          NPVariant *variant);
  static bool SetProperty(NPObject *header, NPIdentifier name,
                          const NPVariant *variant);
  static bool RemoveProperty(NPObject *header, NPIdentifier name);
  /*
  static bool Enumerate(NPObject *header, NPIdentifier **value,
                        uint32_t *count);
  */

  // The Lua state that was used to create the object wrapped by this
  // NPLuaObject.
  lua_State *lua_;
};

NPClass NPLuaObject::np_class_ = {
  NP_CLASS_STRUCT_VERSION,
  NPLuaObject::Allocate,
  NPLuaObject::Deallocate,
  0,
  NPLuaObject::HasMethod,
  NPLuaObject::Invoke,
  NPLuaObject::InvokeDefault,
  NPLuaObject::HasProperty,
  NPLuaObject::GetProperty,
  NPLuaObject::SetProperty,
  NPLuaObject::RemoveProperty,
  0,  // NPLuaObject::Enumerate
};

NPLuaObject *NPLuaObject::Create(lua_State *lua) {
  // DCHECK(lua_istable(lua, -1) || lua_isfunction(lua, -1));
  NPLuaObject *object = static_cast<NPLuaObject *>(
      NPN_CreateObject(GetNPP(lua), &np_class_));
  object->lua_ = lua;

  // registry[lua_object] = np_object.
  lua_pushvalue(lua, -1);
  lua_pushlightuserdata(lua, object);
  lua_rawset(lua, LUA_REGISTRYINDEX);
  // registry[np_object] = lua_object.
  lua_pushlightuserdata(lua, object);
  lua_pushvalue(lua, -2);
  lua_rawset(lua, LUA_REGISTRYINDEX);
  return object;
}

NPLuaObject::~NPLuaObject() {
  PushObject();
  // registry[lua_object] = nil.
  lua_pushvalue(lua_, -1);
  lua_pushnil(lua_);
  lua_rawset(lua_, LUA_REGISTRYINDEX);
  // registry[np_object] = nil.
  lua_pushlightuserdata(lua_, this);
  lua_pushnil(lua_);
  lua_rawset(lua_, LUA_REGISTRYINDEX);
  lua_pop(lua_, 1);
}

// Pushes the representation of the NPIdentifier onto the Lua stack, as either
// an integer or a string.
void PushNPIdentifier(lua_State *lua, NPIdentifier name) {
  if (NPN_IdentifierIsString(name)) {
    NPUTF8* utf8_name = NPN_UTF8FromIdentifier(name);
    lua_pushstring(lua, utf8_name);
    NPN_MemFree(utf8_name);
  } else {
    lua_pushinteger(lua, NPN_IntFromIdentifier(name));
  }
}

// Converts / wraps a NPVariant into a Lua type and pushes it onto the Lua
// stack.
// Note: This function will in the end push exactly 1 value onto the stack, but
// may need up to 3 slots to function. Caller must ensure that enough room is
// available.
void PushVariant(lua_State *lua, const NPVariant &variant) {
  if (NPVARIANT_IS_VOID(variant) || NPVARIANT_IS_NULL(variant)) {
    lua_pushnil(lua);
  } else if (NPVARIANT_IS_BOOLEAN(variant)) {
    lua_pushboolean(lua, NPVARIANT_TO_BOOLEAN(variant));
  } else if (NPVARIANT_IS_DOUBLE(variant)) {
    lua_pushnumber(lua, NPVARIANT_TO_DOUBLE(variant));
  } else if (NPVARIANT_IS_INT32(variant)) {
    lua_pushinteger(lua, NPVARIANT_TO_INT32(variant));
  } else if (NPVARIANT_IS_STRING(variant)) {
    NPString string = NPVARIANT_TO_STRING(variant);
    lua_pushlstring(lua, string.UTF8Characters, string.UTF8Length);
  } else {
    NPObject *object = NPVARIANT_TO_OBJECT(variant);
    // If we already have a mapping for this NPObject, use it (this can be
    // either a Lua object that was previously wrapped into a NPLuaObject, or
    // an already existing userdata wrapper for a browser NPObject), otherwise
    // create a new userdata wrapper.
    lua_pushlightuserdata(lua, object);
    lua_rawget(lua, LUA_REGISTRYINDEX);
    if (lua_isnil(lua, -1)) {
      lua_pop(lua, 1);
      lua_npobject_wrapper::Create(lua, object);
    }
  }
}

// Converts / wraps a Lua type from the top of the Lua stack into a NPVariant,
// and pops it from the Lua stack.
// The created NPVariant needs to be released with NPN_ReleaseVariantValue.
// Note: This function will in the end pop exactly 1 value onto the stack, but
// may need up to 3 empty slots to work. Caller must ensure that enough room is
// available.
void PopVariant(lua_State *lua, NPVariant *variant) {
  switch (lua_type(lua, -1)) {
    case LUA_TNIL:
      NULL_TO_NPVARIANT(*variant);
      break;
    case LUA_TBOOLEAN:
      BOOLEAN_TO_NPVARIANT((lua_toboolean(lua, -1) != 0), *variant);
      break;
    case LUA_TNUMBER:
      DOUBLE_TO_NPVARIANT(lua_tonumber(lua, -1), *variant);
      break;
    case LUA_TSTRING: {
      size_t len = 0;
      const char *string = lua_tolstring(lua, -1, &len);
      NPUTF8* utf8_chars = static_cast<NPUTF8*>(NPN_MemAlloc(len));
      memcpy(utf8_chars, string, len);
      STRINGN_TO_NPVARIANT(utf8_chars, len, *variant);
      break;
    }
    case LUA_TTABLE:
    case LUA_TUSERDATA:
    case LUA_TFUNCTION: {
      // If we already have a mapping for this Lua object, use it (this can be
      // either a pre-existing NPLuaObject that wraps it, or the NPObject that
      // is wrapped by the userdata), otherwise create a new NPLuaObject that
      // wraps it.
      lua_pushvalue(lua, -1);
      lua_rawget(lua, LUA_REGISTRYINDEX);
      NPObject *object = NULL;
      if (lua_isnil(lua, -1)) {
        lua_pop(lua, 1);
        object = NPLuaObject::Create(lua);
      } else {
        object = static_cast<NPObject *>(const_cast<void *>(
            lua_topointer(lua, -1)));
        NPN_RetainObject(object);
        lua_pop(lua, 1);
      }
      OBJECT_TO_NPVARIANT(object, *variant);
      break;
    }
    case LUA_TNONE:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
      VOID_TO_NPVARIANT(*variant);
      break;
  }
  lua_pop(lua, 1);
}

// NPRuntime HasMethod implementation: returns true if the NPLuaObject wraps a
// table (not a function), and the wrapped table has a property of that name,
// that is a function.
bool NPLuaObject::HasMethod(NPObject *header, NPIdentifier name) {
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;
  object->PushObject();
  if (!lua_istable(lua, -1)) {
    lua_pop(lua, 1);
    return false;
  }
  PushNPIdentifier(lua, name);
  lua_gettable(lua, -2);
  bool result = lua_isfunction(lua, -1);
  lua_pop(lua, 2);
  return result;
}

// NPRuntime HasProperty implementation: returns true if the NPLuaObject wraps a
// table. Note that this will return true whether or not the field exists, so
// that values can be set on non-yet-existing fields of the table.
bool NPLuaObject::HasProperty(NPObject *header, NPIdentifier name) {
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;
  object->PushObject();
  bool result = lua_istable(lua, -1);
  lua_pop(lua, 1);
  return result;
}

// NPRuntime Invoke implementation: converts/wraps parameters, push them onto
// the stack, invoke the Lua function, pops the result and converts/wraps it
// into the NPVariant result. This will fail if the wrapped Lua object is not a
// table.
// NOTE: in case of error while calling the Lua function, this returns false
// but doesn't raise an exception, so that Firebug can properly find the place
// where this is happening. The error gets printed out on stdout though, with
// the debug NaCl plug-in, for debugging.
bool NPLuaObject::Invoke(NPObject *header, NPIdentifier name,
                         const NPVariant *args, uint32_t argCount,
                         NPVariant *result) {
  printf("Invoke %s\n", NPN_UTF8FromIdentifier(name));
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;

  object->PushObject();
  if (!lua_istable(lua, -1)) {
    lua_pop(lua, 1);
    return false;
  }
  PushNPIdentifier(lua, name);
  lua_gettable(lua, -2);
  // Make sure we have enough room on the stack to allocate the function
  // parameters.
  lua_checkstack(lua, argCount + 3);
  for (unsigned int i = 0; i < argCount; ++i) {
    PushVariant(lua, args[i]);
  }
  int r = lua_pcall(lua, argCount, 1, 0);
  if (r != 0) {
    printf("Lua error: %s\n", lua_tostring(lua, -1));
    lua_pop(lua, 2);
    return false;
  } else {
    PopVariant(lua, result);
    lua_pop(lua, 1);
    return true;
  }
}

// NPRuntime InvokeDefault implementation: converts/wraps parameters, push them
// onto the stack, invoke the Lua function, pops the result and converts/wraps
// it into the NPVariant result.
// NOTE: in case of error while calling the Lua function, this returns false
// but doesn't raise an exception, so that Firebug can properly find the place
// where this is happening. The error gets printed out on stdout though, with
// the debug NaCl plug-in, for debugging.
bool NPLuaObject::InvokeDefault(NPObject *header, const NPVariant *args,
                                uint32_t argCount, NPVariant *result) {
  printf("InvokeDefault\n");
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;

  object->PushObject();
  // Make sure we have enough room on the stack to allocate the function
  // parameters.
  lua_checkstack(lua, argCount + 3);
  for (unsigned int i = 0; i < argCount; ++i) {
    PushVariant(lua, args[i]);
  }
  int r = lua_pcall(lua, argCount, 1, 0);
  if (r != 0) {
    printf("Lua error: %s\n", lua_tostring(lua, -1));
    lua_pop(lua, 1);
    return false;
  } else {
    PopVariant(lua, result);
    return true;
  }
}

// NPRuntime GetProperty implementation: gets the field from the table,
// converts/wraps the result into the NPVariant result.
bool NPLuaObject::GetProperty(NPObject *header, NPIdentifier name,
                              NPVariant *variant) {
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;

  object->PushObject();
  if (!lua_istable(lua, -1)) {
    lua_pop(lua, 1);
    return false;
  }
  PushNPIdentifier(lua, name);
  lua_gettable(lua, -2);
  PopVariant(lua, variant);
  lua_pop(lua, 1);
  return true;
}

// NPRuntime SetProperty implementation: converts/wraps the value and sets it
// into the the field in the table.
bool NPLuaObject::SetProperty(NPObject *header, NPIdentifier name,
                              const NPVariant *variant) {
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;

  object->PushObject();
  if (!lua_istable(lua, -1)) {
    lua_pop(lua, 1);
    return false;
  }
  PushNPIdentifier(lua, name);
  PushVariant(lua, *variant);
  lua_settable(lua, -3);
  lua_pop(lua, 1);
  return true;
}

// NPRuntime SetProperty implementation: sets nil into the the field in the
// table.
bool NPLuaObject::RemoveProperty(NPObject *header, NPIdentifier name) {
  NPLuaObject *object = static_cast<NPLuaObject *>(header);
  lua_State *lua = object->lua_;

  object->PushObject();
  if (!lua_istable(lua, -1)) {
    lua_pop(lua, 1);
    return false;
  }
  PushNPIdentifier(lua, name);
  lua_pushnil(lua);
  lua_settable(lua, -3);
  lua_pop(lua, 1);
  return true;
}

/* TODO: implement enumerate using lua_next().
bool NPLuaObject::Enumerate(NPObject *header, NPIdentifier **value,
                            uint32_t *count);
*/

// Scriptable object for the plug-in, provides the glue with the browser.
// It exposes one method "execute" to execute lua code into the Lua context,
// and one property "lua" that exposes the Lua global table.
class Plugin : public NPObject {
 public:
  NPError SetWindow(NPWindow *window) { return NPERR_NO_ERROR;}

  static NPClass *GetNPClass() {
    return &class_;
  }

 private:
  explicit Plugin(NPP npp);
  ~Plugin();

  // Executes code in the Lua context.
  bool Execute(const char *code);

  static NPObject *Allocate(NPP npp, NPClass *npclass);
  static void Deallocate(NPObject *object);
  static bool HasMethod(NPObject *header, NPIdentifier name);
  static bool Invoke(NPObject *header, NPIdentifier name,
                     const NPVariant *args, uint32_t argCount,
                     NPVariant *result);
  static bool InvokeDefault(NPObject *header, const NPVariant *args,
                            uint32_t argCount, NPVariant *result);
  static bool HasProperty(NPObject *header, NPIdentifier name);
  static bool GetProperty(NPObject *header, NPIdentifier name,
                          NPVariant *variant);
  static bool SetProperty(NPObject *header, NPIdentifier name,
                          const NPVariant *variant);
  static bool Enumerate(NPObject *header, NPIdentifier **value,
                        uint32_t *count);

  NPP npp_;
  static NPClass class_;
  NPIdentifier execute_id_;
  NPIdentifier lua_id_;

  lua_State *lua_;
  static int LuaPrint(lua_State *lua);
};

// Prints the string representation of a value onto stdout. This is a debugging
// helper, and will only print anything with the debug NaCl plugin.
//
// Input stack:
// 1- The value to print.
//
// Output stack:
// (empty)
int Plugin::LuaPrint(lua_State *lua) {
  lua_getglobal(lua, "tostring");
  lua_pushvalue(lua, 1);
  lua_pcall(lua, 1, 1, 0);
  const char *str = lua_tostring(lua, -1);
  printf("%s\n", str ? str : "(null)");
  return 0;
}

Plugin::Plugin(NPP npp)
    : npp_(npp) {
  const char *names[2] = {"execute", "lua"};
  NPIdentifier ids[2];
  NPN_GetStringIdentifiers(names, 2, ids);
  execute_id_ = ids[0];
  lua_id_ = ids[1];
  // Create the Lua context, and register side-effect free libraries.
  lua_ = lua_open();
  luaopen_base(lua_);
  luaopen_table(lua_);
  luaopen_string(lua_);
  luaopen_math(lua_);
  luaopen_debug(lua_);
  // Register the NPobject lua wrapper, and the print helper function.
  lua_npobject_wrapper::Register(lua_);
  lua_register(lua_, "print", &LuaPrint);
  // Register the NPP instance into the registry.
  lua_pushlightuserdata(lua_, const_cast<char *>(kNPPKey));
  lua_pushlightuserdata(lua_, npp);
  lua_rawset(lua_, LUA_REGISTRYINDEX);
}

Plugin::~Plugin() {
  lua_close(lua_);
}

bool Plugin::Execute(const char *code) {
  if (luaL_loadstring(lua_, code)) {
    printf("lua loadstring error: %s\n", lua_tostring(lua_, -1));
    lua_pop(lua_, 1);
    return false;
  }
  if (lua_pcall(lua_, 0, 0, 0)) {
    printf("lua pcall error: %s\n", lua_tostring(lua_, -1));
    lua_pop(lua_, 1);
    return false;
  }
  return true;
}

// NPRuntime glue for the plugin scriptable object.

NPClass Plugin::class_ = {
  NP_CLASS_STRUCT_VERSION,
  Plugin::Allocate,
  Plugin::Deallocate,
  0,
  Plugin::HasMethod,
  Plugin::Invoke,
  0,
  Plugin::HasProperty,
  Plugin::GetProperty,
  Plugin::SetProperty,
  0,
  Plugin::Enumerate,
};

NPObject *Plugin::Allocate(NPP npp, NPClass *npclass) {
  return new Plugin(npp);
}

void Plugin::Deallocate(NPObject *object) {
  delete static_cast<Plugin *>(object);
}

bool Plugin::HasMethod(NPObject *header, NPIdentifier name) {
  Plugin *plugin = static_cast<Plugin *>(header);
  return name == plugin->execute_id_;
}

bool Plugin::Invoke(NPObject *header, NPIdentifier name,
                    const NPVariant *args, uint32_t argCount,
                    NPVariant *result) {
  Plugin *plugin = static_cast<Plugin *>(header);
  VOID_TO_NPVARIANT(*result);
  if (name == plugin->execute_id_ && argCount == 1 &&
      NPVARIANT_IS_STRING(args[0])) {
    NPString string = NPVARIANT_TO_STRING(args[0]);
    std::string s(string.UTF8Characters, string.UTF8Length);
    return plugin->Execute(s.c_str());
  } else {
    return false;
  }
}

bool Plugin::InvokeDefault(NPObject *header, const NPVariant *args,
                           uint32_t argCount, NPVariant *result) {
  return false;
}

bool Plugin::HasProperty(NPObject *header, NPIdentifier name) {
  Plugin *plugin = static_cast<Plugin *>(header);
  return name == plugin->lua_id_;
}

bool Plugin::GetProperty(NPObject *header, NPIdentifier name,
                         NPVariant *variant) {
  Plugin *plugin = static_cast<Plugin *>(header);
  VOID_TO_NPVARIANT(*variant);
  if (name == plugin->lua_id_) {
    lua_getglobal(plugin->lua_, "_G");
    PopVariant(plugin->lua_, variant);
    return true;
  } else {
    return false;
  }
}

bool Plugin::SetProperty(NPObject *header, NPIdentifier name,
                         const NPVariant *variant) {
  return false;
}

bool Plugin::Enumerate(NPObject *header, NPIdentifier **value,
                       uint32_t *count) {
  Plugin *plugin = static_cast<Plugin *>(header);
  *count = 1;
  NPIdentifier *ids = static_cast<NPIdentifier *>(
      NPN_MemAlloc(*count * sizeof(NPIdentifier)));
  ids[0] = plugin->execute_id_;
  *value = ids;
  return true;
}

// NPP glue.

NPError NPP_New(NPMIMEType mime_type, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[],
                NPSavedData* saved) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  NPObject *object = NPN_CreateObject(instance, Plugin::GetNPClass());
  if (object == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  instance->pdata = object;
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    NPN_ReleaseObject(plugin);
  }
  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  if (window == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    return plugin->SetWindow(window);
  }
  return NPERR_GENERIC_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* ret_value) {
  if (NPPVpluginScriptableNPObject == variable) {
    if (instance == NULL) {
      return NPERR_INVALID_INSTANCE_ERROR;
    }
    NPObject* object = static_cast<NPObject*>(instance->pdata);
    if (NULL != object) {
      NPN_RetainObject(object);
    }
    *reinterpret_cast<NPObject**>(ret_value) = object;
    return NPERR_NO_ERROR;
  }
  return NPERR_INVALID_PARAM;
}

/*
 * NP_Initialize
 */

extern "C" NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                                 NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->getvalue = NPP_GetValue;
  return NPERR_NO_ERROR;
}
