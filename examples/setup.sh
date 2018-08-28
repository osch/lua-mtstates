# Setup script for invoking examples from development sandbox when
# building mtstates using the Makefile.
# Source this into interactive shell by invoking ". setup.sh" from this directory
# This is not necessary if mtstates is installed, e.g. via luarocks.

this_dir=$(pwd)

luamtstates_dir=$(cd "$this_dir"/..; pwd)

if [ ! -e "$luamtstates_dir/examples/setup.sh" -o ! -e "$luamtstates_dir/src/main.c" ]; then

    echo '**** ERROR: ". setup.sh" must be invoked from "examples" directory ***'

else

    echo "Setting lua paths for: $luamtstates_dir"

    add_lua_path="$luamtstates_dir/src/?.lua;$luamtstates_dir/src/?/init.lua"
    add_lua_cpath="$luamtstates_dir/build/?.so"

    # unset LUA_PATH_5_3 LUA_CPATH_5_3 LUA_PATH_5_2 LUA_CPATH_5_2 LUA_PATH LUA_CPATH

    default_version=""
    if which lua > /dev/null 2>&1; then
        default_version=$(lua -e 'v=_VERSION:gsub("^Lua ","");print(v)')
        echo "Setting path for lua (version=$default_version)"
        if [ "$default_version" = "5.1" ]; then
            export LUA_PATH="$add_lua_path;$(lua -e 'print(package.path)')"
            export LUA_CPATH="$add_lua_cpath;$(lua -e 'print(package.cpath)')"
        else
            lua_path_vers=$(echo $default_version|sed 's/\./_/')
            eval "export LUA_PATH_$lua_path_vers=\"$add_lua_path;$(lua -e 'print(package.path)')\""
            eval "export LUA_CPATH_$lua_path_vers=\"$add_lua_cpath;$(lua -e 'print(package.cpath)')\""
        fi
    fi
    
    for vers in 5.1 5.2 5.3; do
        lua_cmd=""
        if which lua$vers > /dev/null 2>&1; then
            lua_cmd="lua$vers"
        elif which lua-$vers > /dev/null 2>&1; then
            lua_cmd="lua-$vers"
        fi
        if [ -n "$lua_cmd" ]; then
            lua_version=$($lua_cmd -e 'v=_VERSION:gsub("^Lua ","");print(v)')
            if [ "$lua_version" != "$default_version" ]; then
                echo "Setting path for $lua_cmd (version=$lua_version)"
                if [ "$lua_version" = "5.1" ]; then
                    export LUA_PATH="$add_lua_path;$($lua_cmd -e 'print(package.path)')"
                    export LUA_CPATH="$add_lua_cpath;$($lua_cmd -e 'print(package.cpath)')"
                else
                    lua_path_vers=$(echo $lua_version|sed 's/\./_/')
                    eval "export LUA_PATH_$lua_path_vers=\"$add_lua_path;$($lua_cmd -e 'print(package.path)')\""
                    eval "export LUA_CPATH_$lua_path_vers=\"$add_lua_cpath;$($lua_cmd -e 'print(package.cpath)')\""
                fi
            fi
        fi
    done
fi

unset lua_cmd this_dir luamtstates_dir add_lua_path add_lua_cpath lua_version lua_path_vers vers default_version

