/*-------------------------------------------------------------------------
 *
 * ilist.c
 *	  support for integrated/inline doubly- and singly- linked lists
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/lib/ilist.c
 *
 * NOTES
 *	  This file only contains functions that are too big to be considered
 *	  for inlining.  See ilist.h for most of the goodies.
 *
 *-------------------------------------------------------------------------
 */
/*-------------------------------------------------------------------------
 *
 * ilist.c
 *	  支持集成/内嵌式的双链表和单链表
 *
 * Portions Copyright (c) 1996-2018, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/lib/ilist.c
 *
 * 注意
 *	  本文件只包含太长而不能内联的函数。大部分好东西都在ilist.h文件中。
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/ilist.h"

/*
 * Delete 'node' from list.
 *
 * It is not allowed to delete a 'node' which is not in the list 'head'
 *
 * Caution: this is O(n); consider using slist_delete_current() instead.
 */
/*
 * 从链表中删除“节点”。
 *
 * 不允许删除不在“head”指向的链表中的“节点”
 *
 * 注意：时间复杂度为O(n)；考虑使用slist_delete_current()。
 */
void
slist_delete(slist_head *head, slist_node *node)
{
	slist_node *last = &head->head;
	slist_node *cur;
	bool		found PG_USED_FOR_ASSERTS_ONLY = false;

	while ((cur = last->next) != NULL)
	{
		if (cur == node)
		{
			last->next = cur->next;
#ifdef USE_ASSERT_CHECKING
			found = true;
#endif
			break;
		}
		last = cur;
	}
	Assert(found);

	slist_check(head);
}

#ifdef ILIST_DEBUG
/*
 * Verify integrity of a doubly linked list
 */
void
dlist_check(dlist_head *head)
{
	dlist_node *cur;

	if (head == NULL)
		elog(ERROR, "doubly linked list head address is NULL");

	if (head->head.next == NULL && head->head.prev == NULL)
		return;					/* OK, initialized as zeroes */

	/* iterate in forward direction */
	for (cur = head->head.next; cur != &head->head; cur = cur->next)
	{
		if (cur == NULL ||
			cur->next == NULL ||
			cur->prev == NULL ||
			cur->prev->next != cur ||
			cur->next->prev != cur)
			elog(ERROR, "doubly linked list is corrupted");
	}

	/* iterate in backward direction */
	for (cur = head->head.prev; cur != &head->head; cur = cur->prev)
	{
		if (cur == NULL ||
			cur->next == NULL ||
			cur->prev == NULL ||
			cur->prev->next != cur ||
			cur->next->prev != cur)
			elog(ERROR, "doubly linked list is corrupted");
	}
}

/*
 * Verify integrity of a singly linked list
 */
void
slist_check(slist_head *head)
{
	slist_node *cur;

	if (head == NULL)
		elog(ERROR, "singly linked list head address is NULL");

	/*
	 * there isn't much we can test in a singly linked list except that it
	 * actually ends sometime, i.e. hasn't introduced a cycle or similar
	 */
	for (cur = head->head.next; cur != NULL; cur = cur->next)
		;
}

#endif							/* ILIST_DEBUG */
