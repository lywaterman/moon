# Moon

Library for calling Lua from Erlang, and back.

chanage it to lua5.1 and can call one luavm with multiple process

为了兼容以前的在线运行系统，moon里面的mongodb cxx driver并没有适配最新的legacy1.00+, 而是适配1.00以前的版本。
如果是第一次使用moon，不建议使用嵌入了mongodb driver的moon，建议使用no-mongodb-driver的分支，自己下载luamongo或者luamongo1.00使用，mongodb driver的话，可以使用luamongo(before legacy-1.00), luamongo1.00(legacy 1.00 and after)

moon已经经过了5个产品的验证

************************************************************************************************
************************************************************************************************
************************************************************************************************
注意： moon默认使用luajit作为lua执行环境，为保障运行中的符号link，moon会在启动vm时dlopen libluajit-5.1.so 请确保/usr/local/lib下包含libluajit-5.1.so
如果你需要使用liblua5.1，请在c_src/lua.cpp中修改

编译成功后请执行make test 如果测试通过，moon就可以使用了
************************************************************************************************
************************************************************************************************
************************************************************************************************
************************************************************************************************



你可以start多个luavm，而每个luavm是一个单独的系统线程

注意：
    lua到erlang的转换，对于空表，默认返回的是erlang中的数组[], 如果希望返回table, 请使用
    a={}; b={is_hash=true}; setmetatable(a,b); return a; 这样返回的a就会是一个空hash表
    
其他具体的例子请看test/moon_test.erl


## Dependencies:

libboost1.48(1.46应该也行), libluajit5.1`

详细使用方法和转换，请看test文件夹中的 moon_test.erl
***************************************************************************************************
lua通过中的erlang.call方法来调用erlang函数，通过erlang.atom来生成erlang atom
就像这样
***************************************************************************************************
moon:eval(luavm, "return erlang.call('erlang', 'process_info', {erlang.call('erlang', 'self', {}).result}).result").

## Type mapping:

<table>
  <tr>
    <th>Erlang</th>
    <th>Lua</th>
    <th>Erlang</th>
    <th>Remarks</th>
  </tr>
  <tr>
    <td>local pid</td>
    <td>userdata</td>
    <td>local pid</td>
    <td>userdata in lua</td>
  </tr>
  <tr>
    <td>nil / undefined</td>
    <td>nil</td>
    <td>nil</td>
    <td>nil in lua</td>
  </tr>
  <tr>
    <td>true</td>
    <td>true</td>
    <td>true</td>
    <td>boolean in lua</td>
  </tr>
  <tr>
    <td>false</td>
    <td>false</td>
    <td>false</td>
    <td>boolean in lua</td>
  </tr>
  <tr>
    <td>42</td>
    <td>42</td>
    <td>42</td>
    <td>number in lua</td>
  </tr>
  <tr>
    <td>42.123</td>
    <td>42.123</td>
    <td>42.123</td>
    <td>number in lua</td>
  </tr>
  <tr>
    <td>atom</td>
    <td>"atom"</td>
    <td><<"atom">></td>
    <td>string in lua, binary, when comes back to erlang, erlang.atom() create erlang atom, is a userdata</td>
  </tr>
  <tr>
    <td>"string"</td>
    <td>{115,116,114,105,110,103}</td>
    <td>"string"</td>
    <td>table with integers in lua, dont use it!</td>
  </tr>
  <tr>
    <td><<"binary">></td>
    <td>"binary"</td>
    <td><<"binary">></td>
    <td>string in lua</td>
  </tr>
  <tr>
    <td>[]</td>
    <td>{}</td>
    <td>[]</td>
    <td></td>
  </tr>
  <tr>
    <td>[10, 100, <<"abc">>]</td>
    <td>{10, 100, "abc"}</td>
    <td>[10, 100, "abc"]</td>
    <td></td>
  </tr>
  <tr>
    <td>[{yet, value}, {another, value}]</td>
    <td>{yet="value", another="value"}</td>
    <td>[{<<"another">>, <<"value">>}, {<<"yet">>, <<"value">>}]</td>
  </tr>
  <tr>
    <td>[{ugly, "mixed"}, list]</td>
    <td>{ugly="mixed", "list"}</td>
    <td>[<<"list">>, {<<"ugly">>, <<"mixed">>}]</td>
    <td>"list" will be accessable at index [1], and "mixed" - under the "ugly" key</td>
  </tr>
</table>
