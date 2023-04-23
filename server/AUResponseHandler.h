#ifndef _AURESPONSEHANDLER_H
#define _AURESPONSEHANDLER_H

#include <vector>

#include "OrderProcess.h"
#include "server.h"
#include "gpbCommunication.h"

using namespace std;

class AUResponseHandler {
 private:
  vector<UAConnectedToWorld> connectedtoworld_vec;
  vector<UADestinationUpdated> destinationupdated_vec;
  vector<UATruckArrived> truckarrived_vec;
  vector<UAOrderDeparture> orderdeparture_vec;
  vector<UAOrderDelivered> orderdeliveredv_vec;
  vector<int> seqNums;

 public:
  AUResponseHandler(const UACommands & r);
  ~AUResponseHandler() {}
  void handle();

 private:
  bool checkExecutedAndRecordIt(int seqNum);
};

#endif