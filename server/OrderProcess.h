#ifndef _ORDER_PROCESS_H
#define _ORDER_PROCESS_H
#include "server.h"
//#include "sql_function.h"
#include "warehouse.h"


void parseOrder(string msg, int client_fd);
int selectWareHouse(const vector<Warehouse> & whList, const Order & order);
void processOrder(const Order& order); 

void processPurchaseMore(APurchaseMore r);
void processPacked(APacked r);
void processLoaded(ALoaded r);


void pushWorldQueue(const ACommands& acommand, int seqNum);
void pushUpsQueue(const AUCommands& aucommand, int seqNum);
void packOrder(Order order);
void callATruck(Order order);

//updated
//amazon-world

//amazon-ups
void processUAConnectedToWorld();
void processUADestinationUpdated();
void processUATruckArrived();
void processUAOrderDeparture();
void processUAOrderDelivered();

#endif
