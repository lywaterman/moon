#include "errors.hpp"
#include "lua_utils.hpp"

#include "erl_nif.h"

const int MAX_DEPTH = 20;

namespace lua {
namespace stack {

/////////////////////////////////////////////////////////////////////////////

class push_t : public boost::static_visitor<void>
{
public :
    typedef push_t self_t;
    push_t(lua_State * vm) : vm_(vm) {};
    void operator()(int32_t const& value)
    {
        lua_pushinteger(vm_, value);
    }
    void operator()(int64_t const& value)
    {
        lua_pushnumber(vm_, value);
    }
    void operator()(double const& value)
    {
        lua_pushnumber(vm_, value);
    }
    void operator()(erlcpp::num_t const& value)
    {
        self_t & self = *this;
        boost::apply_visitor(self, value);
    }
    void operator()(erlcpp::lpid_t const& value)
    {
        //lua_pushlightuserdata(vm_, (void*)value.ptr());
        
            
        boost::shared_ptr<ErlNifEnv> env(enif_alloc_env(), enif_free_env);
        ERL_NIF_TERM pid = enif_make_pid(env.get(), value.ptr()); 
        void* p = lua_newuserdata(vm_, sizeof(ERL_NIF_TERM));
        memcpy(p, &pid, sizeof(ERL_NIF_TERM));
        luaL_getmetatable(vm_, "pid_metatable");
        lua_setmetatable(vm_, -2);
        //std::string str = "erltype_pid";
        //
		//lua_pushlstring(vm_, str.c_str(), str.size());
        //throw errors::unsupported_type("unsupported_type_erl_pid");
    }
    void operator()(erlcpp::atom_t const& value)
    {
		if (value == "true")
		{
			lua_pushboolean(vm_, 1);
		}
		else if (value == "false")
		{
			lua_pushboolean(vm_, 0);
		}
		else if (value == "nil" or value == "undefined" or value=="null")
		{
			lua_pushnil(vm_);
		}
		else
		{
			lua_pushlstring(vm_, value.c_str(), value.size());
		}
    }

    void operator()(erlcpp::binary_t const& value)
    {
        lua_pushlstring(vm_, value.data(), value.size());
    }

    void operator()(erlcpp::list_t const& value)
    {
        lua_createtable(vm_, value.size(), 0);
        int32_t index = 1;
        erlcpp::list_t::const_iterator i, end = value.end();
        for( i = value.begin(); i != end; ++i )
        {
            try
            {
                erlcpp::tuple_t tuple = boost::get<erlcpp::tuple_t>(*i);

                if (tuple.size() != 0) {
                  if (tuple.size() != 2)
                  {
                    throw boost::bad_get();
                  }

                  self_t & self = *this;
                  boost::apply_visitor(self, tuple[0]);
                  boost::apply_visitor(self, tuple[1]);
                  lua_settable(vm_, -3);
                }
            }
            catch(boost::bad_get&)
            {
                self_t & self = *this;
                lua_pushinteger(vm_, index++);
                boost::apply_visitor(self, *i);
                lua_settable(vm_, -3);
            }
        }
    }

