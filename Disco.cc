#include "Disco.h"

Define_Module(Disco);

double Disco::unifRandom()
{
	double random = rand() / double(RAND_MAX);/*
	while(random == 0)
		random = rand() / double(RAND_MAX);*/
    return random;
}

void Disco::startup() {
	stringstream out;
	string temp;
	int startupDelay = (int) (unifRandom() * 100);
	int index = (int) (unifRandom() * totalPrimes);
	index = ( index == totalPrimes ) ? totalPrimes - 1 : index;

	slotDuration = STR_SIMTIME(par("slotDuration"));
	packetsSent = 0;
	primePair[0] = primePairs[index][0];
	primePair[1] = primePairs[index][1];
	counter = 0;

	out << startupDelay;
	temp = out.str(); temp += "ms";
	setTimer(START_OF_SLOT, STR_SIMTIME(temp.c_str()));
}

void Disco::timerFiredCallback(int type) {
	int i = 0;

	switch (type) {
	case START_OF_SLOT:
		//trace() << "Slot starts. Slot No = " << counter;
		counter++;
		setTimer(START_OF_SLOT, slotDuration);

		if(counter % primePair[0] == 0 || counter % primePair[1] == 0) {
			//Wake up and transmit
			trace() << "Up." << counter;
			toNetworkLayer(createRadioCommand(SET_STATE, TX));

			//Send node's schedule
			Schedule mySchedule;//= new Schedule();
			mySchedule.primePair[0] = primePair[0];
			mySchedule.primePair[1] = primePair[1];
			mySchedule.currentSlotNo = counter;
			mySchedule.currentTime = getClock();

			trace() << "Start beacon.";
			toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, true, packetsSent++), BROADCAST_NETWORK_ADDRESS);
			isAsleep = false;
		} else if (!isAsleep) {
			//Sleepn
			trace() << "Down." << counter;
			isAsleep = true;
			toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		}
		break;
	case GO_TO_SLEEP:
		toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		setTimer(START_OF_SLOT, slotDuration);
		break;
	}
}

NeighborDiscPacket* Disco::createNeighborDiscPacket(PACKET_TYPE type, Schedule& schedule, bool request, unsigned int seqNum) {
	NeighborDiscPacket *newPacket = new NeighborDiscPacket("Neighbor discovery packet", APPLICATION_PACKET);
	newPacket->setData(type);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setNodeSchedule(schedule);
	newPacket->setRequest(request);
	newPacket->setByteLength(sizeof(int) * 2 + sizeof(long) + sizeof(simtime_t) + sizeof(bool)); // size of extradata.
	return newPacket;
}

DataPacket* Disco::createDataPacket(PACKET_TYPE type, GossipData& extra,	unsigned int seqNum) {
	DataPacket *newPacket = new DataPacket("Data packet", APPLICATION_PACKET);
	newPacket->setData(type);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setExtraData(extra);
	newPacket->setByteLength(36); // size of extradata.
	return newPacket;
}

void Disco::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi) {
	int msgType = (int)genericPacket->getData();
	int peer = atoi(source);
	NeighborDiscPacket *rcvPacket;
	DataPacket *dataPacket;

	switch (msgType) {
		case NEIGHBOR_DISC_PACKET:
			trace() << "Discovered a neighbor! " << peer;
			rcvPacket = check_and_cast<NeighborDiscPacket*> (genericPacket);
			//Save neighbors schedule if it is being discovered for the first time.
			if(neighborSchedules.count(peer) == 0) {
				neighborSchedules[peer] = rcvPacket->getNodeSchedule();
			}

			//Respond with schedule if neighbor requested for it.
			if(rcvPacket->getRequest()) {
				Schedule mySchedule; //= new Schedule();
				mySchedule.primePair[0] = primePair[0];
				mySchedule.primePair[1] = primePair[1];
				mySchedule.currentSlotNo = counter;
				mySchedule.currentTime = getClock();

				trace() << "Respond with schedule.";
				toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, false, packetsSent++), source);
			}
			break;

		case DATA_PACKET:
			dataPacket = check_and_cast<DataPacket*> (genericPacket);
			break;
		default:
			break;
	}
}

void Disco::finishSpecific() {
	trace() << "Total neighbors = " << neighborSchedules.size();
	for(map<int, Schedule>::iterator it = neighborSchedules.begin(); it != neighborSchedules.end(); it++) {
		trace() << "Neighbor : " << it->first;
		trace() << "Schedule ";
		Schedule schedule = it->second;
		trace() << "Discovered at slot = " << schedule.currentSlotNo;
		trace() << "Prime1 " << schedule.primePair[0] << " Prime2 " << schedule.primePair[1];
		trace() << "Discovered at time = " << schedule.currentTime;
		trace() << "";
	}
}

void Disco::handleSensorReading(SensorReadingMessage * reading) {

}

void Disco::handleNeworkControlMessage(cMessage * msg) {

}
