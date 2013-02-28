#ifndef _DISCO_H_
#define _DISCO_H_

#include "VirtualApplication.h"
#include "DataPacket_m.h"
#include "NeighborDiscPacket_m.h"

#include <map>
#include <queue>
#include <cmath>

using namespace std;

int primePairs[4][2] = {
		{23, 157},
		{29, 67},
		{31, 53},
		{37, 43}
};
int totalPrimes = 4;

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
	int stopAfter;

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
