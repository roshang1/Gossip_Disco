#include "Disco.h"

Define_Module(Disco);

double Disco::unifRandom()
{
	double random = rand() / double(RAND_MAX);
	while(random == 0)
		random = rand() / double(RAND_MAX);
    return random;
}

const char* Disco::getAddressAsString(int dest) {
	stringstream out;
	string neighbour;

	out << dest; neighbour = out.str();
	return neighbour.c_str();
}

int Disco::drawH() {
	int H = unifRandom() * (maxH -2) + 2;
	//trace() << " H " << H;
	return H;
}

double* Disco::drawT() {
	double x,y;
	double myX = mobilityModule->getLocation().x;
	double myY = mobilityModule->getLocation().y;
	double* T = new double[2];
	int range = pow ( (int)par("nodeSeparation"), 2);
	double dist;

	while(true) {
		x = unifRandom() * topX;
		y = unifRandom() * topY;
		dist = pow((x - myX), 2) + pow((y - myY), 2);
		if( dist > range)
			break;
	}
	T[0] = x;
	T[1] = y;
	//trace() << "x " << x << " y " << y;
	return T;
}

void Disco::startup() {
	stringstream out;
	string temp;
	int startupDelay = (int) (unifRandom() * 5000);
	int index = (int) (unifRandom() * totalPrimes);
	index = ( index == totalPrimes ) ? totalPrimes - 1 : index;

	slotDuration = STR_SIMTIME(par("slotDuration"));
	temp = "2ms";
	extendedSlot = slotDuration + STR_SIMTIME(temp.c_str());
	shortenedSlot = slotDuration - STR_SIMTIME(temp.c_str());
	gossipInterval = STR_SIMTIME(par("gossipInterval"));
	packetsSent = 0;
	primePair[0] = primePairs[index][0];
	primePair[1] = primePairs[index][1];
	counter = 0;

	cModule* node = getParentModule();
	cModule* network = node->getParentModule();
	topX = network->par("field_x");
	topY = network->par("field_y");
	int nodeSeparation = par("nodeSeparation");
	maxH = (int)( (topX + topY) / nodeSeparation );

	xi = unifRandom() * 100000;
	//xi = (self == 0) ? 1 : 0;
	wi = 1.0;
	trace() << "Initial values: xi " << xi  << " " << wi << " " << maxH;
	trace() << "Initial delay " << startupDelay << " prime1 " << primePair[0] << " prime2 " << primePair[1];
	trace() << "Extended slot: " << extendedSlot << " shortened " << shortenedSlot;

	isAsleep = false;
	shallGossip = false;

	lastSeq = lastPeer = -1;

	out << startupDelay; temp = out.str(); temp += "ms";
	setTimer(START_OF_SLOT, STR_SIMTIME(temp.c_str()));

	temp = "200s";
	setTimer(GENERATE_SAMPLE, STR_SIMTIME(temp.c_str()));
	setTimer(TEST, STR_SIMTIME(temp.c_str()));

	//trace() << "Down." << counter;
	toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));

	//Statistics
	gSend = gReceive = gForward = 0;
	packetsTrans = packetsRecvd = 0;
	rendezvousCount = 0;
	lastRendezvousSlotNo = avgDelayInSlotNos = rendezvousDuringND = 0;
	lastRendezvous = avgDelayInTime = 0;
	shutNeighborDisc = false;
	declareOutput("Stats");
}

