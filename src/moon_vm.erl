-module(moon_vm).
-behaviour(gen_server).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%% gen_server callbacks:
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, code_change/3]).

%% api:
-export([start_link/1]).
-export([load/3, eval/3, call/4]).

-record(state, {vm, callback}).
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%% Public api:

start_link(Options) ->
    gen_server:start_link(?MODULE, Options, []).

load(Pid, File, Timeout) ->
	Result = gen_server:call(Pid, {load, File, self()}, Timeout),
	case Result of
		{ok, VM} ->
			receive_response_call(Pid, #state{vm=VM, callback=undefined});
		_ ->
			Result
	end.

eval(Pid, Code, Timeout) ->
	Result = gen_server:call(Pid, {eval, Code, self()}, Timeout),

	case Result of
		{ok, VM} ->
			receive_response_call(Pid, #state{vm=VM, callback=undefined});
		_ ->
			Result
	end.

call(Pid, Fun, Args, Timeout) ->
	Result = gen_server:call(Pid, {call, Fun, Args, self()}, Timeout),

	case Result of 
		{ok, VM} ->
			receive_response_call(Pid, #state{vm=VM, callback=undefined});
		_ -> Result
	end.


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%% Private api:


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

init(Options) ->
    Callback = proplists:get_value(callback, Options),
    {ok, VM} = moon_nif:start(self()),
    {ok, #state{vm=VM, callback=Callback}}.

handle_call({load, File, Caller}, _, State=#state{vm=VM}) ->
	try
    	ok = moon_nif:load(VM, to_binary(File), Caller),
    	{reply, {ok, VM}, State}
	catch
		_:Error ->
			{reply, {load_error, Error}, State}
	end;

handle_call({eval, Code, Caller}, _, State=#state{vm=VM}) ->
	try 
    	ok = moon_nif:eval(VM, to_binary(Code), Caller),
    	{reply, {ok, VM}, State}
	catch
		_:Error ->
			{reply, {eval_error, Error}, State}
	end;

handle_call({call, Fun, Args, Caller}, _, State=#state{vm=VM}) when is_list(Args) ->
	try
    	ok = moon_nif:call(VM, to_atom(Fun), Args, Caller),
    	{reply, {ok, VM}, State}
	catch
		_:Error ->
			{reply, {call_error, Error}, State}
	end;	

handle_call({callback, Callback, Args, VM, Caller}, _, State) ->
    try
        case handle_callback(Callback, Args) of
            {error, Result} -> moon_nif:result(VM, [{error, true}, {result, Result}], Caller);
            {ok, Result}    -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller);
            Result          -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller)
        end
    catch 
        _:Error ->
            moon_nif:result(VM, [{error, true}, {result, Error}], Caller)
    end,
    {reply, ok, State};

handle_call(_, _, State) ->
  {reply, {call_error, no_right_param}, State}.

handle_cast(_, State) ->
    {noreply, State}.

handle_info({moon_callback, Args, Caller}, State=#state{vm=VM}) ->
    try
        true = erlang:is_process_alive(Caller),
        case handle_callback(undefined, Args) of
            {error, Result} -> moon_nif:result(VM, [{error, true}, {result, Result}], Caller);
            {ok, Result}    -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller);
            Result          -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller)
        end
    catch 
        _:Error ->
            moon_nif:result(VM, [{error, true}, {result, Error}], Caller)
    end,

    {noreply, State};

handle_info(_, State) ->
    {noreply, State}.

terminate(_, _) ->
    ok.

code_change(_, State, _) ->
    {ok, State}.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

receive_response_self() ->
    receive
        {moon_response, Response, Caller} ->
            Response
	end.

receive_response_call(Pid, State=#state{vm=VM, callback=Callback}) ->
    receive
        {moon_response, Response, Caller} ->
            Response;
        {moon_callback, Args, Caller} ->
            gen_server:call(Pid, {callback, Callback, Args, VM, Caller}, infinity),
            %%try
            %%    case handle_callback(Callback, Args) of
            %%        {error, Result} -> moon_nif:result(VM, [{error, true}, {result, Result}], Caller);
            %%        {ok, Result}    -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller);
            %%        Result          -> moon_nif:result(VM, [{error, false}, {result, Result}], Caller)
            %%    end
            %%catch _:Error ->
            %%    moon_nif:result(VM, [{error, true}, {result, Error}], Caller)
            %%end,
            receive_response_call(Pid, State)
    end.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

handle_callback(undefined, {Mod, Fun, Args}) ->
    erlang:apply(to_atom(Mod),to_atom(Fun),Args);

handle_callback(Callback, Args) when is_function(Callback) ->
    Callback(Args);

handle_callback({Mod, Fun}, Args) when is_atom(Mod), is_atom(Fun) ->
    erlang:apply(Mod, Fun, Args);

handle_callback({Mod, Fun, Args0}, Args1)
        when is_atom(Mod), is_atom(Fun)
           , is_list(Args0), is_list(Args1) ->
    erlang:apply(Mod, Fun, Args0 ++ Args1);

handle_callback(_, _) ->
    error(invalid_call).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

to_atom(Val) when is_atom(Val) -> Val;
to_atom(Val) when is_list(Val) -> list_to_atom(Val);
to_atom(Val) when is_binary(Val) -> list_to_atom(binary_to_list(Val)).

to_binary(Val) when is_binary(Val) -> Val;
to_binary(Val) when is_atom(Val) -> list_to_binary(atom_to_list(Val));
to_binary(Val) when is_list(Val) -> list_to_binary(Val).
