/*
 * Vaguely based on the linux list implementation
 *   include/linux/list.h
 *
 * (which carries no Copyright information, but which is covered presumably by
 * GPL v2)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */
#include <stdlib.h>

#include "list.h"

void list_init(struct list_head *list) {
	list->next = list;
	list->prev = list;
}

static void list_add(struct list_head *prev, struct list_head *next,
		     struct list_head *entry)
{
	entry->next = next;
	next->prev = entry;

	prev->next = entry;
	entry->prev = prev;
}

void list_add_before(struct list_head *list, struct list_head *entry)
{
	list_add(list->prev, list, entry);
}

void list_add_after(struct list_head *list, struct list_head *entry)
{
	list_add(list, list->next, entry);
}

void list_add_end(struct list_head *list, struct list_head *entry)
{
	list_add_before(list, entry);
}

void list_add_start(struct list_head *list, struct list_head *entry)
{
	list_add_after(list, entry);
}

void list_del(struct list_head *entry) {
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;

	entry->prev = entry->next = NULL;
}
