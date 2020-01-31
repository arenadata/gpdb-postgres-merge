/*-------------------------------------------------------------------------
 *
 * sysattr.h
 *	  POSTGRES system attribute definitions.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/sysattr.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SYSATTR_H
#define SYSATTR_H


/*
 * Attribute numbers for the system-defined attributes
 */
#define SelfItemPointerAttributeNumber			(-1)
<<<<<<< HEAD
#define ObjectIdAttributeNumber					(-2)
#define MinTransactionIdAttributeNumber			(-3)
#define MinCommandIdAttributeNumber				(-4)
#define MaxTransactionIdAttributeNumber			(-5)
#define MaxCommandIdAttributeNumber				(-6)
#define TableOidAttributeNumber					(-7)
#define GpSegmentIdAttributeNumber			    (-8)    /*CDB*/
#define FirstLowInvalidHeapAttributeNumber		(-9)
=======
#define MinTransactionIdAttributeNumber			(-2)
#define MinCommandIdAttributeNumber				(-3)
#define MaxTransactionIdAttributeNumber			(-4)
#define MaxCommandIdAttributeNumber				(-5)
#define TableOidAttributeNumber					(-6)
#define FirstLowInvalidHeapAttributeNumber		(-7)
>>>>>>> 9e1c9f959422192bbe1b842a2a1ffaf76b080196

#endif							/* SYSATTR_H */
