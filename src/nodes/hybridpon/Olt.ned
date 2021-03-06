// $Id$
//------------------------------------------------------------------------------
//	Olt.ned --
//
//	This file defines a PON OLT module.
//
//  Copyright (C) 2009 Kyeong Soo (Joseph) Kim
//------------------------------------------------------------------------------


package inet.nodes.pon;


import inet.linklayer.ethernet.EthernetInterface;


module Olt
{
    parameters:
        @display("bgb=188,169;bgp=2,2");
        int numOnus;	// number of ONUs connected to OLT
        string schType;	// type of 'Scheduler' (algorithm and operation mode)
    gates:
		inout pong;		// to and from PON (i.e., splitter)
		inout ethg;		// to and from network (i.e., metro/backbone)
        output fromSchedulerToMonitor;
        output fromIpPktGeneratorToMonitor;
        output fromIpPktSinkToMonitor;
    submodules:
        scheduler: <schType> like IScheduler {
            parameters:
                @display("i=block/classifier;p=94,116");
                numOnus = numOnus;
        }
        eth: EthernetInterface {
            parameters:
                @display("p=145,257,row,110;q=l2queue");
        }
    connections:
        scheduler.toMonitor --> fromSchedulerToMonitor;
		pong <--> scheduler.phy
		ethg <--> eth.phys
		eth.netwOut --> scheduler.ifIn
		eth.netwIn <-- scheduler.ifOut
}