void Disco::timerFiredCallback(int type) {
	int i = 0;
	int diffBetweenRendezvous;
	bool adjustSlotSize;
	list<int> awakePeers;
	string temp;

	//Stat
	bool isNeighborAwake;
	double sum = 0;
	simtime_t totalTime = 0;

	//Set the timer asap in all the cases.
	switch (type) {
	case START_OF_SLOT:
		if(counter == 0)
			trace() << "Slot starts. Slot No = " << counter;
		counter++;

		if(counter % primePair[0] == 0 || counter % primePair[1] == 0) {
			isNeighborAwake = adjustSlotSize = false;
			//Check in any neighbor is awake, either wait for gossip msg or initiate gossip with that peer.
			for(map<int, RendezvousSchedule>::iterator it = rendezvousPerNeighbor.begin(); it != rendezvousPerNeighbor.end(); it++) {
				if(it->second.slotNos.count(counter) > 0) {
					diffBetweenRendezvous = it->second.slotNos[counter];
					trace() << "Rendezvous slot. " << counter << " " << diffBetweenRendezvous;
					trace() << "Talk to " << it->first  << " is slave? " << it->second.adjustSlotSize;
					it->second.slotNos.erase(counter);
					it->second.slotNos[counter + diffBetweenRendezvous] = diffBetweenRendezvous;
					trace() << "Next rendezvous slot. " << counter + diffBetweenRendezvous << " " << diffBetweenRendezvous;

					if(it->second.adjustSlotSize) { //Do not touch the radio in the next slot.
						//Listen on the radio for two slots and wait for a gossip msg.
						adjustSlotSize = true;
					} else {
						//Initiate gossip with selected peers.
						//Generally there is only one but there could be more than one.
						awakePeers.push_back(it->first);
					}
					isNeighborAwake = true;
				}
			}

			if(!adjustSlotSize) {
				setTimer(START_OF_SLOT, slotDuration);
			} else {
				setTimer(NO_OPERATION, slotDuration);
			}

			//Wake up and transmit
			trace() << "Up." << counter;
			//To Do: Different behaviour of radio at startup while in neighbor disc or gossip phase.
			if (unifRandom() > 0.5 ) {
				toNetworkLayer(createRadioCommand(SET_STATE, TX));
				transmitBeacon();
			} else {
				toNetworkLayer(createRadioCommand(SET_STATE, RX));
				//temp = "3ms";
				//setTimer(TRANSMIT_BEACON, STR_SIMTIME(temp.c_str()));
			}

			isAsleep = false;
			if(isNeighborAwake) {
				//(x + y + z)/3 = (2((x+y)/2) + z)/3;
				sum = (rendezvousCount * avgDelayInSlotNos) + (counter - lastRendezvousSlotNo);
				totalTime = (rendezvousCount * avgDelayInTime) + (getClock() - lastRendezvous);
				rendezvousCount++;
				avgDelayInSlotNos = sum / rendezvousCount;
				avgDelayInTime = totalTime / rendezvousCount;
				lastRendezvousSlotNo = counter;
				lastRendezvous = getClock();

				initiateGossip(awakePeers);
			}
		} else if (!isAsleep) {
			/*temp = "2ms";
			setTimer(GO_TO_SLEEP, STR_SIMTIME(temp.c_str()));*/
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
		counter++;
		if(counter % primePair[0] == 0 || counter % primePair[1] == 0) {
			isNeighborAwake = adjustSlotSize = false;
			//Check in any neighbor is awake, either wait for gossip msg or initiate gossip with that peer.
			for(map<int, RendezvousSchedule>::iterator it = rendezvousPerNeighbor.begin(); it != rendezvousPerNeighbor.end(); it++) {
				if(it->second.slotNos.count(counter) > 0) {
					diffBetweenRendezvous = it->second.slotNos[counter];
					trace() << "Rendezvous slot. " << counter << " " << diffBetweenRendezvous;
					trace() << "Talk to " << it->first  << " is slave? " << it->second.adjustSlotSize;
					it->second.slotNos.erase(counter);
					it->second.slotNos[counter + diffBetweenRendezvous] = diffBetweenRendezvous;
					trace() << "Next rendezvous slot. " << counter + diffBetweenRendezvous << " " << diffBetweenRendezvous;

					if(it->second.adjustSlotSize) { //Do not touch the radio in the next slot.
						//Listen on the radio for two slots and wait for a gossip msg.
						adjustSlotSize = true;
					} else {
						//Initiate gossip with selected peers.
						//Generally there is only one but there could be more than one.
						awakePeers.push_back(it->first);
					}
					isNeighborAwake = true;
				}
			}

			if(!adjustSlotSize) {
				setTimer(START_OF_SLOT, slotDuration);
			} else {
				setTimer(NO_OPERATION, slotDuration);
			}

			//Wake up and transmit
			trace() << "Up." << counter;

			isAsleep = false;
			if(isNeighborAwake) {
				//(x + y + z)/3 = (2((x+y)/2) + z)/3;
				sum = (rendezvousCount * avgDelayInSlotNos) + (counter - lastRendezvousSlotNo);
				totalTime = (rendezvousCount * avgDelayInTime) + (getClock() - lastRendezvous);
				rendezvousCount++;
				avgDelayInSlotNos = sum / rendezvousCount;
				avgDelayInTime = totalTime / rendezvousCount;
				lastRendezvousSlotNo = counter;
				lastRendezvous = getClock();

				initiateGossip(awakePeers);
			}
		} else {
			setTimer(START_OF_SLOT, slotDuration);
		}
		break;
	case TRANSMIT_BEACON:
		transmitBeacon();
		break;
	case GO_TO_SLEEP:
		toNetworkLayer(createRadioCommand(SET_STATE, SLEEP));
		setTimer(START_OF_SLOT, shortenedSlot);
		break;
	case TEST:
		shutNeighborDisc = true;
		rendezvousDuringND = rendezvousCount;
		break;
	case GENERATE_SAMPLE:
		double* T = drawT();
		int H = drawH();
		int dest = getPeer(T[0], T[1]);

		if (dest != -1) {
			//trace() << "Sending to " << dest << " for location: " << T[0]  <<  " " << T[1] ;
			GossipData send;

			send.H = H - 1;
			send.targetX = T[0];
			send.targetY = T[1];
			send.xi = ( ( (double) send.H )/ H ) * xi;
			send.wi = ( ( (double) send.H )/ H ) * wi;
			//trace() << "Before  " << xi << " " << wi << " H " << H << " Ratio " << xi/wi;
			//trace() << "Sending " << send.xi << " " << send.wi << " to "<< dest;
			xi = xi / H; wi = wi / H;
			//trace() << "After  " << xi << " " << wi << " Ratio " << xi/wi;
			gSend++;
			msgQueues[dest].msgs.push(send);
		}
		setTimer(GENERATE_SAMPLE, gossipInterval);
		break;
	}
}

void Disco::transmitBeacon() {
	if(shutNeighborDisc)
		return;

	Schedule mySchedule;//= new Schedule();
	mySchedule.primePair[0] = primePair[0];
	mySchedule.primePair[1] = primePair[1];
	mySchedule.currentSlotNo = counter;

	NodeProfile myProfile;
	myProfile.x = mobilityModule->getLocation().x;
	myProfile.y = mobilityModule->getLocation().y;

	toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, myProfile, true, packetsSent++), BROADCAST_NETWORK_ADDRESS);
}

