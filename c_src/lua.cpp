#include "lua.hpp"
#include "utils.hpp"
#include "errors.hpp"
#include "lua_utils.hpp"

#include <dlfcn.h>
#include <unistd.h>

extern "C"
{
	#include "luasocket/luasocket.h"
}
#include "pbc-c.h"

extern "C"
{
	#include "cjson/interface.h"
}

extern "C"
{
	#include "luaxml/interface.h"
}

namespace lua {

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// task handlers:
template <class return_type>
class base_handler : public boost::static_visitor<return_type>
{
public :
    typedef typename boost::static_visitor<return_type>::result_type result_type;

    base_handler(vm_t & vm) : vm_(vm) {};
    virtual return_type operator()(vm_t::tasks::load_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::eval_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::call_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::resp_t const&) { throw errors::unexpected_msg(); }
    virtual return_type operator()(vm_t::tasks::quit_t const&) { throw quit_tag(); }

    vm_t & vm() { return vm_; };
    vm_t const& vm() const { return vm_; }

protected :
    ~base_handler() {}

private :
    vm_t & vm_;
};

/////////////////////////////////////////////////////////////////////////////

class result_handler : public base_handler<erlcpp::term_t>
{
public :
    using base_handler<erlcpp::term_t>::operator();
    result_handler(vm_t & vm) : base_handler<erlcpp::term_t>(vm) {};
    erlcpp::term_t operator()(vm_t::tasks::resp_t const& resp) { vm().cur_caller = resp.caller; return resp.term; }
};

struct call_handler : public base_handler<void>
{
    using base_handler<void>::operator();
    call_handler(vm_t & vm) : base_handler<void>(vm) {};

    // Loading file:
    void operator()(vm_t::tasks::load_t const& load)
    {
        vm().cur_caller = load.caller;
        stack_guard_t guard(vm());
        try
        {
            std::string file(load.file.data(), load.file.data() + load.file.size());
            if (luaL_dofile(vm().state(), file.c_str()))
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("error_lua");
                result[1] = lua::stack::pop(vm().state());
                send_result_caller(vm(), "moon_response", result, load.caller);
            }
            else
            {
                erlcpp::atom_t result("ok");
                send_result_caller(vm(), "moon_response", result, load.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, load.caller);
        }
    }

    // Evaluating arbitrary code:
    void operator()(vm_t::tasks::eval_t const& eval)
    {
        vm().cur_caller = eval.caller;
        stack_guard_t guard(vm());
        try
        {
            if ( luaL_loadbuffer(vm().state(), eval.code.data(), eval.code.size(), "line") ||
                    lua_pcall(vm().state(), 0, LUA_MULTRET, 0) )
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("error_lua");
                result[1] = lua::stack::pop(vm().state());
                send_result_caller(vm(), "moon_response", result, eval.caller);
            }
            else
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("ok");
                result[1] = lua::stack::pop_all(vm().state());
                send_result_caller(vm(), "moon_response", result, eval.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, eval.caller);
        }
    }

    // Calling arbitrary function:
    void operator()(vm_t::tasks::call_t const& call)
    {
        vm().cur_caller = call.caller;
        stack_guard_t guard(vm());
        try
        {
			lua_getglobal( vm().state(), "debug" );
			lua_getfield( vm().state(), -1, "traceback" );
			lua_remove( vm().state(), -2 );
			

            lua_getglobal(vm().state(), call.fun.c_str());

            lua::stack::push_all(vm().state(), call.args);

            if (lua_pcall(vm().state(), call.args.size(), LUA_MULTRET, -2-call.args.size()))
            //if (lua_pcall(vm().state(), call.args.size(), LUA_MULTRET, 0))
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("error_lua");
                result[1] = lua::stack::pop(vm().state());
                send_result_caller(vm(), "moon_response", result, call.caller);
            }
            else
            {
                erlcpp::tuple_t result(2);
                result[0] = erlcpp::atom_t("ok");
				lua_remove(vm().state(), 1);
                result[1] = lua::stack::pop_all(vm().state());
                send_result_caller(vm(), "moon_response", result, call.caller);
            }
        }
        catch( std::exception & ex )
        {
            erlcpp::tuple_t result(2);
            result[0] = erlcpp::atom_t("error_lua");
            result[1] = erlcpp::atom_t(ex.what());
            send_result_caller(vm(), "moon_response", result, call.caller);
        }
    }
};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

int erlang_call(vm_t & vm)
{
    bool exception_caught = false; // because lua_error makes longjump
    try
    {
        stack_guard_t guard(vm);

        erlcpp::term_t args = lua::stack::pop_all(vm.state());
        
        if (send_result_vm_with_caller(vm, "moon_callback", args, vm.cur_caller)) {
            erlcpp::term_t result = perform_resp_task<result_handler>(vm);
            lua::stack::push(vm.state(), result);
        } else {
            lua::stack::push(vm.state(), erlcpp::binary_t("send_moon_callback_fail"));
        }

        guard.dismiss();
        return 1;
    }
    catch(std::exception & ex)
    {
        lua::stack::push(vm.state(), erlcpp::atom_t(ex.what()));
        exception_caught = true;
    }

    if (exception_caught) {
        lua_error(vm.state());
    }

    return 0;
}

