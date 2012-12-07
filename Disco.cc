#include "Disco.h"

Define_Module(Disco);

double Disco::unifRandom()
{
	double random = rand() / double(RAND_MAX);/*
	while(random == 0)
		random = rand() / double(RAND_MAX);*/
    return random;
}

const char* Disco::getAddressAsString(int dest) {
	stringstream out;
	string neighbour;

	out << dest; neighbour = out.str();
	return neighbour.c_str();
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
	shallGossip = false;
	stopNeighborDisc = false;

	out << startupDelay;
	temp = out.str(); temp += "ms";
	setTimer(START_OF_SLOT, STR_SIMTIME(temp.c_str()));
	trace() << "Down." << counter;
	toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
}

void Disco::timerFiredCallback(int type) {
	int i = 0;
	int diffBetweenRendezvous;
	bool adjustSlotSize;
	list<int> awakePeers;

	//Set the timer asap in all the cases.
	switch (type) {
	case START_OF_SLOT:
		//trace() << "Slot starts. Slot No = " << counter;
		counter++;
		slotEdge = getClock();
		stopNeighborDisc = false;

		if(counter % primePair[0] == 0 || counter % primePair[1] == 0) {
			adjustSlotSize = false;
			//Check in any neighbor is awake, either wait for gossip msg or initiate gossip with that peer.
			for(map<int, RendezvousSchedule>::iterator it = rendezvousPerNeighbor.begin(); it != rendezvousPerNeighbor.end(); it++)
				if(it->second.slotNos.count(counter) > 0){
					diffBetweenRendezvous = it->second.slotNos[counter];
					//trace() << "Rendezvous slot. " << counter << " " << diffBetweenRendezvous;
					//trace() << "Talk to " << it->first  << " is slave? " << it->second.adjustSlotSize;
					it->second.slotNos.erase(counter);
					it->second.slotNos[counter + diffBetweenRendezvous] = diffBetweenRendezvous;
					//trace() << "Next rendezvous slot. " << counter + diffBetweenRendezvous << " " << diffBetweenRendezvous;

					if(it->second.adjustSlotSize) { //Do not touch the radio in the next slot.
						//Listen on the radio for two slots and wait for a gossip msg.
						adjustSlotSize = true;
					} else {
						//Initiate gossip with selected peers.
						//Generally there is only one but there could be more than one.
						awakePeers.push_back(it->first);
					}
				}

			if(!adjustSlotSize) {
				setTimer(START_OF_SLOT, slotDuration);
			} else {
				setTimer(NO_OPERATION, slotDuration);
			}

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
			initiateGossip(awakePeers);
		} else if (!isAsleep) {
			setTimer(START_OF_SLOT, slotDuration);
			//Sleeping
//			trace() << "Down." << counter;
			isAsleep = true;
			toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		} else {
			setTimer(START_OF_SLOT, slotDuration);
		}
		break;
	case NO_OPERATION:
		//trace() << "No operation.";
		setTimer(START_OF_SLOT, slotDuration);
		counter++;
		stopNeighborDisc = true;
		break;
	case GO_TO_SLEEP:
		toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		setTimer(START_OF_SLOT, slotDuration);
		break;
	}
}

void Disco::initiateGossip(list<int> awakePeers) {
	for(list<int>::iterator it = awakePeers.begin(); it != awakePeers.end(); it++) {
		GossipData *send = new GossipData();
		send->H = 0;
		send->xi = 2;
		send->wi = 3;
		send->targetX = 3.0;
		send->targetY = 4.0;
		trace() << "Initiate gossip with " << (*it);
		toNetworkLayer(createDataPacket(DATA_PACKET, *send, packetsSent++), getAddressAsString(*it) );
	}
}

