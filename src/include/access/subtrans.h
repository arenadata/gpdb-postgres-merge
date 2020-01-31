/*
 * subtrans.h
 *
 * PostgreSQL subtransaction-log manager
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/subtrans.h
 */
#ifndef SUBTRANS_H
#define SUBTRANS_H

/* Number of SLRU buffers to use for subtrans */
#define NUM_SUBTRANS_BUFFERS	32

<<<<<<< HEAD
typedef struct SubTransData
{
	TransactionId parent;
	TransactionId topMostParent;
} SubTransData;

extern void SubTransSetParent(TransactionId xid, TransactionId parent, bool overwriteOK);
=======
extern void SubTransSetParent(TransactionId xid, TransactionId parent);
>>>>>>> 9e1c9f959422192bbe1b842a2a1ffaf76b080196
extern TransactionId SubTransGetParent(TransactionId xid);
extern TransactionId SubTransGetTopmostTransaction(TransactionId xid);

extern Size SUBTRANSShmemSize(void);
extern void SUBTRANSShmemInit(void);
extern void BootStrapSUBTRANS(void);
extern void StartupSUBTRANS(TransactionId oldestActiveXID);
extern void ShutdownSUBTRANS(void);
extern void CheckPointSUBTRANS(void);
extern void ExtendSUBTRANS(TransactionId newestXact);
extern void TruncateSUBTRANS(TransactionId oldestXact);

#endif							/* SUBTRANS_H */
