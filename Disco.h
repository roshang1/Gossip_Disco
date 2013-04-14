#ifndef _DISCO_H_
#define _DISCO_H_

#include "VirtualApplication.h"
#include "DataPacket_m.h"
#include "NeighborDiscPacket_m.h"

#include <map>
#include <queue>
#include <cmath>

using namespace std;

int primePairs[10][2] = {
		//5
		/*{23, 157},
		{29, 67},
		{31, 53},
		{37, 43}
		*/
		//10
/*		{11,109},
		{13,43},
		{17,23},
		{19,23}*/

		//2

		 {53,449},
{59,331},
{61,277},
{67,197},
{71,167},
{73,157},
{79,137},
{83,127},
{89,113},
{97,103}

};
int totalPrimes = 10;

enum TIMERS {
	START_OF_SLOT = 1, GO_TO_SLEEP = 2, NO_OPERATION= 3, GENERATE_SAMPLE = 4, TRANSMIT_BEACON = 5, TEST = 6, SAMPLE_AVG = 7
};

enum PACKET_TYPE {
	NEIGHBOR_DISC_PACKET = 1, DATA_PACKET = 2
};

struct RendezvousSchedule {
	// Map of rendezvous slot no to
	// gap between two rendezvous.
	map<long, long> slotNos;
	bool adjustSlotSize;
	int primes[2];
	int diff;
};

struct MsgQueue {
	queue<GossipData> msgs;
};

class Disco: public VirtualApplication {
private:
	int packetsSent;
	simtime_t slotDuration, extendedSlot, shortenedSlot, gossipInterval;
	double xi, wi;
	cModule *node, *wchannel, *network;
	int primePair[2];
	long counter, lastAwakeSlot;
	bool isAsleep, shallGossip, stopNeighborDisc, isNoOpMode;
	int topX, topY;
	int maxH;
	//Needed just for testing, not required practically.
	map<int, Schedule> neighborSchedules;
	map<int, NodeProfile> neighborProfiles;
	map<int, RendezvousSchedule> rendezvousPerNeighbor;
	map<int, MsgQueue> msgQueues;
	int lastSeq, lastPeer;
	int forcedStopCounter;
	double stopAt, stopCounter;
	bool stopGossip;

	//Statistics
	int gSend, gReceive, gForward, gReached, gMidway, packetsTrans, packetsRecvd;
	int rendezvousCount, lastRendezvousSlotNo, rendezvousDuringND, totalHops, expectedTotalHops;
	simtime_t lastRendezvous, avgDelayInTime;
	double avgDelayInSlotNos;
	bool shutNeighborDisc;
	double lastAverage;

	double unifRandom();
protected:
	void startup();
	void finishSpecific();
	void timerFiredCallback(int);
	void handleSensorReading(SensorReadingMessage *);
	void handleNeworkControlMessage(cMessage *);
	void fromNetworkLayer(ApplicationPacket *, const char *, double, double);
	DataPacket* createDataPacket(PACKET_TYPE type, GossipData& extra, int, unsigned int seqNum);
	NeighborDiscPacket* createNeighborDiscPacket(PACKET_TYPE type, Schedule& schedule, NodeProfile& profile, bool request, unsigned int seqNum);
	RendezvousSchedule* predictFutureRendezvous(Schedule*, bool);
	const char* getAddressAsString(int);
	void initiateGossip(list<int>);
	int drawH();
	float* drawT();
	int getPeer(float, float);
	bool isWithinRange(NodeProfile* profile);
	double computeSigma(GossipData& packet);
	void transmitBeacon();
};
#endif