NeighborDiscPacket* Disco::createNeighborDiscPacket(PACKET_TYPE type, Schedule& schedule, bool request, unsigned int seqNum) {
	NeighborDiscPacket *newPacket = new NeighborDiscPacket("Neighbor discovery packet", APPLICATION_PACKET);
	newPacket->setData(type);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setNodeSchedule(schedule);
	newPacket->setRequestType(request);
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
	RendezvousSchedule* predictedRedezvous;
	GossipData* data;

	switch (msgType) {
		case NEIGHBOR_DISC_PACKET:
			if(stopNeighborDisc) {
				trace() << "Neighbor disc stopped. Received packet from	" << source;
				break;
			}
			rcvPacket = check_and_cast<NeighborDiscPacket*> (genericPacket);
			//Save neighbors schedule if it is being discovered for the first time.
			if(neighborSchedules.count(peer) == 0) {
//				trace() << "Discovered a neighbor! " << peer;
				neighborSchedules[peer] = rcvPacket->getNodeSchedule();
				predictedRedezvous = predictFutureRendezvous(&(rcvPacket->getNodeSchedule()), rcvPacket->getRequestType());
				//predictedRedezvous->adjustSlotSize = rcvPacket->getRequestType();
				rendezvousPerNeighbor[peer] = *predictedRedezvous;

				//Respond with schedule if neighbor requested for it.
				if(rcvPacket->getRequestType()) {
					Schedule mySchedule; //= new Schedule();
					mySchedule.primePair[0] = primePair[0];
					mySchedule.primePair[1] = primePair[1];
					mySchedule.currentSlotNo = counter;
					mySchedule.currentTime = getClock();

					//trace() << "Respond with schedule.";
					toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, false, packetsSent++), source);
				}
			}
			break;

		case DATA_PACKET:
			dataPacket = check_and_cast<DataPacket*> (genericPacket);
			data = &(dataPacket->getExtraData());
			trace() << "Got msg from " << source << " H " << data->H;
			if( data->H < 4 ) {
				GossipData *send = new GossipData();
				send->H = data->H + 1;
				send->xi = 2;
				send->wi = 3;
				send->targetX = 3.0;
				send->targetY = 4.0;
				toNetworkLayer(createDataPacket(DATA_PACKET, *send, packetsSent++), source );
			}
			break;
		default:
			break;
	}
}

RendezvousSchedule* Disco::predictFutureRendezvous(Schedule* schedule, bool isRequest) {
	int diff = counter - schedule->currentSlotNo;
	int c0, c1, b0, b1, B0, B1, B, z, x0, x1;
	int firstRendezvous;
	int i, j;
	double remainder;
	RendezvousSchedule *nextRendezvous = new RendezvousSchedule();

	if(isRequest && diff <= 0)
		diff++;
	else if( !isRequest && diff > 0)
		diff--;

	if(diff > 0) {
		c0 = 0;
		c1 = diff;
	//	trace() << "Diff " << c1;
		nextRendezvous->adjustSlotSize = true;
	} else {
		c0 = diff * -1;
		c1 = 0;
		//trace() << "Diff " << c0;
		nextRendezvous->adjustSlotSize = false;
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
						z = B1 * x1 * c1;
						firstRendezvous = z % B;
						while(firstRendezvous <= counter) {
							firstRendezvous += B;
						}
						//Map rendezvousSlotNo to difference between rendezvous;
						nextRendezvous->slotNos[firstRendezvous] = B;
					//	trace() << firstRendezvous << "  " << B;
						break;
					}
				}
			} else {
				//solve B0x0 Equivalent To 1 (mod b0)
				for(x0 = 0; x0 < b0; x0++) {
					remainder = (B0 * x0 - 1) % b0;
					if(remainder == 0.0) {
						z = B0 * x0 * c0;
						firstRendezvous = z % B;
						firstRendezvous -= c0;
						while(firstRendezvous <= counter) {
							firstRendezvous += B;
						}
						//Map rendezvousSlotNo to difference between rendezvous;
						nextRendezvous->slotNos[firstRendezvous] = B;
					//	trace() << firstRendezvous << "  " << B;
						break;
					}
				}
			}
		}
	}

	return nextRendezvous;
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
