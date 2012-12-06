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
	int startupDelay = (int) (unifRandom() * 1000);
	int index = (int) (unifRandom() * totalPrimes);
	index = ( index == totalPrimes ) ? totalPrimes - 1 : index;

	slotDuration = STR_SIMTIME(par("slotDuration"));
	packetsSent = 0;
	primePair[0] = primePairs[index][0];
	primePair[1] = primePairs[index][1];
	counter = -1;

	isAsleep = false;
	isSlave = false;
	shallGossip = false;

	out << startupDelay;
	temp = out.str(); temp += "ms";
	setTimer(START_OF_SLOT, STR_SIMTIME(temp.c_str()));
	trace() << "Down." << counter;
	toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
}

void Disco::timerFiredCallback(int type) {
	int i = 0;
	int diffBetweenRendezvous;

	switch (type) {
	case START_OF_SLOT:
		//trace() << "Slot starts. Slot No = " << counter;
		counter++;
		slotEdge = getClock();
		if(nextRendezvous.count(counter) == 0) {
			setTimer(START_OF_SLOT, slotDuration);
		} else {
			trace() << "Rendezvous slot. " << counter << " " << diffBetweenRendezvous;
			diffBetweenRendezvous = nextRendezvous[counter];
			nextRendezvous.erase(counter);
			nextRendezvous[counter + diffBetweenRendezvous] = diffBetweenRendezvous;
			trace() << "Next rendezvous slot. " << counter + diffBetweenRendezvous << " " << diffBetweenRendezvous;

			if(isSlave) { //Do not touch the radio in the next slot.
				setTimer(NO_OPERATION, slotDuration);
			} else {
				setTimer(START_OF_SLOT, slotDuration);
			}
		}

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

			toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, true, packetsSent++), BROADCAST_NETWORK_ADDRESS);
			isAsleep = false;
		} else if (!isAsleep) {
			//Sleeping
			trace() << "Down." << counter;
			isAsleep = true;
			toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		}
		break;
	case NO_OPERATION:
		//trace() << "Slot starts. Slot No = " << counter;
		counter++;
		setTimer(START_OF_SLOT, slotDuration);
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
			rcvPacket = check_and_cast<NeighborDiscPacket*> (genericPacket);
			//Save neighbors schedule if it is being discovered for the first time.
			if(neighborSchedules.count(peer) == 0) {
				trace() << "Discovered a neighbor! " << peer;
				neighborSchedules[peer] = rcvPacket->getNodeSchedule();
				predictFutureRendezvous(&(rcvPacket->getNodeSchedule()));
				if(rcvPacket->getRequest()) {
					isSlave  = true;
				}
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

void Disco::predictFutureRendezvous(Schedule* schedule) {
	int diff = counter - schedule->currentSlotNo;
	int c0, c1, b0, b1, B0, B1, B, z, x0, x1;
	int firstRendezvous;
	int i, j;
	double remainder;

	if(diff > 0) {
		c0 = 0;
		c1 = diff;
	} else {
		c0 = diff * -1;
		c1 = 0;
	}

	for(i = 0; i < 2; i++) {
		for(j = 0; j < 2; j++) {
			B1 = b0 = primePair[i];
			B0 = b1 = schedule->primePair[j];
			B = b0 * b1;

			if(c0 == 0) {
				//solve B1x1 Equivalent To 1 (mod b1)
				for(x1 = 0; x1 < b1; x1++) {
					remainder =  (B1 * x1 - 1) % b1;
					if(remainder == 0.0) {
						break;
					}
				}
				z = B1 * x1 * c1;
				firstRendezvous = z % B;
				while(firstRendezvous <= counter) {
					firstRendezvous += B;
				}
				//Map rendezvousSlotNo to difference between rendezvous;
				nextRendezvous[firstRendezvous] = B;
				trace() << firstRendezvous << "  " << B;
			} else {
				//solve B0x0 Equivalent To 1 (mod b0)
				for(x0 = 0; x0 < b0; x0++) {
					remainder = (B0 * x0 - 1) % b0;
					if(remainder == 0.0) {
						break;
					}
				}
				z = B0 * x0 * c0;
				firstRendezvous = z % B;
				firstRendezvous -= c0;
				while(firstRendezvous <= counter) {
					firstRendezvous += B;
				}
				//Map rendezvousSlotNo to difference between rendezvous;
				nextRendezvous[firstRendezvous] = B;
				trace() << firstRendezvous << "  " << B;
			}
		}
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
