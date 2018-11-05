/*-------------------------------------------------------------------------
 *
 * nodeSubplan.h
 *
 *
 *
 * Portions Copyright (c) 1996-2014, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/nodeSubplan.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef NODESUBPLAN_H
#define NODESUBPLAN_H

#include "nodes/execnodes.h"
#include "executor/execdesc.h"

extern SubPlanState *ExecInitSubPlan(SubPlan *subplan, PlanState *parent);

extern AlternativeSubPlanState *ExecInitAlternativeSubPlan(AlternativeSubPlan *asplan, PlanState *parent);

extern void ExecReScanSetParamPlan(SubPlanState *node, PlanState *parent);

/*
 * MPP Change: Added a parameter (ParamListInfo p) to this function.  See comments in nodeSubplan.cpp
 */
extern void ExecSetParamPlan(SubPlanState *node, ExprContext *econtext, QueryDesc *gbl_queryDesc);

<<<<<<< HEAD
/* Gpmon: It seems does not make any sense to gpmon this node. */
=======
extern void ExecSetParamPlanMulti(const Bitmapset *params, ExprContext *econtext);

>>>>>>> 8bc709b37411ba7ad0fd0f1f79c354714424af3d
#endif   /* NODESUBPLAN_H */
