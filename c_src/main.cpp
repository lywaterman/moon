#include "lua.hpp"
#include "types.hpp"
#include "utils.hpp"


using namespace erlcpp;

/////////////////////////////////////////////////////////////////////////////

static struct {
    ERL_NIF_TERM ok;
    ERL_NIF_TERM error;
    ERL_NIF_TERM enomem;
    ERL_NIF_TERM invalid_args;
    ERL_NIF_TERM invalid_type;
    ERL_NIF_TERM not_implemented;
} atoms;

/////////////////////////////////////////////////////////////////////////////

static ErlNifResourceType * res_type = 0;

static int init(ErlNifEnv *env, void **priv, ERL_NIF_TERM load_info)
{
    atoms.ok                = enif_make_atom(env, "ok");
    atoms.error             = enif_make_atom(env, "error");
    atoms.enomem            = enif_make_atom(env, "enomem");
    atoms.invalid_args      = enif_make_atom(env, "invalid_args");
    atoms.invalid_type      = enif_make_atom(env, "invalid_type");
    atoms.not_implemented   = enif_make_atom(env, "not_implemented");

    res_type = enif_open_resource_type(
        env, "lua", "lua_vm", lua::vm_t::destroy,
        static_cast<ErlNifResourceFlags>(ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER), NULL
    );

    return (!!res_type) ? 0 : -1;
}

static int upgrade(ErlNifEnv* env, void** priv_data, void** old_priv_data, ERL_NIF_TERM load_info) {
    return 0;
}

static void unload(ErlNifEnv *env, void *priv_data)
{
}


/////////////////////////////////////////////////////////////////////////////

static ERL_NIF_TERM start(ErlNifEnv * env, int argc, const ERL_NIF_TERM argv[])
{
    try
    {
        if (argc < 1)
        {
            return enif_make_badarg(env);
        }

        lpid_t pid = from_erl<lpid_t>(env, argv[0]);
        boost::shared_ptr<lua::vm_t> vm = lua::vm_t::create(res_type, pid);
        ERL_NIF_TERM result = enif_make_resource(env, vm.get());
        return enif_make_tuple2(env, atoms.ok, result);
    }
    catch( std::exception & ex )
    {
        return enif_make_tuple2(env, atoms.error, enif_make_atom(env, ex.what()));
    }
}

static ERL_NIF_TERM load(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    try
    {
        if (argc < 3)
        {
            return enif_make_badarg(env);
        }

        lua::vm_t * vm = NULL;
        if(!enif_get_resource(env, argv[0], res_type, reinterpret_cast<void**>(&vm)))
        {
            return enif_make_badarg(env);
        }

        binary_t file = from_erl<binary_t>(env, argv[1]);
		lpid_t 	 caller_pid = from_erl<lpid_t>(env, argv[2]);
        lua::vm_t::tasks::load_t load(file, caller_pid);
        vm->add_task(lua::vm_t::task_t(load));

        return atoms.ok;
    }
    catch( std::exception & ex )
    {
        return enif_make_tuple2(env, atoms.error, enif_make_atom(env, ex.what()));
    }
}

static ERL_NIF_TERM eval(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    try
    {
        if (argc < 3)
        {
            return enif_make_badarg(env);
        }

        lua::vm_t * vm = NULL;
        if(!enif_get_resource(env, argv[0], res_type, reinterpret_cast<void**>(&vm)))
        {
            return enif_make_badarg(env);
        }

        binary_t script = from_erl<binary_t>(env, argv[1]);
		lpid_t   caller_pid = from_erl<lpid_t>(env, argv[2]);
        lua::vm_t::tasks::eval_t eval(script, caller_pid);
        vm->add_task(lua::vm_t::task_t(eval));

        return atoms.ok;
    }
    catch( std::exception & ex )
    {
        return enif_make_tuple2(env, atoms.error, enif_make_atom(env, ex.what()));
    }
}

static ERL_NIF_TERM call(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    try
    {
        if (argc < 4)
        {
            return enif_make_badarg(env);
        }

        lua::vm_t * vm = NULL;
        if(!enif_get_resource(env, argv[0], res_type, reinterpret_cast<void**>(&vm)))
        {
            return enif_make_badarg(env);
        }

        atom_t fun = from_erl<atom_t>(env, argv[1]);
        list_t args = from_erl<list_t>(env, argv[2]);
		lpid_t caller_pid = from_erl<lpid_t>(env, argv[3]);
        lua::vm_t::tasks::call_t call(fun, args, caller_pid);
        vm->add_task(lua::vm_t::task_t(call));

        return atoms.ok;
    }
    catch( std::exception & ex )
    {
        return enif_make_tuple2(env, atoms.error, enif_make_atom(env, ex.what()));
    }
}

static ERL_NIF_TERM result(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    try
    {

        if (argc < 3) {
            return enif_make_badarg(env);
        }

        lua::vm_t * vm = NULL;
        if(!enif_get_resource(env, argv[0], res_type, reinterpret_cast<void**>(&vm))) {

            return enif_make_badarg(env);
        }

        term_t term = from_erl<term_t>(env, argv[1]);
		lpid_t caller_pid = from_erl<lpid_t>(env, argv[2]);

        lua::vm_t::tasks::resp_t resp(term, caller_pid);
        vm->add_resp_task(lua::vm_t::task_t(resp));

        return atoms.ok;
    }
    catch( std::exception & ex )
    {
        return enif_make_tuple2(env, atoms.error, enif_make_atom(env, ex.what()));
    }
}



/////////////////////////////////////////////////////////////////////////////

static ErlNifFunc nif_funcs[] = {
    {"start", 1, start},
    {"load", 3, load},
    {"eval", 3, eval},
    {"call", 4, call},
    {"result", 3, result}
};

ERL_NIF_INIT(moon_nif, nif_funcs, &init, NULL, upgrade, unload)

/////////////////////////////////////////////////////////////////////////////

