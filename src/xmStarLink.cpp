
#include "xmStarLink.h"

void insert_star_link(xmStarLink* oldLink, xmStarLink* newLink) {
	newLink->prev = oldLink;
	newLink->next = oldLink->next;
	oldLink->next = newLink;
}

void remove_star_link(xmStarLink** link) {
	(*link)->prev->next = (*link)->next;
	(*link)->next->prev = (*link)->prev;
	delete *link;
	*link = NULL;
}

void free_star_link(xmStarLink* head) {
	if (head) {
		xmStarLink *now = head->next;
		xmStarLink *next;

		while (now != head) {
			next = now->next;
			delete now;
			now = next;
		}
		head->prev = head;
		head->next = head;
	}
}
