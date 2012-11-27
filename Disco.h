#ifndef _DISCO_H_
#define _DISCO_H_

#include "VirtualApplication.h"
#include "DiscoPacket_m.h"

using namespace std;

class Disco: public VirtualApplication {
private:
	int packetsSent;
	simtime_t neighbourCheckInterval, gossipInterval;
	double si, wi;
	cModule *node, *wchannel, *network;

protected:
	void startup();
	void finishSpecific();
	void timerFiredCallback(int);
	void handleSensorReading(SensorReadingMessage *);
	void handleNeworkControlMessage(cMessage *);
	void fromNetworkLayer(ApplicationPacket *, const char *, double, double);
	DiscoPacket* createDiscoDataPacket(double data, ExchngInfo& extra,	unsigned int seqNum);
};
#endif