void Disco::initiateGossip(list<int> awakePeers) {

/*	if(awakePeers.size() == 0) {
		////trace() << "Did adjustment at " << rendezvousCount;
		return;
	}*/

/*	////trace() << "Initiator: Rendezvous no " << rendezvousCount;
	MsgQueue* aQueue;

	////trace() << "Initiator: Before transmission.";
	for(map<int, MsgQueue>::iterator it = msgQueues.begin(); it != msgQueues.end(); it++) {
		aQueue = &(it->second);
		////trace() << "Initiator: Queue for " << it->first << " has " << aQueue->msgs.size() << " msgs.";
	}*/

	for(list<int>::iterator it = awakePeers.begin(); it != awakePeers.end(); it++) {
		////trace() << "Initiator: Try gossip with " << (*it);
		if( !msgQueues[*it].msgs.empty() ) {
			//gSend++;
			packetsTrans++;
			toNetworkLayer(createDataPacket(DATA_PACKET, msgQueues[*it].msgs.front(), 0, packetsSent++), getAddressAsString(*it) );
			msgQueues[*it].msgs.pop();
/*			if( !msgQueues[*it].msgs.empty() ) {
				gSend++;
				toNetworkLayer(createDataPacket(DATA_PACKET, msgQueues[*it].msgs.front(), 0, packetsSent++), getAddressAsString(*it) );
				msgQueues[*it].msgs.pop();
			}*/
		} else {
			//Send dummy packet
			GossipData send;
			send.H = 0;
			send.xi = 2;
			send.wi = 3;
			send.targetX = 3.0;
			send.targetY = 4.0;
			packetsTrans++;
			toNetworkLayer(createDataPacket(DATA_PACKET, send, 0, packetsSent++), getAddressAsString(*it) );
		}
	}

/*	////trace() << "Initiator: After transmission.";
	for(map<int, MsgQueue>::iterator it = msgQueues.begin(); it != msgQueues.end(); it++) {
		aQueue = &(it->second);
		////trace() << "Initiator: Queue for " << it->first << " has " << aQueue->msgs.size() << " msgs.";
	}*/
}

