// $Id$
//-------------------------------------------------------------------------------
//	Scheduler.h --
//
//	This file declares 'Scheduler' and its derived classes for hybrid
//	TDM/WDM-PON OLT.
//
//	Copyright (C) 2009 Kyeong Soo (Joseph) Kim
//-------------------------------------------------------------------------------


// For Trace of TX & RX usage
//#define TRACE_TXRX


#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include <assert.h>
#include <omnetpp.h>
#include "HybridPon.h"
#include "HybridPonMsg_m.h"
#include "Monitor.h"

class Scheduler: public cSimpleModule {
	//--------------------------------------------------------------------------
	//	Member variables
	//--------------------------------------------------------------------------

protected:
	// NED parameters (as defined in NED files)
	int cwMax;
	int numReceivers;
	int numTransmitters;
	int numOnus; //	= numChannels (now obsolete)
	int numUsersPerOnu;
	int queueSizePoll; // Size of FIFO queue for polling frames [bit]
	simtime_t maxTxDelay;
	simtime_t onuTimeout;
	string distances;

	// Status variables
	int busyQueuePoll; // Counter to emulate a FIFO for polling frames
	TimeVector RTT;
	TimeVector CH;
	TimeVector TX;
	TimeVector RX;

	// For trace of grant PON frames (interval, grant size and frame length)
	TimeVector vTxTime; // vector of previous grant PON frame TX times

	// For ONU polling
	HybridPonMsgVector pollEvent;
	/* 	TimeVector  pollOnu; */

	// For monitoring
	Monitor *monitor;

	//--------------------------------------------------------------------------
	//	Member functions
	//--------------------------------------------------------------------------

	// Misc.
	void debugSchedulerStatus(void);
	virtual void debugSnapshot(void);
	virtual int scheduleOnuPoll(simtime_t t, HybridPonMessage *msg) // wrapper funciton
	{
		return scheduleAt(t, msg);
	}
	virtual void handleGrant(int lambda, HybridPonFrame *grant) = 0; // pure virtual function
	virtual void initializeSpecific(void) = 0; // "
	virtual void finishSpecific(void) = 0; // "

	/*     // QUICK DEBUG */
	/*     simtime_t debugRX(void); */
	/*     // QUICK DEBUG     */

	// Event handling
	virtual void sendOnuPoll(HybridPonMessage *msg);
	void transmitPollFrame(HybridPonMessage *msg);
	virtual void receiveHybridPonFrame(HybridPonFrame *msg);
	virtual void receiveIpPacket(IpPacket *msg) = 0; // pure virtual function

	// Scheduling
	virtual simtime_t seqSchedule(int onu, HybridPonFrame *ponFrameToOnu);

	// OMNeT++
	void initialize(void);
	virtual void handleMessage(cMessage *msg) = 0; // pure virtual function
	void finish(void);
};

//------------------------------------------------------------------------------
// Classes based on sequential operation mode
//------------------------------------------------------------------------------

class Sequential: public Scheduler {
protected:
	//--------------------------------------------------------------------------
	//	Member variables
	//--------------------------------------------------------------------------

	// NED parameters (as defined in NED files)
	int queueSize; // Size of FIFO queue for data frames [bit]

	// Status variables
	int busyQueue; // Counter to emulate a FIFO for us & ds data frames


	//--------------------------------------------------------------------------
	//	Member functions
	//--------------------------------------------------------------------------

	// Misc.
	virtual void handleGrant(int lambda, HybridPonFrame *grant);
	virtual void initializeSpecific(void);
	virtual void finishSpecific(void);

	// Event handling
	virtual void receiveIpPacket(IpPacket *msg);
	void transmitDataFrame(DummyPacket *msg);

	// OMNeT++
	virtual void handleMessage(cMessage *msg);
};

class SSSF: public Scheduler // SSSF (Sequential Scheduling with	Schedule-time Framing)
{
protected:
	//--------------------------------------------------------------------------
	//	Member variables
	//--------------------------------------------------------------------------

	// NED parameters (as defined in NED files)
	int voqSize; // Size of VOQ [bit]
	//    int             rsDepth;        // Max. # of checking in ONU timeout rescheduling

	// For VOQs: Indexing is done as follows
	// - Downstream data:       [0...numOnus-1]
	// - Upstream grants/polls: [numOnus...2*numOnus-1]
	Voq *voq;
	IntVector voqBitCtr; // vector of VOQ bit counters [bit]

	// TX queues (for Seq. Scheduler Vers. 4 & 5) to store scheduled
	// downstream PON frames (at most one per TX queue).
	Voq *txQueue;

	// For trace of VOQs
	cOutVector *vQueueLength; // array of output vector for VOQ length [frame]
	cOutVector *vQueueOctet; // array of output vector for VOQ size [octet]
#ifdef TRACE_TXRX
	// For trace of TX & RX usages
	cOutVector *vTxUsage; // array of output vector for TX usage (time, chIdx)
	cOutVector *vRxUsage; // array of output vector for RX usage (time, chIdx)
#endif

	// For rescheduling ONU timeout (poll) events
	OnuPollList onuPollList; // sorted list of scheduled ONU timeout events

	// For estimating ONU incoming (upstream) rate [0...numOnus-1]
	IntVector grantCtr; // vector of queued grant counters
	DoubleVector vRate; // vector of estimated ONU incoming rate
	IntVector vGrant; // vector of previous grants
	IntVector vRequest; // vector of previous requests
	TimeVector vRxTime; // vector of previous PON frame RX times

	// For estimating OLT incoming (downstream) rate [0...numOnus-1]
	IntVector dsArrvCtr; // vector of downstream arrival (bit) counters
	IntVector dsTxCtr; // vector of downstream TX (bit) counters


