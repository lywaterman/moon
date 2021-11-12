-module(moon_test).
-include_lib("eunit/include/eunit.hrl").

the_test_() ->
    {foreach,
        fun setup/0,
        fun teardown/1,
        [
            {"Starting/Stopping the VM",
                fun() -> ok end
            },
            {"Erlang -> Lua type mapping",
                fun() ->
                    Script = <<"function test(Arg, Type) return type(Arg) == Type end">>,
                    ?assertMatch({ok, undefined}, moon:eval(vm, Script)),
                    ?assertMatch({ok, true}, moon:call(vm, test, [nil, <<"nil">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [true, <<"boolean">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [false, <<"boolean">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [42, <<"number">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [42.5, <<"number">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [hello, <<"string">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [<<"hello">>, <<"string">>])),
                    ?assertMatch({ok, true}, moon:call(vm, test, [[], <<"table">>]))
                end
            },
            {"Lua -> Erlang type mapping",
                fun() ->
                    ?assertMatch({ok, <<"ok">>}, moon:eval(vm, <<"local x = erlang.call(\"io\", \"format\", {\"Look ma, im calling!\"}); return x.result">>)),
                    ?assertMatch({ok, nil}, moon:eval(vm, <<"return nil">>)),
                    ?assertMatch({ok, true}, moon:eval(vm, <<"return true">>)),
                    ?assertMatch({ok, false}, moon:eval(vm, <<"return false">>)),
                    ?assertMatch({ok, 42}, moon:eval(vm, <<"return 42">>)),
                    ?assertMatch({ok, 42}, moon:eval(vm, <<"return 42.0">>)),
                    ?assertMatch({ok, 42.005}, moon:eval(vm, <<"return 42.005">>)),
                    ?assertMatch({ok, <<"hello">>}, moon:eval(vm, <<"return \"hello\"">>)),
                    ?assertMatch({ok, <<"goodbye">>}, moon:eval(vm, <<"return \"goodbye\"">>)),
                    ?assertMatch({ok, []}, moon:eval(vm, <<"return {}">>)),

                    ?assertMatch({ok, [10, 100, <<"abc">>]},
                        moon:eval(vm, <<"return {10, 100, \"abc\"}">>)),

                    ?assertMatch({ok, [{<<"another">>, <<"value">>}, {<<"yet">>, <<"value">>}]},
                        moon:eval(vm, <<"return {yet=\"value\", another=\"value\"}">>)),

					?assertMatch({ok, [{1, <<"list">>}, {2, [{<<"x">>, 1}]}, {<<"ugly">>, <<"mixed">>}]},
						moon:eval(vm, <<"return {ugly=\"mixed\", \"list\", {x=1}}">>)),
                    ?assertMatch({ok, [{}]},
                                 moon:eval(vm, <<"do a={}; b={is_hash=true}; setmetatable(a,b); return a;  end">>)),

                    Script = <<"function pid_test(pid) return pid end">>,
                    ?assertMatch({ok, undefined}, moon:eval(vm, Script)),
                    ?assertMatch({ok, <<"luatype_function">>},
                                 moon:eval(vm, <<"do return pid_test end">>)),
                    SelfPid = self(),
                    ?assertMatch({ok, SelfPid},
                                 moon:call(vm, pid_test, [self()])),
                    ?assertMatch({ok, ok}, moon:eval(vm, "return erlang.atom('ok', 'false')"))

                end
            }
        ]
    }.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

setup() ->
    error_logger:tty(false),
    application:start(moon),
    {ok, Res} = moon:start_vm(),
    register(vm, Res).

teardown(_) ->
    ok = moon:stop_vm(whereis(vm)),
    application:stop(moon).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
