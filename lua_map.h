/*
    This file is part of Iceball.

    Iceball is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Iceball is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Iceball.  If not, see <http://www.gnu.org/licenses/>.
*/

// common functions
int icelua_fn_common_map_get_dims(lua_State *L)
{
	int top = icelua_assert_stack(L, 0, 0);
	
	map_t *map = (L == lstate_server ? svmap : clmap);
	
	// if no map, just give off nils
	if(map == NULL)
	{
		return 0;
	} else {
		lua_pushinteger(L, map->xlen);
		lua_pushinteger(L, map->ylen);
		lua_pushinteger(L, map->zlen);
		return 3;
	}
}

int icelua_fn_common_map_pillar_get(lua_State *L)
{
	int top = icelua_assert_stack(L, 2, 2);
	int px, pz;
	int i;
	
	px = lua_tointeger(L, 1);
	pz = lua_tointeger(L, 2);
	
	map_t *map = (L == lstate_server ? svmap : clmap);
	
	// if no map, return nil
	if(map == NULL)
		return 0;
	
	// get a pillar
	uint8_t *p = map->pillars[(pz&(map->zlen-1))*map->xlen+(px&(map->xlen-1))];
	
	// build the list
	int llen = 4*((255&(int)*p)+1);
	lua_createtable(L, llen, 0);
	p += 4;
	
	for(i = 1; i <= llen; i++)
	{
		lua_pushinteger(L, i);
		lua_pushinteger(L, *(p++));
		lua_settable(L, -3);
	}
	
	return 1;
}

int icelua_fn_common_map_pillar_set(lua_State *L)
{
	int top = icelua_assert_stack(L, 3, 3);
	int px, pz, tlen;
	int i;
	
	px = lua_tointeger(L, 1);
	pz = lua_tointeger(L, 2);
	if(!lua_istable(L, 3))
		return luaL_error(L, "expected a table, got something else");
	tlen = lua_objlen(L, 3);
	
	map_t *map = (L == lstate_server ? svmap : clmap);
	
	// if no map, ignore (for now)
	if(map == NULL)
		return 0;
	
	// validate that the table is not TOO large and is 4-byte aligned wrt size
	if((tlen&3) || tlen > 1024)
		return luaL_error(L, "table length %d invalid", tlen);
	
	// validate the table's input range
	for(i = 0; i < tlen; i++)
	{
		lua_pushinteger(L, 1+i);
		lua_gettable(L, 3);
		int v = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if(v < 0 || v > 255)
			return luaL_error(L, "value at index %d out of unsigned byte range (%d)"
				, 1+i, v);
	}
	
	// validate the table data
	i = 0;
	for(;;)
	{
		lua_pushinteger(L, 1+i+0);
		lua_gettable(L, 3);
		lua_pushinteger(L, 1+i+1);
		lua_gettable(L, 3);
		lua_pushinteger(L, 1+i+2);
		lua_gettable(L, 3);
		lua_pushinteger(L, 1+i+3);
		lua_gettable(L, 3);
		int n = lua_tointeger(L, -4);
		int s = lua_tointeger(L, -3);
		int e = lua_tointeger(L, -2);
		int a = lua_tointeger(L, -1);
		lua_pop(L, 4);
		
		//printf("%i %i | %i %i | %i %i %i %i\n",px,pz,i,tlen,n,s,e,a);
		
		// Note, we are not supporting the shenanigans you can do in VOXLAP.
		// Especially considering that editing said shenanigans causes issues.
		// Also noting that said shenanigans weren't all that exploited,
		// VOXLAP automatically corrects shenanigans when you edit stuff,
		// and pyspades has no support for such shenanigans.
		if(e+1 < s)
			return luaL_error(L, "pillar has end+1 < start (%d < %d)"
					, e+1, s);
		if(i != 0 && s < a)
			return luaL_error(L, "pillar has start < air (%d < %d)"
					, s, a);
		if(n != 0 && n-1 < e-s+1)
			return luaL_error(L, "pillar has length < top length (%d < %d)"
					, n-1, e-s+1);
		
		
		// NOTE: this doesn't validate the BGRT (colour/type) entries.
		int la = 0;
		if(n == 0)
		{
			int exlen = (e-s+1)*4+i+4;
			if(exlen != tlen)
				return luaL_error(L, "pillar table len should be %d, got %d instead"
					, exlen, tlen);
			break;
		} else {
			i += 4*n;
			// should always be colour on the bottom!
			if(i > tlen-4)
				return luaL_error(L, "pillar table overflow when validating");
		
		}
	}
	
	// expand the pillar data if necessary
	int idx = (pz&(map->zlen-1))*map->xlen+(px&(map->xlen-1));
	uint8_t *p = map->pillars[idx];
	if((p[0]+1)*4 < tlen)
	{
		p = map->pillars[idx] = realloc(p, tlen+4);
		p[0] = (tlen>>2)-1;
	}
	
	// transfer the table data
	p += 4;
	for(i = 1; i <= tlen; i++)
	{
		lua_pushinteger(L, i);
		lua_gettable(L, 3);
		*(p++) = (uint8_t)lua_tointeger(L, -1);
		lua_pop(L, 1);
	}
	
	force_redraw = 1;
	
	return 0;
}

// client functions
int icelua_fn_client_map_fog_get(lua_State *L)
{
	int top = icelua_assert_stack(L, 0, 0);
	
	lua_pushinteger(L, (fog_color>>16)&255);
	lua_pushinteger(L, (fog_color>>8)&255);
	lua_pushinteger(L, (fog_color)&255);
	lua_pushnumber(L, fog_distance);
	
	return 4;
}

int icelua_fn_client_map_fog_set(lua_State *L)
{
	int top = icelua_assert_stack(L, 4, 4);
	
	int r = lua_tointeger(L, 1)&255;
	int g = lua_tointeger(L, 2)&255;
	int b = lua_tointeger(L, 3)&255;
	fog_distance = lua_tonumber(L, 4);
	if(fog_distance < 5.0f)
		fog_distance = 5.0f;
	if(fog_distance > FOG_MAX_DISTANCE)
		fog_distance = FOG_MAX_DISTANCE;
	
	fog_color = (r<<16)|(g<<8)|b;
	force_redraw = 1;
	
	return 4;
}