	//--------------------------------------------------------------------------
	//	Member functions
	//--------------------------------------------------------------------------

	// Misc.
	virtual int assignGrants(int ch, int usRequest);
	void debugOnuPollListStatus(void);
	cMessage *cancelOnuPoll(HybridPonMessage *msg); // wrapper function for cancelEvent()
	virtual int scheduleOnuPoll(simtime_t t, HybridPonMessage *msg);
	inline virtual void rescheduleOnuPolls(void) {
	}
	; // to make it optimized away by the compiler
	virtual void handleGrant(int lambda, HybridPonFrame *grant);
	virtual void initializeSpecific(void);
	virtual void finishSpecific(void);

	// Event handling
	virtual void sendOnuPoll(HybridPonMessage *msg);
	virtual void receiveIpPacket(IpPacket *pkt);
	virtual void receiveHybridPonFrame(HybridPonFrame *frame);
	virtual void transmitDataFrame(DummyPacket *msg);

#ifdef TRACE_TXRX
	void releaseTx(DummyPacket *msg);
	void receiveRx(DummyPacket *msg);
	void releaseRx(DummyPacket *msg);
#endif

	// Scheduling:	Note that we split the original scheduling function 'seqSchedule()'
	//				into two parts as follows:
	// - getTxTime(): Find the earliest possible TX time;
	// - scheduleFrame(): Schedule a frame and update global status variables.
	simtime_t getTxTime(const int ch, const bool isGrant, int &txIdx,
			int &rxIdx);
	void scheduleFrame(const simtime_t txTime, const int ch, const int txIdx,
			const int rxIdx, HybridPonFrame *ponFrameToOnu);

	// OMNeT++
	virtual void handleMessage(cMessage *msg);
};

//class Sequential_v3 : public Sequential_v2     // Sequential scheduler Ver. 3
//{
//	//--------------------------------------------------------------------------
//    //	Member variables
//    //--------------------------------------------------------------------------
//
//
//	//--------------------------------------------------------------------------
//	//	Member functions
//	//--------------------------------------------------------------------------
//
//	// Event handling
//    virtual void	receiveIpPacket(IpPacket *pkt);
//    virtual void	transmitDataFrame(DummyPacket *msg);
//};
//
//
//class Sequential_v5 : public Sequential_v4	// Sequential scheduler Ver. 5
//{
//	//--------------------------------------------------------------------------
//	//	Member functions
//	//--------------------------------------------------------------------------
//
//	// Misc.
//	virtual int		assignGrants(int ch, int usRequest);
//};


//------------------------------------------------------------------------------
// Classes based on batch operation mode
//------------------------------------------------------------------------------

class Batch: public Scheduler {
protected:
	//--------------------------------------------------------------------------
	//	Member variables
	//--------------------------------------------------------------------------

	// NED parameters (as defined in NED files)
	simtime_t batchPeriod;
	int voqSize;
	double voqThreshold;

	// For VOQs: Indexing is done as follows
	// - Downstream data:       [0...numOnus-1]
	// - Upstream grants/polls: [numOnus...2*Onus-1]
	Voq *voq;
	IntVector voqBitCtr; // vector of VOQ lengths [bit]
	int voqStartIdx; // VOQ index to start scheduling with at each batch period
	// --> used to provide better fairness among VOQs

	// For trace of VOQs
	cOutVector *vQueueLength; // array of output vector for VOQ length [frame]
	cOutVector *vQueueOctet; // array of output vector for VOQ size in octet


	//--------------------------------------------------------------------------
	//	Member functions
	//--------------------------------------------------------------------------

	// Misc.
	void debugVoqStatus(void);
	virtual void debugSnapshot(void);
	virtual void handleGrant(int lambda, HybridPonFrame *grant);
	virtual void initializeSpecific(void);
	virtual void finishSpecific(void);

	// Event handling
	virtual void sendOnuPoll(HybridPonMessage *msg);
	virtual void receiveIpPacket(IpPacket *msg);
	void batchSchedule(cMessage *batchMsg);
	void transmitVoqDataFrame(HybridPonMessage *msg);

	// Scheduler to be defined in derived classes (pure virtual function)
	virtual int scheduler(const simtime_t TX, const simtime_t RX, simtime_t &t,
			int &voqsToSchedule, BoolVector &schedulableVoq, Voq *voq) = 0;

	// OMNeT++
	virtual void handleMessage(cMessage *msg);
};

class BEDF: public Batch {
	//--------------------------------------------------------------------------
	//	Member functions
	//--------------------------------------------------------------------------

	// Scheduler
	virtual int scheduler(const simtime_t TX, const simtime_t RX, simtime_t &t,
			int &voqsToSchedule, BoolVector &schedulableVoq, Voq *voq);
};

//class LongestQueueFirst : public Batch
//{
//	//--------------------------------------------------------------------------
//	//	Member functions
//	//--------------------------------------------------------------------------
//
//	// Scheduler
//    virtual int scheduler(
//        const simtime_t TX,
//        const simtime_t RX,
//        simtime_t &t,
//        int &voqsToSchedule,
//        BoolVector &schedulableVoq,
//        Voq *voq
//        );
//};
//
//
//class EdfWithLqf : public Batch
//{
//	//--------------------------------------------------------------------------
//	//	Member functions
//	//--------------------------------------------------------------------------
//
//	// Scheduler
//    virtual int scheduler(
//        const simtime_t TX,
//        const simtime_t RX,
//        simtime_t &t,
//        int &voqsToSchedule,
//        BoolVector &schedulableVoq,
//        Voq *voq
//        );
//};


#endif  // __SCHEDULER_H
