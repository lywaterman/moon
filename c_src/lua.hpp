#pragma once

#include "types.hpp"
#include "queue.hpp"

#include <lua.hpp>
#include <boost/shared_ptr.hpp>

namespace lua {

class vm_t
{
private:
    vm_t(erlcpp::lpid_t const& pid);
    ~vm_t();

    void run();
    void stop();

    static void* thread_run(void * vm);

public :
    struct tasks
    {
        struct load_t
        {
            load_t(erlcpp::binary_t const& file, erlcpp::lpid_t const& caller)
                : file(file), caller(caller)
            {}
            erlcpp::binary_t file;
			erlcpp::lpid_t	 caller;
        };
        struct eval_t
        {
            eval_t(erlcpp::binary_t const& code, erlcpp::lpid_t const& caller)
                : code(code), caller(caller)
            {}
            erlcpp::binary_t code;
			erlcpp::lpid_t	 caller;
        };
        struct call_t
        {
            call_t(erlcpp::atom_t const& fun, erlcpp::list_t const& args, erlcpp::lpid_t const& caller)
                : fun(fun), args(args), caller(caller)
            {};
            erlcpp::atom_t fun;
            erlcpp::list_t args;
			erlcpp::lpid_t caller;
        };
        struct resp_t
        {
            resp_t(erlcpp::term_t const& term, erlcpp::lpid_t const& caller) : term(term), caller(caller) {}
            erlcpp::term_t term;
			erlcpp::lpid_t caller;
        };
        struct quit_t {};
    };
    typedef boost::variant
    <
        tasks::load_t,
        tasks::eval_t,
        tasks::call_t,
        tasks::resp_t,
        tasks::quit_t
    > task_t;

public :

    erlcpp::lpid_t erl_pid() const { return pid_; }

    void add_task(task_t const& task);
    task_t get_task();

    void add_resp_task(task_t const& task);
    task_t get_resp_task();


    lua_State* state();
    lua_State const * state() const;

    static void destroy(ErlNifEnv* env, void* obj);
    static boost::shared_ptr<vm_t> create(ErlNifResourceType* res_type, erlcpp::lpid_t const& pid);
    
    erlcpp::lpid_t               cur_caller;
private :
    erlcpp::lpid_t               pid_;
    ErlNifTid                    tid_;
    boost::shared_ptr<lua_State> luastate_;
    queue<task_t>                queue_;
    queue<task_t>                resp_queue_;
};

}