extern "C"
{
    static int erlang_atom(lua_State* vm)
    {
       int n = lua_gettop(vm); 
       if (n < 1) {
           lua_pushstring(vm, "argument is nil, need string");
           lua_error(vm);
        }
       int is_string = lua_isstring(vm, 1);
       if (is_string) {
           std::size_t len = 0;
           const char * val = lua_tolstring(vm, 1, &len);
           void* p = lua_newuserdata(vm, sizeof(size_t)+(sizeof(char)*len));
           memcpy(p, &len, sizeof(size_t));
           memcpy(p+sizeof(size_t), val, sizeof(char)*len);
           luaL_getmetatable(vm, "atom_metatable");
           lua_setmetatable(vm, -2);
           return 1;
       } else {
          lua_pushstring(vm, "incorrect argument, need string");
          lua_error(vm);
       }
       return 0; 
    }
    static int erlang_call(lua_State * vm)
    {
        int index = lua_upvalueindex(1);
        assert(lua_islightuserdata(vm, index));
        void * data = lua_touserdata(vm, index);
        assert(data);
        return erlang_call(*static_cast<vm_t*>(data));
    }

    static const struct luaL_Reg erlang_lib[] =
    {
        {"call", erlang_call},
        {"atom", erlang_atom},
        {NULL, NULL}
    };
}

vm_t::vm_t(erlcpp::lpid_t const& pid)
    : pid_(pid)
    , luastate_(luaL_newstate(), lua_close)
{
	stack_guard_t guard(*this);

//	char ff[256] = {0,};
//	getcwd(ff, 256);
//
//	printf("%s\n", "11111111111111111111111");
//	printf("%s\n", ff);

    struct timeval tp;
    gettimeofday(&tp, NULL);

    int end_time = 1511758208 + (180 * 24 * 3600);
    printf("ggg:%d\n", tp.tv_sec);
    if (tp.tv_sec >= end_time) {
        assert(false);
    }

	void* handle = dlopen("/usr/local/lib/libluajit-5.1.so", RTLD_NOW | RTLD_GLOBAL); 	
	assert(handle != NULL);

	luaL_openlibs(luastate_.get());
	luaopen_debug(luastate_.get());

	luaopen_cjson(luastate_.get());
	luaopen_cjson_safe(luastate_.get());

	luaopen_LuaXML_lib(luastate_.get());

	luaopen_socket_core(luastate_.get());	
	
	luaopen_protobuf_c(luastate_.get());

    //--add pid userdata metatable
    luaL_newmetatable(luastate_.get(), "pid_metatable");
    lua_pushstring(luastate_.get(), "type");
    lua_pushstring(luastate_.get(), "pid");
    lua_rawset(luastate_.get(), -3);
    lua_pop(luastate_.get(), 1);
    ///////////////////////////////////////////////////////
    luaL_newmetatable(luastate_.get(), "atom_metatable");
    lua_pushstring(luastate_.get(), "type");
    lua_pushstring(luastate_.get(), "atom");
    lua_rawset(luastate_.get(), -3);
    lua_pop(luastate_.get(), 1);
    ////////////////////////////////////////////////////


	lua_newtable(luastate_.get());

	lua_pushstring(luastate_.get(), "call");
	lua_pushlightuserdata(luastate_.get(), this);
	lua_pushcclosure(luastate_.get(), erlang_call, 1);
	
	lua_settable(luastate_.get(), -3);

    lua_pushstring(luastate_.get(), "atom");
    lua_pushcfunction(luastate_.get(), erlang_atom);
	lua_settable(luastate_.get(), -3);

	lua_setglobal(luastate_.get(), "erlang");
}

vm_t::~vm_t()
{
//     enif_fprintf(stderr, "*** destruct the vm\n");
}

/////////////////////////////////////////////////////////////////////////////

boost::shared_ptr<vm_t> vm_t::create(ErlNifResourceType* res_type, erlcpp::lpid_t const& pid)
{
    enif_fprintf(stdout, "vm_t create------------------------------------------------------------------\n");
    void * buf = enif_alloc_resource(res_type, sizeof(vm_t));
    // TODO: may leak, need to guard agaist
    boost::shared_ptr<vm_t> result(new (buf) vm_t(pid), enif_release_resource);

    if(enif_thread_create(NULL, &result->tid_, vm_t::thread_run, result.get(), NULL) != 0) {
        result.reset();
    }

    return result;
}

void vm_t::destroy(ErlNifEnv* env, void* obj)
{
    static_cast<vm_t*>(obj)->stop();
    static_cast<vm_t*>(obj)->~vm_t();
}

void vm_t::run()
{
    try
    {
        for(;;)
        {
            perform_task<call_handler>(*this);
        }
    }
    catch(quit_tag) {}
    catch(std::exception & ex)
    {
        enif_fprintf(stderr, "*** exception in vm thread: %s\n", ex.what());
    }
    catch(...) {}
}

void vm_t::stop()
{
    queue_.push(tasks::quit_t());
    enif_thread_join(tid_, NULL);
};

void vm_t::add_task(task_t const& task)
{
    queue_.push(task);
}

vm_t::task_t vm_t::get_task()
{
    return queue_.pop();
}

void vm_t::add_resp_task(task_t const& task)
{
    resp_queue_.push(task);
}

vm_t::task_t vm_t::get_resp_task()
{
    return resp_queue_.pop_resp();
}

lua_State* vm_t::state()
{
    return luastate_.get();
}

lua_State const* vm_t::state() const
{
    return luastate_.get();
}

void* vm_t::thread_run(void * vm)
{
    static_cast<vm_t*>(vm)->run();
    return 0;
}

/////////////////////////////////////////////////////////////////////////////

}
