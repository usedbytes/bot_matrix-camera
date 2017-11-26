#ifndef __LIST_H__
#define __LIST_H__

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

void list_init(struct list_head *list);
void list_add_before(struct list_head *list, struct list_head *entry);
void list_add_after(struct list_head *list, struct list_head *entry);
void list_add_end(struct list_head *list, struct list_head *entry);
void list_add_start(struct list_head *list, struct list_head *entry);
void list_del(struct list_head *entry);

#endif