int Disco::getPeer(double targetX, double targetY) {
	double x = mobilityModule->getLocation().x;
	double y = mobilityModule->getLocation().y;
	double minDist = pow((x - targetX), 2) + pow((y - targetY), 2);
	vector<int> peersCloseToT;
	map<int, NodeProfile>::iterator it;
	double dist;

	for (it = neighborProfiles.begin(); it != neighborProfiles.end(); ++it)
	{
		dist = pow((it->second.x - targetX), 2) + pow((it->second.y - targetY), 2);
		if(dist < minDist)
			peersCloseToT.push_back(it->first);
	}

	if(peersCloseToT.size() == 0)
		return -1;

	int idx = unifRandom() * (peersCloseToT.size() - 1);
	return peersCloseToT.at(idx);
}

bool Disco::isWithinRange(NodeProfile* profile) {
	double x = mobilityModule->getLocation().x;
	double y = mobilityModule->getLocation().y;
	double dist = pow((x - profile->x), 2) + pow((y - profile->y), 2);
	double nodeSeparation = (double) par("nodeSeparation");
	nodeSeparation += 2.0;
	nodeSeparation = pow(nodeSeparation, 2);
	return dist <= nodeSeparation;
}

void Disco::fromNetworkLayer(ApplicationPacket * genericPacket, const char *source, double rssi, double lqi) {
	int msgType = (int)genericPacket->getData();
	int peer = atoi(source);
	NeighborDiscPacket *rcvPacket;
	DataPacket *dataPacket;
	RendezvousSchedule* predictedRedezvous;
	GossipData* data;
	int seq = genericPacket->getSequenceNumber();

	if(seq == lastSeq && peer == lastPeer) {
		trace() << "Duplicate hai duplicate!";
		return;
	}
	lastSeq = seq;
	lastPeer = peer;

	switch (msgType) {
		case NEIGHBOR_DISC_PACKET:
			rcvPacket = check_and_cast<NeighborDiscPacket*> (genericPacket);
			//Save neighbors schedule if it is being discovered for the first time.
			if(neighborProfiles.count(peer) == 0) {
				if( !isWithinRange(& (rcvPacket->getNodeProfile()) ) ) {
					trace() << "Not a neighbor " << source;
					break;
				}
				trace() << "Discovered a neighbor! " << peer;
				neighborSchedules[peer] = rcvPacket->getNodeSchedule();
				neighborSchedules[peer].discAt = getClock();
				neighborProfiles[peer] = rcvPacket->getNodeProfile();
				predictedRedezvous = predictFutureRendezvous(&(rcvPacket->getNodeSchedule()), rcvPacket->getRequestType());
				//predictedRedezvous->adjustSlotSize = rcvPacket->getRequestType();
				rendezvousPerNeighbor[peer] = *predictedRedezvous;
				msgQueues[peer] = *(new MsgQueue());
			}
			//Respond with schedule if neighbor requested for it.
			if(rcvPacket->getRequestType()) {
				Schedule mySchedule; //= new Schedule();
				mySchedule.primePair[0] = primePair[0];
				mySchedule.primePair[1] = primePair[1];
				mySchedule.currentSlotNo = counter;

				NodeProfile myProfile;
				myProfile.x = mobilityModule->getLocation().x;
				myProfile.y = mobilityModule->getLocation().y;

				trace() << "Respond with schedule to " << source;
				toNetworkLayer(createNeighborDiscPacket(NEIGHBOR_DISC_PACKET, mySchedule, myProfile, false, packetsSent++), source);
			}
			break;

		case DATA_PACKET:
			dataPacket = check_and_cast<DataPacket*> (genericPacket);
			data = &(dataPacket->getExtraData());
			packetsRecvd++;
			if( data->H != 0 ) {
				gReceive++;
				//trace() << "Got msg from " << source << " H " << data->H;
				//trace() << "Received " << data->xi << " " << data->wi << " H " << data->H << " from " << peer;
				//trace() << "Before  " << xi << " " << wi << " Ratio " << xi/wi;
				//trace() << "Keep " << (data->xi / data->H) << " " << (data->wi / data->H);
				xi += (data->xi / data->H);
				wi += (data->wi / data->H);
				//trace() << "After  " << xi << " " << wi << " Ratio " << xi/wi;
				if(data->H != 1) {
					int dest = getPeer(data->targetX, data->targetY);
					if (dest != -1) {
						//trace() << "Forwarding to " << dest << " for location: " << data->targetX  <<  " " << data->targetY;
						GossipData send;

						send.H = data->H - 1;
						send.targetX = data->targetX;
						send.targetY = data->targetY;
						send.xi = ( ( (double) send.H )/ data->H ) *  data->xi;
						send.wi = ( ( (double) send.H )/ data->H ) *  data->wi;

						//trace() << "Forward " << send.xi << " " << send.wi << " to "<< dest;
						gForward++;
						msgQueues[dest].msgs.push(send);
					} else {
						//trace() << "Couldn't send. Save everything that was sent.";
						//trace() << "Before  " << xi << " " << wi << " Ratio " << xi/wi;
						xi += ( ( (double) data->H - 1 )/ data->H ) *  data->xi;
						wi += ( ( (double) data->H - 1 )/ data->H ) *  data->wi;
						//trace() << "After  " << xi << " " << wi << " Ratio " << xi/wi;
					}
				}
			}

			if( dataPacket->getExchangeNo() < 3 ) {
/*				////trace() << "Receiver: Rendezvous no " << rendezvousCount;
				MsgQueue* aQueue;

				////trace() << "Receiver: Before transmission.";
				for(map<int, MsgQueue>::iterator it = msgQueues.begin(); it != msgQueues.end(); it++) {
					aQueue = &(it->second);
					////trace() << "Receiver: Queue for " << it->first << " has " << aQueue->msgs.size() << " msgs.";
				}*/

				////trace() << "Receiver: Try gossip with " << peer;
				if ( !msgQueues[peer].msgs.empty() ) {
				//	gSend++;
					packetsTrans++;
					toNetworkLayer(createDataPacket(DATA_PACKET, msgQueues[peer].msgs.front(), dataPacket->getExchangeNo() + 1, packetsSent++), source);
					msgQueues[peer].msgs.pop();
	/*				if ( !msgQueues[peer].msgs.empty() ) {
						gSend++;
						toNetworkLayer(createDataPacket(DATA_PACKET, msgQueues[peer].msgs.front(), dataPacket->getExchangeNo() + 1, packetsSent++), source);
						msgQueues[peer].msgs.pop();
					}*/
				} else {
					GossipData send;
					send.H = 0;
					send.xi = 2;
					send.wi = 3;
					send.targetX = 3.0;
					send.targetY = 4.0;
					packetsTrans++;
					toNetworkLayer(createDataPacket(DATA_PACKET, send, dataPacket->getExchangeNo() + 1, packetsSent++), source );
				}

/*				////trace() << "Receiver: After transmission.";
				for(map<int, MsgQueue>::iterator it = msgQueues.begin(); it != msgQueues.end(); it++) {
					aQueue = &(it->second);
					////trace() << "Receiver: Queue for " << it->first << " has " << aQueue->msgs.size() << " msgs.";
				}*/
			}
			break;
		default:
			break;
	}
}

