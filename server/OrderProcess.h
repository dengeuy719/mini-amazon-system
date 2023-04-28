#ifndef _ORDERPROCESS_H
#define _ORDERPROCESS_H

#include <vector>
#include <string>

#include "warehouse.h"
#include "lib/build/gen/world_amazon.pb.h"
#include "lib/build/gen/amazon_ups.pb.h"


// void pushWorldQueue(const ACommands& acommand, int seqNum);
// void pushUpsQueue(const AUCommands& aucommand, int seqNum);
void pushUpsQueue(AUCommands& aucommand);
void pushWorldQueue(ACommands& acommand);

//void parseOrder(string msg, int client_fd);
int selectWareHouse(const vector<Warehouse> & whList);
void processOrder(string msg, int orderID); 
void packOrder(int orderID, int whID);
void requestTruck(int orderID, int whID);

//updated
//amazon-world
void processPurchaseMore(const APurchaseMore & r);
void processPacked(const APacked & r);
void processLoaded(const ALoaded & r);
//amazon-ups
void processUAConnectedToWorld(const UAConnectedToWorld & r);
void processUADestinationUpdated(const UADestinationUpdated & r);
void processUATruckArrived(const UATruckArrived & r);
void processUAOrderDeparture(const UAOrderDeparture & r);
void processUAOrderDelivered(const UAOrderDelivered & r);

#endif
