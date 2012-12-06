#ifndef _DISCO_H_
#define _DISCO_H_

#include "VirtualApplication.h"
#include "DataPacket_m.h"
#include "NeighborDiscPacket_m.h"

using namespace std;

int primePairs[4][2] = {
		{23, 157},
		{29, 67},
		{31, 53},
		{37, 43}
};
int totalPrimes = 4;

enum TIMERS {
	START_OF_SLOT = 1, GO_TO_SLEEP = 2
};

enum PACKET_TYPE {
	NEIGHBOR_DISC_PACKET = 1, DATA_PACKET = 2
};

class Disco: public VirtualApplication {
private:
	int packetsSent;
	simtime_t slotDuration, gossipInterval;
	double si, wi;
	cModule *node, *wchannel, *network;
	int primePair[2];
	long counter;
	bool isAsleep;
	map<int, Schedule> neighborSchedules;

	double unifRandom();
protected:
	void startup();
	void finishSpecific();
	void timerFiredCallback(int);
	void handleSensorReading(SensorReadingMessage *);
	void handleNeworkControlMessage(cMessage *);
	void fromNetworkLayer(ApplicationPacket *, const char *, double, double);
	DataPacket* createDataPacket(PACKET_TYPE type, GossipData& extra, unsigned int seqNum);
	NeighborDiscPacket* createNeighborDiscPacket(PACKET_TYPE type, Schedule& schedule, bool request, unsigned int seqNum);
};
#endif