RendezvousSchedule* Disco::predictFutureRendezvous(Schedule* schedule, bool isRequest) {
	int mySlotNo = counter;
	int diff, c0, c1, b0, b1, B0, B1, B, z, x0, x1;
	int firstRendezvous;
	int i, j;
	double remainder;
	RendezvousSchedule *nextRendezvous = new RendezvousSchedule();

/*	if(counter % primePair[0] != 0 && counter % primePair[1] != 0)
		mySlotNo--; //Msg was received at the end of the awake slot and counter was incremented after that, fix it.
	diff = mySlotNo - schedule->currentSlotNo;*/

	diff = counter - schedule->currentSlotNo;

	//Align the nodes properly.
	if( isRequest && diff <= 0 )
		diff++;
	else if( !isRequest && diff > 0 )
		diff--;

	if(diff > 0) {
		trace() << "Ctr " << counter << " mySNo " << mySlotNo << " N's SNo " << schedule->currentSlotNo << " Diff " << diff;
		c0 = 0;
		c1 = diff;
		nextRendezvous->adjustSlotSize = true;
	} else {
		trace() << "Slot no " << counter << " Neighbor's slot no " << schedule->currentSlotNo << "Diff " << diff;
		c0 = diff * -1;
		c1 = 0;
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
						trace() << firstRendezvous << "  " << B;
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
						trace() << firstRendezvous << "  " << B;
						break;
					}
				}
			}
		}
	}

	return nextRendezvous;
}

