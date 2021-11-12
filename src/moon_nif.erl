-module(moon_nif).

-export([start/1, load/3, eval/3, call/4, result/3]).
-on_load(init/0).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

start(_) ->
    exit(nif_library_not_loaded).

load(_, _, _) ->
    exit(nif_library_not_loaded).

eval(_, _, _) ->
    exit(nif_library_not_loaded).

call(_, _, _, _) ->
    exit(nif_library_not_loaded).

result(_, _, _) ->
    exit(nif_library_not_loaded).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%% local functions:

init() ->
    case erlang:system_info(smp_support) of
        true ->
            SoName = filename:join(priv_dir(), ?MODULE),
            ok = erlang:load_nif(filename:absname(SoName), 0);
        false ->
            error(no_smp_support)
     end.
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

priv_dir() ->
    case code:priv_dir(gl_wrapper_lua) of
        PrivDir when is_list(PrivDir) ->
            PrivDir;
        {error, bad_name} ->
            Ebin = filename:dirname(code:which(?MODULE)),
            filename:join(filename:dirname(Ebin), "priv")
    end.
