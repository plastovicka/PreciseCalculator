/*
 (C) Petr Lastovicka

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License.
 */
#ifndef list2H
#define list2H
//---------------------------------------------------------------------------
#define forl2(T,l) for(T *item=(T*)(l).nxt;\
	(NxtPrv*)item!=&(l); item=(T*)item->nxt)

//---------------------------------------------------------------------------
struct NxtPrv
{
	NxtPrv *nxt, *prv;

	NxtPrv(){ nxt=prv=this; }
	virtual ~NxtPrv();
	bool isOut(){ return nxt==this; }
	void append(NxtPrv *item); 	// Put item at the end of the list
	void prepend(NxtPrv *item);	// Put item at the beginning of the list
};

class List2 : public NxtPrv
{
public:
	bool isEmpty(){ return nxt==this; }
	void deleteAll();
	~List2(){ deleteAll(); }

	NxtPrv *first(){ return nxt!=this ? nxt : 0; }
	NxtPrv *last(){ return prv!=this ? prv : 0; }
	NxtPrv *remove(); 	 	// Take item off the front of the list
	int count();
};
//---------------------------------------------------------------------------
#endif
