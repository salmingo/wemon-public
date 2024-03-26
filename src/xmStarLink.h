/**
 * @file xmStarLink.h 定义星像特征链表
 * @author 卢晓猛 (lxm@nao.cas.cn)
 * @brief
 * @version 0.1
 * @date 2023-09-30
 *
 * © ARTD Group, NAOC
 *
 */

#ifndef XM_STAR_LINK_H_
#define XM_STAR_LINK_H_

#include "xmStar.h"

struct xmStarLink {
	xmStarPtr star;		///< 星像特征
	xmStarLink* prev;	///< 前导指针
	xmStarLink* next;	///< 后续指针

public:
	xmStarLink() = default;
	xmStarLink(xmStarPtr star) {
		this->star = star;
		prev = next = this;
	}
};

/**
 * @brief 在链表后插入新的链表
 * @param oldLink  已有链表
 * @param newLink  新的链表
 */
void insert_star_link(xmStarLink* oldLink, xmStarLink* newLink);
/**
 * @brief 删除链表
 * @param link  链表
 */
void remove_star_link(xmStarLink** link);
/**
 * @brief 清理链表
 * @param head 链表头指针
 */
void free_star_link(xmStarLink* head);

#endif