NeighborDiscPacket* Disco::createNeighborDiscPacket(PACKET_TYPE type, Schedule& schedule, NodeProfile& profile, bool request, unsigned int seqNum) {
	NeighborDiscPacket *newPacket = new NeighborDiscPacket("Neighbor discovery packet", APPLICATION_PACKET);
	newPacket->setData(type);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setNodeSchedule(schedule);
	newPacket->setNodeProfile(profile);
	newPacket->setRequestType(request);
	newPacket->setByteLength(sizeof(Schedule) + sizeof(NodeProfile) + sizeof(bool)); // size of extradata.
	return newPacket;
}

DataPacket* Disco::createDataPacket(PACKET_TYPE type, GossipData& extra, int exchngNo, unsigned int seqNum) {
	DataPacket *newPacket = new DataPacket("Data packet", APPLICATION_PACKET);
	newPacket->setData(type);
	newPacket->setSequenceNumber(seqNum);
	newPacket->setExtraData(extra);
	newPacket->setExchangeNo(exchngNo);
	newPacket->setByteLength(sizeof(GossipData) + sizeof(int)); // size of extradata.
	return newPacket;
}

void Disco::finishSpecific() {
	MsgQueue *aQueue;
	int totalQueueSize = 0;
	trace() << "Total neighbors = " << neighborSchedules.size();
	for(map<int, Schedule>::iterator it = neighborSchedules.begin(); it != neighborSchedules.end(); it++) {
		trace() << "Neighbor : " << it->first;
		trace() << "Schedule ";
		Schedule schedule = it->second;
		trace() << "Discovered at slot = " << schedule.currentSlotNo << " time = " << schedule.discAt ;
		trace() << "Prime1 " << schedule.primePair[0] << " Prime2 " << schedule.primePair[1];
		trace() << "";
	}

	for(map<int, MsgQueue>::iterator it = msgQueues.begin(); it != msgQueues.end(); it++) {
		aQueue = &(it->second);
		trace() << "QueueSize to " << it->first << " = " << aQueue->msgs.size();
		while( !aQueue->msgs.empty() ) {
			xi += aQueue->msgs.front().xi;
			wi += aQueue->msgs.front().wi;
			aQueue->msgs.pop();
			totalQueueSize++;
		}
	}
	trace() << "Final Average " << xi/wi << " " << xi << " " << wi;

	collectOutput("Stats", "Sent ", gSend);
	collectOutput("Stats", "Received ", gReceive);
	collectOutput("Stats", "Forwarded ", gForward);
	collectOutput("Stats", "Rendezvous ", rendezvousCount - rendezvousDuringND);
	collectOutput("Stats", "AvgDelayInSlotNo ", avgDelayInSlotNos);
	collectOutput("Stats", "AvgDelayInTime ", SIMTIME_DBL(avgDelayInTime));
	collectOutput("Stats", "QueueSize ", totalQueueSize);
	collectOutput("Stats", "PacketsTrans ", packetsTrans);
	collectOutput("Stats", "PacketsRecvd ", packetsRecvd);
}

void Disco::handleSensorReading(SensorReadingMessage * reading) {

}

void Disco::handleNeworkControlMessage(cMessage * msg) {

}
