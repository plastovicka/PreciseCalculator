/*
	(C) 2005-2007  Petr Lastovicka

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License.
	*/
#include "hdr.h"
#include "list2.h"

NxtPrv::~NxtPrv()
{
	prv->nxt= nxt;
	nxt->prv= prv;
}


void List2::deleteAll()
{
	while(nxt!=this) delete nxt;
}

NxtPrv *List2::remove()
{
	NxtPrv *item=nxt;
	if(item==this) return 0;
	nxt=item->nxt;
	nxt->prv=this;
	item->nxt= item->prv= item;
	return item;
}

void NxtPrv::append(NxtPrv *item)
{
	assert(item->isOut());
	prv->nxt= item;
	item->prv= prv;
	prv= item;
	item->nxt= this;
}

void NxtPrv::prepend(NxtPrv *item)
{
	assert(item->isOut());
	nxt->prv= item;
	item->nxt= nxt;
	nxt= item;
	item->prv= this;
}

int List2::count()
{
	int n=0;
	forl2(NxtPrv, *this) n++;
	return n;
}
