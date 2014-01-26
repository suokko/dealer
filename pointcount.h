#ifndef POINTCOUNT_H
#define POINTCOUNT_H
/* Indexes into the pointcount array */

enum idxPointcount
{
	idxHcp = 0 ,
	idxControls ,
	idxTens ,
	idxJacks ,
	idxQueens ,
	idxKings ,
	idxAces ,
	idxTop2 ,
	idxTop3 ,
	idxWinners = idxTop3 ,
	idxTop4 ,
	idxTop5 ,
	idxC13  ,
	
	idxEnd 
} ;

/* the pointcount array itself */
extern int tblPointcount [idxEnd][13] ;

#endif /* POINTCOUNT_H */