    void operator()(erlcpp::tuple_t const& value)
    {
        lua_createtable(vm_, value.size(), 0);
        for( erlcpp::tuple_t::size_type i = 0, end = value.size(); i != end; ++i )
        {
            self_t & self = *this;
            lua_pushinteger(vm_, i+1);
            boost::apply_visitor(self, value[i]);
            lua_settable(vm_, -3);
        }
    }

private :
    lua_State * vm_;
};

/////////////////////////////////////////////////////////////////////////////

void push(lua_State * vm, erlcpp::term_t const& val)
{
    push_t p(vm);
    boost::apply_visitor(p, val);
}

void push_all(lua_State * vm, erlcpp::list_t const& list)
{
    push_t p(vm);
    std::for_each(list.begin(), list.end(), boost::apply_visitor(p));
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

erlcpp::term_t peek(lua_State * vm, const void* table_pointer, int depth)
{
    const char * val = lua_typename(vm, lua_type(vm, -1));
    std::string v = val;
    v = "luatype_" + v;
    erlcpp::binary_t default_return = erlcpp::binary_t(v);

  switch( lua_type(vm, -1) )
  {
    case LUA_TUSERDATA: {
      int res = luaL_getmetafield(vm, -1, "type");
      if (res == 0) {
          return default_return;
      } else {
        char* type = (char*)lua_tostring(vm, -1);
        lua_pop(vm, 1);
        if (strcmp(type, "pid") == 0) {
            boost::shared_ptr<ErlNifEnv> env(enif_alloc_env(), enif_free_env);
            ERL_NIF_TERM term = *((ERL_NIF_TERM*)lua_touserdata(vm, -1));
            if (enif_is_pid(env.get(), term)) {
                return erlcpp::from_erl<erlcpp::lpid_t>(env.get(), term);
            } else {
                return default_return;
            };
        } else if(strcmp(type, "atom") == 0) {
             void* p = lua_touserdata(vm, -1);
             size_t len = *((size_t*)p);
            return erlcpp::atom_t(erlcpp::atom_t::data_t((const char*)(p+sizeof(size_t)), len)); 
        } else {
            return default_return;
        }
      }
    }
    case LUA_TNIL:
      return erlcpp::atom_t("nil");
    case LUA_TBOOLEAN:
      return erlcpp::atom_t(lua_toboolean(vm, -1) ? "true" : "false");
    case LUA_TNUMBER:
      {
        lua_Number  d = lua_tonumber(vm, -1);
        lua_Integer i = lua_tointeger(vm, -1);
        if (d != i) {
          return erlcpp::num_t(d);
        } else {
          return erlcpp::num_t(i);
        }
      }
    case LUA_TSTRING:
      {
        std::size_t len = 0;
        const char * val = lua_tolstring(vm, -1, &len);
        return erlcpp::binary_t(erlcpp::binary_t::data_t(val, val+len)); 
      }
    case LUA_TTABLE:
      {
        erlcpp::list_t result;
        erlcpp::list_t result_hash;
      
        //Table的Pointer
        const void* pointer = lua_topointer(vm, -1);

        if (table_pointer == pointer) {
          //外部的表包含一个自己的引用
          std::string v = "(table_self)";
          return erlcpp::binary_t(v);
        }

        if (depth >= MAX_DEPTH) {
          std::string v = "(table)";
          return erlcpp::binary_t(v);
        }

        int cur_depth = depth+1;

        bool is_hash = false;
        lua_pushnil(vm);
        for(int32_t index = 1; lua_next(vm, -2); ++index)
        {
          erlcpp::term_t val = pop(vm, pointer, cur_depth);
          erlcpp::term_t key = peek(vm, NULL, 0);
          try
          {
            if (boost::get<LUA_INTEGER>(boost::get<erlcpp::num_t>(key)) == index)
            {
              result.push_back(val);

              erlcpp::tuple_t pair(2);
              pair[0] = key;
              pair[1] = val;
              result_hash.push_back(pair);

            }
            else
            {
              is_hash = true;
              erlcpp::tuple_t pair(2);
              pair[0] = key;
              pair[1] = val;
              result_hash.push_back(pair);
            }
          }
          catch(boost::bad_get&)
          {
            is_hash = true;
            erlcpp::tuple_t pair(2);
            pair[0] = key;
            pair[1] = val;
            result_hash.push_back(pair);
          }
        }
        if (is_hash) {
          return result_hash;
        }

        int top = lua_gettop(vm);

        //目前确定为数组
        if (result.size() == 0) {
          int res = luaL_getmetafield(vm, -1, "is_hash");

          if (res == 0) {
            return result; 
          } else {
            int is_hash = lua_toboolean(vm, -1);

            lua_settop(vm, top);

            if (is_hash) {
              erlcpp::tuple_t pair(0);
              result_hash.push_back(pair);

              return result_hash;
            } else {
              return result;
            }
          }
        }
        return result;
      }
    default :

      const char * val = lua_typename(vm, lua_type(vm, -1));
      std::string v = val;

      v = "luatype_" + v;
      return erlcpp::binary_t(v);

      //std::string v = lua_typename(vm, lua_type(vm, -1));
      //v = "unsupported_type_lua_" + v;
      //throw errors::unsupported_type(v);

      //throw errors::unsupported_type(lua_typename(vm, lua_type(vm, -1)));
  }
}

erlcpp::term_t pop(lua_State * vm)
{
    return pop(vm, NULL, 0);
}

erlcpp::term_t pop(lua_State * vm, const void* pointer, int depth)
{
    erlcpp::term_t result = peek(vm, pointer, depth);
    lua_pop(vm, 1);
    return result;
}

erlcpp::term_t pop_all(lua_State * vm)
{
    switch(int N = lua_gettop(vm))
    {
        case 0 : return erlcpp::atom_t("undefined");
        case 1 : return pop(vm);
        default:
        {
            erlcpp::tuple_t result(N);
            while(N)
            {
                result[--N] = pop(vm);
            }
            return result;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////

} // namespace stack
} // namespace lua
