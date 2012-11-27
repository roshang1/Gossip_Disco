#include "Disco.h"

Define_Module(Disco);

void Disco::startup() {
	packetsSent = 0;
	string temp;

	temp = "100ms";
	setTimer(1, STR_SIMTIME(temp.c_str()));
}

void Disco::timerFiredCallback(int type) {
	string temp;
	int i = 0;
	int sum = 0;

	switch (type) {
	case 1:
		trace() << "Up";
		toNetworkLayer(createRadioCommand(SET_STATE, RX));
		temp = "10ms";
		setTimer(2, STR_SIMTIME(temp.c_str()));
		for(i = 0; i < 2; i++) {
			ExchngInfo *send = new ExchngInfo();

			send->H = 1;
			send->xi = 2;
			send->wi = 3;
			send->targetX = 3.0;
			send->targetY = 4.0;
			toNetworkLayer(createDiscoDataPacket(2.0, *send, packetsSent++), BROADCAST_NETWORK_ADDRESS);
		}
		break;
	case 2:
		trace() << "Down";
		toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		temp = "990ms";
		setTimer(1, STR_SIMTIME(temp.c_str()));
		break;
	}

}

DiscoPacket* Disco::createDiscoDataPacket(double data, ExchngInfo& extra,	unsigned int seqNum) {
	DiscoPacket *newPacket = new DiscoPacket("Disco Msg", APPLICATION_PACKET);
	newPacket->setData(data);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setExtraData(extra);
	newPacket->setByteLength(36); // size of extradata.
	return newPacket;
}

void Disco::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi) {
	//trace() << "Received Packet! Neighbor Discovered!";
}

void Disco::finishSpecific() {

}

void Disco::handleSensorReading(SensorReadingMessage * reading) {

}

void Disco::handleNeworkControlMessage(cMessage * msg) {

}
