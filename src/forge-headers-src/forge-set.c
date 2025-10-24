/*
 * forge: Forge your system
 * Copyright (C) 2025  malloc-nbytes
 * Contact: zdhdev@yahoo.com

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "forge/set.h"

forge_set
forge_set_create(forge_set_hash_sig hash,
           forge_set_cmp_sig  cmp)
{
        return (forge_set) {
                
        };
}

int forge_set_contains(set *set, void *v);

void forge_set_remove(set *set, void *v);
